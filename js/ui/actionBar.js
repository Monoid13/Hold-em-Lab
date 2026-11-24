// Hero action controls with bet slider, presets and keyboard shortcuts.
import { esc, fmt, pct } from './dom.js';

export class ActionBar {
  constructor(root, ctrl, { onHint }) {
    this.root = root;
    this.ctrl = ctrl;
    this.onHint = onHint;
    this.amount = 0;
    root.addEventListener('click', (e) => this._click(e));
    root.addEventListener('input', (e) => {
      if (e.target.matches('.bet-slider')) this._setAmount(Number(e.target.value), 'slider');
    });
    root.addEventListener('change', (e) => {
      if (e.target.matches('.bet-input')) this._setAmount(Number(e.target.value), 'input');
      if (e.target.matches('#autodeal')) {
        ctrl.settings.autoDeal = e.target.checked;
        this.onSettings?.();
        if (e.target.checked && ctrl.state.handOver) ctrl.deal();
      }
    });
  }

  legal() {
    const t = this.ctrl.table;
    return this.ctrl.state.awaitingHero && t?.hand ? t.legalActions(this.ctrl.heroSeat) : null;
  }

  presets(l) {
    const h = this.ctrl.table.hand;
    const out = [];
    const bb = h.bb;
    const pot = l.pot;
    if (h.street === 'preflop') {
      if (h.raiseCount === 0) {
        for (const x of [2.5, 3, 4]) out.push([`${x}bb`, Math.round(x * bb)]);
      } else {
        out.push(['3×', l.currentBet * 3]);
        out.push(['4×', l.currentBet * 4]);
      }
      out.push(['Pot', l.currentBet + pot + l.toCall]);
    } else if (l.canBet) {
      for (const [lab, f] of [['⅓', 0.33], ['½', 0.5], ['⅔', 0.66], ['Pot', 1]]) out.push([lab, Math.round(l.myBet + f * pot)]);
    } else {
      out.push(['2.5×', Math.round(l.currentBet * 2.5)]);
      out.push(['Pot', l.currentBet + pot + l.toCall]);
    }
    out.push(['All-in', l.maxTo]);
    return out
      .map(([lab, v]) => [lab, Math.max(l.minTo, Math.min(l.maxTo, Math.round(v)))])
      .filter(([lab, v], i, arr) => arr.findIndex((x) => x[1] === v) === i || lab === 'All-in');
  }

  _setAmount(v, src) {
    const l = this.legal();
    if (!l) return;
    this.amount = Math.max(l.minTo, Math.min(l.maxTo, Math.round(v || l.minTo)));
    const input = this.root.querySelector('.bet-input');
    const slider = this.root.querySelector('.bet-slider');
    if (input && src !== 'input') input.value = this.amount;
    if (slider && src !== 'slider') slider.value = this.amount;
    const lbl = this.root.querySelector('.raise-label');
    if (lbl) lbl.textContent = this._raiseText(l);
  }

  _raiseText(l) {
    if (this.amount >= l.maxTo) return `All-in ${fmt(l.maxTo)}`;
    return `${l.canBet ? 'Bet' : 'Raise to'} ${fmt(this.amount)}`;
  }

  _click(e) {
    const b = e.target.closest('[data-act]');
    if (!b || b.disabled) return;
    const act = b.dataset.act;
    if (act === 'preset') return this._setAmount(Number(b.dataset.v));
    this.perform(act);
  }

  perform(act) {
    const c = this.ctrl;
    if (act === 'deal') return c.deal();
    if (act === 'pause') return c.togglePause();
    if (act === 'rebuy') return c.rebuyHero();
    if (act === 'hint') return this.onHint?.();
    const l = this.legal();
    if (!l) return;
    if (act === 'fold') {
      if (l.canCheck) return; // never fold when checking is free
      c.heroAct({ type: 'fold' });
    } else if (act === 'call') c.heroAct({ type: l.toCall > 0 ? 'call' : 'check' });
    else if (act === 'raise') {
      if (!(l.canBet || l.canRaise)) return;
      c.heroAct({ type: l.canBet ? 'bet' : 'raise', amount: this.amount || l.minTo });
    } else if (act === 'allin') c.heroAct({ type: 'allin' });
  }

