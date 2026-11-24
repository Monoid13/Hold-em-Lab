// Stats dashboard: results, decision quality, leak finder, positional stats and opponent reads.
import { esc, pct, fmtBB, fmt, gradeBadge } from './dom.js';
import { GRADES, LEAK_TAGS } from '../coach/coach.js';
import { STAT_DEFS } from '../bots/stats.js';
import { PROFILES } from '../bots/profiles.js';
import { HERO_ID } from '../app/controller.js';
import { DRILLS } from '../train/drills.js';

const TARGETS = {
  vpip: [0.2, 0.3], pfr: [0.15, 0.25], threeBet: [0.06, 0.11], foldTo3bet: [0.4, 0.6], steal: [0.3, 0.5],
  cbet: [0.5, 0.75], foldToCbet: [0.35, 0.55], foldToBet: [0.35, 0.55], wtsd: [0.24, 0.32], wsd: [0.48, 0.6],
};
const STAT_ORDER = ['vpip', 'pfr', 'threeBet', 'foldTo3bet', 'steal', 'cbet', 'foldToCbet', 'foldToBet', 'wtsd', 'wsd'];

export function readOpponent(stats, id) {
  const n = stats.get(id).hands;
  if (n < 20) return { label: 'Unknown', tip: `Only ${n} hands observed — keep watching.` };
  const v = stats.raw(id, 'vpip').pct ?? 0;
  const p = stats.raw(id, 'pfr').pct ?? 0;
  const af = stats.af(id) ?? 0;
  const fvb = stats.raw(id, 'foldToBet').pct;
  if (v > 0.42 && p >= 0.27) return { label: 'Maniac', tip: 'Call down lighter and let them bluff; avoid bluffing them.' };
  if (v > 0.38 && p < 0.16) return { label: 'Calling station', tip: 'Value bet thinly and size up; almost never bluff.' };
  if (v < 0.16) return { label: 'Nit', tip: 'Steal their blinds; fold to their big bets and 3-bets.' };
  if (fvb !== null && fvb > 0.58) return { label: 'Folds too much', tip: 'Bluff more often, especially on turns and rivers.' };
  if (v >= 0.27 && p >= 0.2) return { label: 'LAG', tip: 'Widen your calling and 3-bet ranges; trap with strong hands.' };
  if (p / Math.max(v, 0.01) >= 0.65) return { label: 'TAG', tip: 'Respect their raises; attack their c-bets when they give up.' };
  if (af < 1.3) return { label: 'Passive', tip: 'Bet for value; their raises are very strong.' };
  return { label: 'Recreational', tip: 'Play solid value-heavy poker against them.' };
}

export class StatsView {
  constructor(root, { ctrl, progress, onReset }) {
    this.root = root;
    this.ctrl = ctrl;
    this.progress = progress;
    this.onReset = onReset;
    root.addEventListener('click', (e) => {
      const b = e.target.closest('[data-st]');
      if (!b) return;
      const a = b.dataset.st;
      if (a === 'reset' && confirm('Reset all your results, decision history and HUD stats? Drill scores are kept.')) {
        this.progress.reset();
        this.ctrl.resetOpponentKnowledge();
        this.onReset?.();
        this.render();
      }
      if (a === 'reset-opp' && confirm('Reset all observed opponent stats?')) {
        for (const id of Object.keys(this.ctrl.stats.players)) if (id !== HERO_ID) this.ctrl.stats.reset(id);
        this.render();
      }
    });
  }

