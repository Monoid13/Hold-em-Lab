// Training sidebar: live HUD (hand strength, equity, odds), coach feedback, hint and hand log.
import { esc, pct, fmt, fmtBB, gradeBadge, cardText } from './dom.js';
import { GRADES } from '../coach/coach.js';
import { analyzeHand } from '../core/handinfo.js';
import { outsToEquity } from '../core/handinfo.js';

export class Sidebar {
  constructor(root, ctrl) {
    this.root = root;
    this.ctrl = ctrl;
    this.hintShown = false;
    this.hintFor = null;
    root.addEventListener('click', (e) => {
      const b = e.target.closest('[data-side]');
      if (!b) return;
      if (b.dataset.side === 'hint') this.showHint();
      if (b.dataset.side === 'collapse') {
        b.closest('.panel').classList.toggle('collapsed');
      }
    });
  }

  showHint() {
    const st = this.ctrl.state;
    if (!st.awaitingHero || !st.spot) return;
    this.hintShown = true;
    this.hintFor = this.ctrl.table.hand.id + ':' + this.ctrl.table.hand.log.length;
    this.render();
  }

  render() {
    const c = this.ctrl;
    const st = c.state;
    const h = c.table?.hand;
    const set = c.settings;
    if (this.hintFor && h && this.hintFor !== h.id + ':' + h.log.length) this.hintShown = false;
    let out = '';

    // HUD
    if (set.showHud && h && h.players[c.heroSeat]) {
      const hp = h.players[c.heroSeat];
      const board = h.board.slice(0, h.finished && st.revealCount < 5 ? st.revealCount : h.board.length);
      const info = analyzeHand(hp.hole, board);
      const spot = st.spot;
      const cardsToCome = board.length === 3 ? 2 : board.length === 4 ? 1 : 0;
      const rows = [];
      rows.push(['Hand', `<b>${esc(info.name)}</b>${info.detail ? ` <span class="muted">· ${esc(info.detail)}</span>` : ''}`]);
      if (info.draws.description.length) rows.push(['Draws', esc(info.draws.description.join(', '))]);
      if (cardsToCome && info.outs.drawOuts) rows.push(['Outs', `${info.outs.drawOuts} → ${pct(outsToEquity(info.outs.drawOuts, 1))} next card${cardsToCome === 2 ? `, ${pct(outsToEquity(info.outs.drawOuts, 2))} by river` : ''}`]);
      if (info.texture) rows.push(['Board', esc(info.texture.description)]);
      rows.push(['Position', `${esc(hp.position)}${spot ? (spot.ip ? ' · in position' : ' · out of position') : ''}`]);
      let eqBlock = '';
      if (spot && !hp.folded) {
        const eq = spot.equity;
        const need = spot.needed;
        eqBlock = `<div class="eq-meter" title="Equity vs the opponents' estimated ranges (built only from their actions and stats)">
          <div class="eq-bar"><div class="eq-fill" style="width:${Math.round(eq * 100)}%"></div>${need > 0 ? `<div class="eq-need" style="left:${Math.round(need * 100)}%"></div>` : ''}</div>
          <div class="eq-legend"><span>Equity <b>${pct(eq)}</b></span>${need > 0 ? `<span>Need <b>${pct(need)}</b></span>` : ''}<span>SPR <b>${spot.spr.toFixed(1)}</b></span></div>
        </div>`;
        if (spot.oppRanges.length) rows.push(['Ranges', spot.oppRanges.map((r) => `${esc(r.name)} ~${pct(r.width)}`).join(' · ')]);
      }
      out += `<section class="panel hud"><header><h3>Your hand</h3><span class="hole">${hp.hole.map(cardText).join(' ')}</span></header>
        ${eqBlock}
        <dl>${rows.map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`).join('')}</dl>
        ${st.awaitingHero && spot && !this.hintShown ? '<button class="btn ghost small wide" data-side="hint">💡 Show coach hint</button>' : ''}
        ${this.hintShown && spot ? this._hint(spot) : ''}
      </section>`;
    }

    // Coach feedback
    if (set.coachMode !== 'off') {
      const g = st.lastGrade;
      const show = g && (set.coachMode === 'all' || (g.grade !== 'best' && g.grade !== 'good'));
      const totals = c.progress.totals();
      out += `<section class="panel coach"><header><h3>Coach</h3>${totals.accuracy !== null ? `<span class="muted small">Decision score ${pct(totals.accuracy)} · ${totals.decisions} graded</span>` : ''}</header>`;
      if (show) {
        out += `<div class="feedback">
          <div class="fb-head">${gradeBadge(g.grade, GRADES[g.grade])} <span class="muted">${esc(g.street)}</span>${g.evLossBB && g.grade !== 'best' ? `<span class="evloss">−${g.evLossBB.toFixed(1)} bb EV</span>` : ''}</div>
          <div class="fb-actions">You: <b>${esc(g.heroLabel)}</b>${g.grade !== 'best' ? ` · Coach: <b>${esc(g.bestLabel)}</b>` : ''}</div>
          <ul>${g.lines.map((l) => `<li>${esc(l)}</li>`).join('')}</ul>
        </div>`;
      } else if (g && set.coachMode === 'mistakes') {
        out += `<p class="muted small">Last decision: ${gradeBadge(g.grade, GRADES[g.grade])} — feedback is shown for mistakes only.</p>`;
      } else out += '<p class="muted small">Make a decision and the coach will grade it against the estimated EV of every option.</p>';
      out += '</section>';
    }

    // Log
    out += `<section class="panel log"><header><h3>Hand log</h3></header><div class="log-lines">${st.log.slice(-40).map((l) => `<div>${esc(l)}</div>`).join('')}</div></section>`;
    this.root.innerHTML = out;
    const ll = this.root.querySelector('.log-lines');
    if (ll) ll.scrollTop = ll.scrollHeight;
  }

  _hint(spot) {
    let body = `<div class="hint"><div><b>Coach suggests: ${esc(spot.recommendedLabel)}</b></div>`;
    if (spot.preflop) {
      body += `<div class="muted small">${spot.mix.map((m) => `${esc(m.label)} ${pct(m.w)}`).join(' · ')}</div>`;
      body += `<ul>${spot.preflop.notes.map((n) => `<li>${esc(n)}</li>`).join('')}</ul>`;
    } else {
      const opts = spot.postflop.options.slice().sort((a, b) => b.ev - a.ev).slice(0, 4);
      body += `<table class="mini"><tr><th>Option</th><th>EV</th><th>Folds</th></tr>${opts
        .map((o) => `<tr><td>${esc(labelOf(o, spot))}</td><td>${(o.ev >= 0 ? '+' : '') + fmt(o.ev)}</td><td>${o.foldAll !== undefined ? pct(o.foldAll) : ''}</td></tr>`)
        .join('')}</table><div class="muted small">EV in chips relative to folding now.</div>`;
    }
    return body + '</div>';
  }
}

function labelOf(o, spot) {
  if (o.type === 'fold') return 'Fold';
  if (o.type === 'check') return 'Check';
  if (o.type === 'call') return `Call ${o.amount}`;
  const v = spot.view;
  if (o.amount >= v.legal.maxTo) return 'All-in';
  return `${o.type === 'bet' ? 'Bet' : 'Raise to'} ${o.amount} (${Math.round(((o.amount - v.me.bet) / v.potTotal) * 100)}%)`;
}

export { fmtBB };