  render() {
    const c = this.ctrl;
    const st = c.state;
    const l = this.legal();
    if (!c.table) {
      this.root.innerHTML = '';
      return;
    }
    if (!l) {
      let status = '';
      if (st.heroBusted) status = `<span>You are out of chips.</span><button class="btn primary" data-act="rebuy">Rebuy (${fmt(c.startingStack)})</button>`;
      else if (st.gameOver) status = '<span>Game over.</span>';
      else if (st.handOver) status = `<button class="btn primary" data-act="deal">Deal next hand <kbd>Space</kbd></button>`;
      else if (st.thinkingSeat >= 0) status = `<span class="muted">${esc(c.table.seats[st.thinkingSeat]?.name)} is thinking…</span>`;
      else status = '<span class="muted">…</span>';
      const heroIn = c.table.hand?.players[c.heroSeat];
      const folded = heroIn?.folded && !c.table.hand.finished ? '<span class="muted">You folded — watch how the hand plays out.</span>' : '';
      this.root.innerHTML = `<div class="ab-row waiting">${status} ${folded}
        <span class="spacer"></span>
        <label class="toggle"><input type="checkbox" id="autodeal" ${c.settings.autoDeal ? 'checked' : ''}> Auto-deal</label>
        <button class="btn ghost" data-act="pause">${c.paused ? '▶ Resume' : '⏸ Pause'}</button>
      </div>`;
      return;
    }
    if (!this.amount || this.amount < l.minTo || this.amount > l.maxTo || this._forHand !== `${c.table.hand.id}-${c.table.hand.log.length}`) {
      this._forHand = `${c.table.hand.id}-${c.table.hand.log.length}`;
      const def = this.presets(l)[0];
      this.amount = def ? def[1] : l.minTo;
    }
    const canAgg = l.canBet || l.canRaise;
    const odds = l.toCall > 0 ? l.toCall / (l.pot + l.toCall) : 0;
    const presets = canAgg ? this.presets(l).map(([lab, v]) => `<button class="btn chip-btn" data-act="preset" data-v="${v}">${esc(lab)}</button>`).join('') : '';
    const step = Math.max(1, Math.round(c.table.hand.bb / 2));
    this.root.innerHTML = `
      <div class="ab-info">
        <span>Pot <b>${fmt(l.pot)}</b></span>
        ${l.toCall > 0 ? `<span>To call <b>${fmt(l.toCall)}</b></span><span title="Equity needed to break even on a call">Need <b>${pct(odds)}</b> equity</span>` : '<span>No bet to you</span>'}
        <span>Stack <b>${fmt(l.stack)}</b></span>
        <span class="spacer"></span>
        <button class="btn ghost small" data-act="hint" title="Ask the coach (H)">💡 Hint <kbd>H</kbd></button>
      </div>
      <div class="ab-row">
        <button class="btn fold" data-act="fold" ${l.canCheck ? 'disabled title="Checking is free"' : ''}>Fold <kbd>F</kbd></button>
        <button class="btn call" data-act="call">${l.toCall > 0 ? `${l.toCall >= l.stack ? 'Call all-in' : 'Call'} ${fmt(l.toCall)}` : 'Check'} <kbd>C</kbd></button>
        ${canAgg ? `<button class="btn raise" data-act="raise"><span class="raise-label">${esc(this._raiseText(l))}</span> <kbd>R</kbd></button>` : ''}
      </div>
      ${canAgg ? `<div class="ab-row sizing">
        <div class="presets">${presets}</div>
        <input class="bet-slider" type="range" min="${l.minTo}" max="${l.maxTo}" step="${step}" value="${this.amount}" aria-label="Bet size">
        <input class="bet-input" type="number" min="${l.minTo}" max="${l.maxTo}" value="${this.amount}" aria-label="Bet amount">
      </div>` : ''}`;
  }
}