  render() {
    const pr = this.progress;
    const T = pr.totals();
    const st = this.ctrl.stats;
    const hands = pr.data.hands;
    const errs = T.grades.inaccuracy + T.grades.mistake + T.grades.blunder;

    const tiles = [
      ['Hands played', fmt(T.hands), ''],
      ['Net won', fmtBB(T.netBB), T.netBB > 0 ? 'gain' : T.netBB < 0 ? 'loss' : ''],
      ['Win rate', `${T.bb100 >= 0 ? '+' : '−'}${Math.abs(T.bb100).toFixed(1)} bb/100`, T.bb100 > 0 ? 'gain' : T.bb100 < 0 ? 'loss' : ''],
      ['Decision score', T.accuracy === null ? '–' : pct(T.accuracy), ''],
      ['Errors', `${fmt(errs)} of ${fmt(T.decisions)}`, ''],
      ['EV lost to errors', `${pr.data.evLossBB.toFixed(1)} bb`, ''],
    ]
      .map(([l, v, c]) => `<div class="tile"><div class="tile-label">${l}</div><div class="tile-value ${c}">${v}</div></div>`)
      .join('');

    // Hero HUD stats vs targets
    const statRows = STAT_ORDER.map((k) => {
      const raw = st.raw(HERO_ID, k);
      const [lo, hi] = TARGETS[k];
      let verdict = '<span class="muted">—</span>';
      if (raw.n >= 15 && raw.pct !== null) verdict = raw.pct < lo ? '<span class="lean low">▼ low</span>' : raw.pct > hi ? '<span class="lean high">▲ high</span>' : '<span class="lean ok">✓ ok</span>';
      return `<tr><td title="${esc(STAT_DEFS[k].desc)}">${STAT_DEFS[k].label}</td><td class="num">${raw.pct === null ? '–' : pct(raw.pct)}</td><td class="num muted">${raw.n}</td><td class="num muted">${Math.round(lo * 100)}–${Math.round(hi * 100)}%</td><td>${verdict}</td></tr>`;
    }).join('');
    const af = st.af(HERO_ID);

    // leaks
    const leaks = [];
    for (const k of STAT_ORDER) {
      const raw = st.raw(HERO_ID, k);
      if (raw.n < 25 || raw.pct === null) continue;
      const [lo, hi] = TARGETS[k];
      if (raw.pct < lo * 0.85 || raw.pct > hi * 1.15) leaks.push({ score: Math.abs(raw.pct - (lo + hi) / 2) * 10, title: `${STAT_DEFS[k].label} ${pct(raw.pct)} is ${raw.pct < lo ? 'low' : 'high'} (typical ${Math.round(lo * 100)}–${Math.round(hi * 100)}%)`, tip: statTip(k, raw.pct < lo) });
    }
    for (const [tag, count] of Object.entries(pr.data.tags)) {
      if (count < 2 || !LEAK_TAGS[tag]) continue;
      leaks.push({ score: count, title: `${LEAK_TAGS[tag].label} — ${count}×`, tip: LEAK_TAGS[tag].tip });
    }
    leaks.sort((a, b) => b.score - a.score);

    // positions
    const posOrder = ['UTG', 'UTG+1', 'UTG+2', 'LJ', 'HJ', 'CO', 'BTN', 'SB', 'BB'];
    const posRows = posOrder
      .filter((p) => pr.data.byPosition[p])
      .map((p) => {
        const d = pr.data.byPosition[p];
        return `<tr><td><span class="pos">${p}</span></td><td class="num">${d.hands}</td><td class="num">${pct(d.vpip / d.hands)}</td><td class="num ${d.netBB > 0 ? 'gain' : d.netBB < 0 ? 'loss' : ''}">${fmtBB(d.netBB)}</td><td class="num">${((d.netBB / d.hands) * 100).toFixed(0)}</td></tr>`;
      })
      .join('');

    // grades per street
    const streets = ['preflop', 'flop', 'turn', 'river'];
    const gradeRows = streets
      .map((s) => {
        const g = pr.data.decisions[s];
        const n = Object.values(g).reduce((a, b) => a + b, 0);
        return `<tr><td>${s}</td>${Object.keys(GRADES).map((k) => `<td class="num">${g[k]}</td>`).join('')}<td class="num">${n ? pct((g.best + g.good * 0.85 + g.inaccuracy * 0.5 + g.mistake * 0.2) / n) : '–'}</td></tr>`;
      })
      .join('');
    const accBars = streets.map((s) => {
      const g = pr.data.decisions[s];
      const n = Object.values(g).reduce((a, b) => a + b, 0);
      return { label: s, value: n ? (g.best + g.good * 0.85 + g.inaccuracy * 0.5 + g.mistake * 0.2) / n : null, n };
    });

    // opponents
    const seatsNow = new Set(Object.values(this.ctrl.table?.seats || []).filter(Boolean).map((p) => p.id));
    const oppIds = Object.keys(st.players).filter((id) => id !== HERO_ID && st.players[id].hands > 0);
    oppIds.sort((a, b) => (seatsNow.has(b) - seatsNow.has(a)) || st.players[b].hands - st.players[a].hands);
    const reveal = this.ctrl.settings.revealProfiles;
    const oppRows = oppIds
      .slice(0, 24)
      .map((id) => {
        const [, name, key] = id.split(':');
        const read = readOpponent(st, id);
        const r = (k) => st.raw(id, k).pct;
        const a = st.af(id);
        const prof = PROFILES[key];
        return `<tr class="${seatsNow.has(id) ? '' : 'muted'}"><td>${esc(name)}</td><td class="num">${st.players[id].hands}</td><td class="num">${pct(r('vpip'))}</td><td class="num">${pct(r('pfr'))}</td><td class="num">${a === null ? '–' : a === Infinity ? '∞' : a.toFixed(1)}</td><td class="num">${pct(r('foldToBet'))}</td><td class="num">${pct(r('wtsd'))}</td><td><b>${esc(read.label)}</b><div class="muted small">${esc(read.tip)}</div></td><td>${reveal && prof ? esc(prof.label) : '<span class="muted" title="Enable \'Reveal bot styles\' in Settings">hidden</span>'}</td></tr>`;
      })
      .join('');

    const drillRows = DRILLS.map((d) => {
      const s = pr.data.drills[d.key];
      return `<tr><td>${esc(d.title)}</td><td class="num">${s ? s.attempts : 0}</td><td class="num">${s && s.attempts ? pct(s.correct / s.attempts) : '–'}</td><td class="num">${s ? s.bestStreak : 0}</td></tr>`;
    }).join('');

    this.root.innerHTML = `
      <div class="tiles">${tiles}</div>
      <div class="stats-grid">
        <section class="panel wide-panel">
          <header><h3>Results</h3><span class="muted small">Cumulative winnings in big blinds, hand by hand</span></header>
          ${profitChart(hands)}
        </section>
        <section class="panel">
          <header><h3>Decision quality by street</h3></header>
          ${barChart(accBars)}
          <div class="table-scroll"><table class="data"><thead><tr><th>Street</th>${Object.entries(GRADES).map(([k, g]) => `<th class="num" title="${g.label}">${gradeBadge(k, g)}</th>`).join('')}<th class="num">Score</th></tr></thead><tbody>${gradeRows}</tbody></table></div>
        </section>
        <section class="panel">
          <header><h3>Leak finder</h3></header>
          ${leaks.length ? `<ol class="leaks">${leaks.slice(0, 8).map((l) => `<li><b>${esc(l.title)}</b><div class="muted">${esc(l.tip)}</div></li>`).join('')}</ol>` : '<p class="muted">No clear leaks yet. Leaks appear after ~25 hands of a stat or when the coach flags the same mistake twice.</p>'}
        </section>
        <section class="panel">
          <header><h3>Your HUD stats</h3><span class="muted small">What the bots see — they adapt to these</span></header>
          <div class="table-scroll"><table class="data"><thead><tr><th>Stat</th><th class="num">You</th><th class="num">n</th><th class="num">Typical (6-max)</th><th></th></tr></thead><tbody>${statRows}
          <tr><td title="Aggression factor: (bets + raises) / calls, postflop">AF</td><td class="num">${af === null ? '–' : af === Infinity ? '∞' : af.toFixed(2)}</td><td></td><td class="num muted">2.0–4.0</td><td></td></tr></tbody></table></div>
        </section>
        <section class="panel">
          <header><h3>By position</h3></header>
          ${posRows ? `<div class="table-scroll"><table class="data"><thead><tr><th>Pos</th><th class="num">Hands</th><th class="num">VPIP</th><th class="num">Net</th><th class="num">bb/100</th></tr></thead><tbody>${posRows}</tbody></table></div>` : '<p class="muted">No hands yet.</p>'}
        </section>
        <section class="panel">
          <header><h3>Drills</h3></header>
          <div class="table-scroll"><table class="data"><thead><tr><th>Drill</th><th class="num">Tries</th><th class="num">Correct</th><th class="num">Best streak</th></tr></thead><tbody>${drillRows}</tbody></table></div>
        </section>
        <section class="panel wide-panel">
          <header><h3>Opponent reads</h3><span class="muted small">Auto-read from observed stats — compare with your own reads</span><span class="spacer"></span><button class="btn ghost small" data-st="reset-opp">Reset opponent stats</button></header>
          ${oppRows ? `<div class="table-scroll"><table class="data"><thead><tr><th>Player</th><th class="num">Hands</th><th class="num">VPIP</th><th class="num">PFR</th><th class="num">AF</th><th class="num">Fold vs bet</th><th class="num">WTSD</th><th>Read & exploit</th><th>Actual style</th></tr></thead><tbody>${oppRows}</tbody></table></div>` : '<p class="muted">Play some hands to build reads.</p>'}
        </section>
      </div>
      <p class="pad"><button class="btn danger" data-st="reset">Reset my stats</button></p>`;
    this._wireChart();
  }

