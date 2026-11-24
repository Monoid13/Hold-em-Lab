// Hand history browser with step-by-step replay and coach annotations.
import { cardHTML, esc, fmt, fmtBB, gradeBadge } from './dom.js';
import { GRADES } from '../coach/coach.js';
import { PROFILES } from '../bots/profiles.js';

export class ReviewView {
  constructor(root, { history, settings }) {
    this.root = root;
    this.history = history;
    this.settings = settings;
    this.sel = null;
    this.step = 0;
    this.filter = 'all';
    root.addEventListener('click', (e) => {
      const item = e.target.closest('[data-hand]');
      if (item) {
        this.sel = item.dataset.hand;
        this.step = 0;
        this.render();
        return;
      }
      const b = e.target.closest('[data-rv]');
      if (!b) return;
      const rec = this.current();
      const max = rec ? rec.steps.length - 1 : 0;
      const a = b.dataset.rv;
      if (a === 'first') this.step = 0;
      if (a === 'prev') this.step = Math.max(0, this.step - 1);
      if (a === 'next') this.step = Math.min(max, this.step + 1);
      if (a === 'last') this.step = max;
      if (a === 'goto') this.step = Number(b.dataset.step);
      if (a === 'filter') this.filter = b.dataset.f;
      if (a === 'reveal') this.settings.revealInReview = !this.revealAll();
      this.render();
    });
  }

  revealAll() {
    return this.settings.revealInReview !== false;
  }

  current() {
    return this.history.hands.find((h) => h.id === this.sel) || null;
  }

  key(e) {
    if (!this.current()) return false;
    if (e.key === 'ArrowRight') this.step = Math.min(this.current().steps.length - 1, this.step + 1);
    else if (e.key === 'ArrowLeft') this.step = Math.max(0, this.step - 1);
    else return false;
    this.render();
    return true;
  }

  render() {
    let hands = this.history.hands.slice().reverse();
    if (this.filter === 'mistakes') hands = hands.filter((h) => h.decisions.some((d) => ['inaccuracy', 'mistake', 'blunder'].includes(d.grade)));
    if (this.filter === 'big') hands = hands.filter((h) => Math.abs((h.nets?.[h.heroSeat] || 0) / h.bb) >= 20);
    if (this.filter === 'showdown') hands = hands.filter((h) => h.results && !h.results.uncontested);
    if (!this.sel && hands.length) this.sel = hands[0].id;
    const list = hands
      .map((h) => {
        const hero = h.players.find((p) => p.seat === h.heroSeat);
        const net = (h.nets?.[h.heroSeat] || 0) / h.bb;
        const worst = worstGrade(h.decisions);
        return `<button class="hand-item ${h.id === this.sel ? 'active' : ''}" data-hand="${h.id}">
          <span class="hi-no">#${h.handNo}</span>
          <span class="hi-cards">${hero ? hero.hole.map((c) => cardHTML(c, { small: true })).join('') : ''}</span>
          <span class="hi-pos">${esc(hero?.position ?? '')}</span>
          <span class="hi-net ${net > 0 ? 'gain' : net < 0 ? 'loss' : ''}">${fmtBB(net)}</span>
          <span class="hi-grade">${worst ? `<span class="grade-dot grade-${worst}" title="${GRADES[worst].label}">${GRADES[worst].symbol}</span>` : ''}</span>
        </button>`;
      })
      .join('');
    const filters = [['all', 'All'], ['mistakes', 'With mistakes'], ['big', 'Big pots'], ['showdown', 'Showdowns']]
      .map(([k, l]) => `<button class="btn chip-btn ${this.filter === k ? 'on' : ''}" data-rv="filter" data-f="${k}">${l}</button>`)
      .join('');
    this.root.innerHTML = `<div class="review-layout">
      <aside class="hand-list"><div class="filters">${filters}</div>${list || '<p class="muted pad">No hands yet — play some at the table.</p>'}</aside>
      <section class="replay">${this._replay()}</section>
    </div>`;
  }

  _replay() {
    const rec = this.current();
    if (!rec) return '<p class="muted pad">Select a hand to replay it step by step.</p>';
    const step = rec.steps[this.step];
    const reveal = this.revealAll();
    const isLast = this.step === rec.steps.length - 1;
    const sdSeats = new Set(rec.results?.showdown.map((s) => s.seat) || []);
    const rows = rec.players
      .map((p) => {
        const ps = step.players[p.seat];
        const showCards = p.seat === rec.heroSeat || reveal || (isLast && sdSeats.has(p.seat));
        const prof = p.profile ? PROFILES[p.profile] : null;
        const sd = isLast ? rec.results?.showdown.find((s) => s.seat === p.seat) : null;
        const net = rec.nets?.[p.seat] ?? 0;
        return `<tr class="${ps.folded ? 'folded' : ''} ${step.seat === p.seat ? 'acting' : ''}">
          <td>${esc(p.avatar ?? '')} ${esc(p.name)}${prof && reveal ? ` <span class="muted small">${esc(prof.label)}</span>` : ''}</td>
          <td><span class="pos">${esc(p.position)}</span></td>
          <td class="cards">${p.hole.map((c) => cardHTML(c, { small: true, hidden: !showCards })).join('')}</td>
          <td class="num">${fmt(ps.stack)}</td>
          <td class="num">${ps.bet ? fmt(ps.bet) : ''}</td>
          <td>${sd ? esc(sd.handName) : ps.folded ? '<span class="muted">folded</span>' : ''}${isLast ? ` <b class="${net > 0 ? 'gain' : net < 0 ? 'loss' : ''}">${net ? (net > 0 ? '+' : '') + fmt(net) : ''}</b>` : ''}</td>
        </tr>`;
      })
      .join('');
    const board = [0, 1, 2, 3, 4].map((i) => (step.board[i] !== undefined ? cardHTML(step.board[i]) : '<div class="card slot"></div>')).join('');
    const decisions = rec.decisions
      .map((d) => `<div class="decision ${d.step === this.step ? 'current' : ''}">
        <div class="fb-head"><button class="link" data-rv="goto" data-step="${Math.max(0, d.step - 1)}">${esc(d.street)}</button> ${gradeBadge(d.grade, GRADES[d.grade])}${d.evLossBB && d.grade !== 'best' ? `<span class="evloss">−${d.evLossBB.toFixed(1)} bb EV</span>` : ''}</div>
        <div class="fb-actions">You: <b>${esc(d.heroLabel)}</b>${d.grade !== 'best' ? ` · Coach: <b>${esc(d.bestLabel)}</b>` : ''}</div>
        <ul>${d.lines.map((l) => `<li>${esc(l)}</li>`).join('')}</ul>
      </div>`)
      .join('');
    const actions = rec.steps
      .map((s, i) => `<button class="step ${i === this.step ? 'on' : ''}" data-rv="goto" data-step="${i}">${esc(s.label)}</button>`)
      .join('');
    return `<header class="replay-head"><h2>Hand #${rec.handNo}</h2><span class="muted">${new Date(rec.time).toLocaleString()} · blinds ${rec.sb}/${rec.bb}</span>
        <span class="spacer"></span><label class="toggle"><input type="checkbox" data-rv="reveal" ${reveal ? 'checked' : ''}> Show all hole cards</label></header>
      <div class="replay-board"><div class="board">${board}</div><div class="pot">Pot ${fmt(step.pot + Object.values(step.players).reduce((s, p) => s + p.bet, 0))}</div><div class="muted">${esc(step.label)}</div></div>
      <div class="replay-controls">
        <button class="btn ghost" data-rv="first" aria-label="First">⏮</button><button class="btn ghost" data-rv="prev" aria-label="Previous">◀</button>
        <span>Step ${this.step + 1} / ${rec.steps.length}</span>
        <button class="btn ghost" data-rv="next" aria-label="Next">▶</button><button class="btn ghost" data-rv="last" aria-label="Last">⏭</button>
        <span class="muted small">← → keys</span>
      </div>
      <div class="table-scroll"><table class="replay-table"><thead><tr><th>Player</th><th>Pos</th><th>Cards</th><th class="num">Stack</th><th class="num">Bet</th><th></th></tr></thead><tbody>${rows}</tbody></table></div>
      <div class="steps">${actions}</div>
      <h3>Coach review</h3>
      ${decisions || '<p class="muted">No graded decisions in this hand (you may have folded preflop with no choice to make, or the coach was off).</p>'}`;
  }
}

const ORDER = ['blunder', 'mistake', 'inaccuracy', 'good', 'best'];
function worstGrade(ds) {
  let w = null;
  for (const d of ds) if (w === null || ORDER.indexOf(d.grade) < ORDER.indexOf(w)) w = d.grade;
  return w;
}