  _wireChart() {
    const svg = this.root.querySelector('svg.profit');
    if (!svg) return;
    const tip = this.root.querySelector('.chart-tip');
    const pts = JSON.parse(svg.dataset.pts);
    const cross = svg.querySelector('.cross');
    const dot = svg.querySelector('.hover-dot');
    const vb = svg.viewBox.baseVal;
    const move = (ev) => {
      const r = svg.getBoundingClientRect();
      const x = ((ev.clientX - r.left) / r.width) * vb.width;
      let best = pts[0];
      for (const p of pts) if (Math.abs(p[0] - x) < Math.abs(best[0] - x)) best = p;
      cross.setAttribute('x1', best[0]);
      cross.setAttribute('x2', best[0]);
      cross.style.opacity = 1;
      dot.setAttribute('cx', best[0]);
      dot.setAttribute('cy', best[1]);
      dot.style.opacity = 1;
      tip.innerHTML = `Hand ${best[2]}<br><b>${fmtBB(best[3])}</b> total · ${fmtBB(best[4])} this hand`;
      tip.style.opacity = 1;
      const px = (best[0] / vb.width) * r.width;
      tip.style.left = `${Math.min(r.width - 140, Math.max(0, px + 12))}px`;
      tip.style.top = `${(best[1] / vb.height) * r.height - 10}px`;
    };
    svg.addEventListener('pointermove', move);
    svg.addEventListener('pointerleave', () => {
      cross.style.opacity = 0;
      dot.style.opacity = 0;
      tip.style.opacity = 0;
    });
  }
}

function niceStep(range, target = 5) {
  const raw = range / target;
  const mag = 10 ** Math.floor(Math.log10(raw || 1));
  const norm = raw / mag;
  return (norm < 1.5 ? 1 : norm < 3 ? 2 : norm < 7 ? 5 : 10) * mag;
}

function profitChart(hands) {
  if (hands.length < 2) return '<p class="muted">Play a few hands to see your graph.</p>';
  const W = 720;
  const H = 240;
  const pad = { l: 48, r: 16, t: 12, b: 26 };
  let cum = 0;
  const series = [[0, 0, 0]];
  hands.forEach((h, i) => {
    cum += h.netBB;
    series.push([i + 1, cum, h.netBB]);
  });
  const ys = series.map((p) => p[1]);
  let min = Math.min(0, ...ys);
  let max = Math.max(0, ...ys);
  if (max - min < 10) {
    max += 5;
    min -= 5;
  }
  const step = niceStep(max - min);
  min = Math.floor(min / step) * step;
  max = Math.ceil(max / step) * step;
  const n = series.length - 1;
  const X = (i) => pad.l + (i / n) * (W - pad.l - pad.r);
  const Y = (v) => pad.t + ((max - v) / (max - min)) * (H - pad.t - pad.b);
  const path = series.map((p, i) => `${i ? 'L' : 'M'}${X(p[0]).toFixed(1)},${Y(p[1]).toFixed(1)}`).join('');
  const area = `${path}L${X(n).toFixed(1)},${Y(0).toFixed(1)}L${X(0).toFixed(1)},${Y(0).toFixed(1)}Z`;
  let grid = '';
  for (let v = min; v <= max + 1e-9; v += step) {
    grid += `<line class="${Math.abs(v) < 1e-9 ? 'zero' : 'gl'}" x1="${pad.l}" x2="${W - pad.r}" y1="${Y(v)}" y2="${Y(v)}"/><text class="tick" x="${pad.l - 6}" y="${Y(v) + 4}" text-anchor="end">${Math.round(v)}</text>`;
  }
  const xStep = niceStep(n, 6);
  for (let i = xStep; i <= n; i += xStep) grid += `<text class="tick" x="${X(i)}" y="${H - 6}" text-anchor="middle">${i}</text>`;
  const pts = series.map((p) => [Number(X(p[0]).toFixed(1)), Number(Y(p[1]).toFixed(1)), p[0], Number(p[1].toFixed(1)), Number(p[2].toFixed(1))]);
  const last = pts[pts.length - 1];
  return `<div class="chart-wrap">
    <svg class="profit" viewBox="0 0 ${W} ${H}" role="img" aria-label="Cumulative winnings: ${fmtBB(cum)} over ${n} hands" data-pts='${JSON.stringify(pts)}'>
      ${grid}
      <path class="area" d="${area}"/>
      <path class="line" d="${path}"/>
      <circle class="end-dot" cx="${last[0]}" cy="${last[1]}" r="4"/>
      <text class="end-label" x="${Math.min(last[0], W - pad.r - 4)}" y="${last[1] - 10}" text-anchor="end">${fmtBB(cum)}</text>
      <line class="cross" x1="0" x2="0" y1="${pad.t}" y2="${H - pad.b}" style="opacity:0"/>
      <circle class="hover-dot" r="5" cx="0" cy="0" style="opacity:0"/>
    </svg>
    <div class="chart-tip" style="opacity:0"></div>
  </div>`;
}

function barChart(bars) {
  const W = 360;
  const rowH = 30;
  const H = bars.length * rowH + 8;
  const lx = 64;
  const maxW = W - lx - 56;
  const rows = bars
    .map((b, i) => {
      const y = 4 + i * rowH;
      const w = b.value === null ? 0 : Math.max(2, b.value * maxW);
      return `<g><title>${b.label}: ${b.value === null ? 'no decisions' : `${pct(b.value)} over ${b.n} decisions`}</title>
        <text class="tick" x="${lx - 8}" y="${y + 15}" text-anchor="end">${b.label}</text>
        <rect class="bar-bg" x="${lx}" y="${y + 3}" width="${maxW}" height="16" rx="4"/>
        ${w ? `<path class="bar" d="M${lx},${y + 3}h${w - 4}a4,4 0 0 1 4,4v8a4,4 0 0 1 -4,4h-${w - 4}z"/>` : ''}
        <text class="bar-val" x="${lx + maxW + 8}" y="${y + 15}">${b.value === null ? '–' : pct(b.value)}</text></g>`;
    })
    .join('');
  return `<svg class="bars" viewBox="0 0 ${W} ${H}" role="img" aria-label="Decision score by street">${rows}</svg>`;
}

function statTip(k, low) {
  const tips = {
    vpip: low ? 'You are folding too many playable hands — open wider from late position and defend your big blind.' : 'You play too many hands. Fold weak offsuit hands, especially out of position.',
    pfr: low ? 'Raise more instead of calling. Raising wins pots preflop and gives you the initiative.' : 'You raise a very wide range; make sure your opens are position-dependent.',
    threeBet: low ? '3-bet more: value hands plus a few suited-ace bluffs.' : 'You 3-bet very often; opponents will 4-bet you light.',
    foldTo3bet: low ? 'You defend too much vs 3-bets with dominated hands.' : 'You fold too often to 3-bets — opponents can 3-bet you with any two cards.',
    steal: low ? 'Open more from CO/BTN/SB when folded to you — blinds are free money.' : 'Your steals are very wide; tighten up vs aggressive blinds.',
    cbet: low ? 'C-bet more as the preflop raiser, especially on high-card dry boards.' : 'You c-bet almost always; check back more on boards that favour the caller.',
    foldToCbet: low ? 'You float c-bets too much — fold weak hands that cannot improve.' : 'You fold to c-bets too often. Defend with pairs, draws and good overcards.',
    foldToBet: low ? 'You rarely fold when facing bets — you are paying off value bets.' : 'You fold too often when bet into; bots will bluff you relentlessly.',
    wtsd: low ? 'You give up before showdown too often.' : 'You go to showdown too much — fold more on the river vs strong lines.',
    wsd: low ? 'You lose at showdown — you are calling down with weak hands.' : 'You win most showdowns — you may be folding too much earlier.',
  };
  return tips[k];
}
