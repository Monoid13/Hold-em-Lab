// Range charts: opening ranges by position and the overall hand ranking.
import { esc } from './dom.js';
import { HAND_CLASSES, RFI_SETS, RFI_WIDTH, PERCENTILE, RANK_POS } from '../core/ranges.js';

const POSITIONS = ['UTG', 'UTG+1', 'UTG+2', 'LJ', 'HJ', 'CO', 'BTN', 'SB', 'HU'];

export class RangesView {
  constructor(root) {
    this.root = root;
    this.pos = 'CO';
    this.mode = 'rfi';
    this.info = null;
    root.addEventListener('click', (e) => {
      const b = e.target.closest('[data-rg]');
      if (b) {
        if (b.dataset.rg === 'pos') this.pos = b.dataset.p;
        if (b.dataset.rg === 'mode') this.mode = b.dataset.m;
        this.render();
        return;
      }
      const cell = e.target.closest('[data-cls]');
      if (cell) {
        this.info = Number(cell.dataset.cls);
        this.render();
      }
    });
  }

  render() {
    const set = RFI_SETS[this.pos];
    const cells = HAND_CLASSES.map((c) => {
      let cls = 'cell';
      let style = '';
      if (this.mode === 'rfi') {
        if (set.has(c.index)) cls += ' in';
      } else {
        const p = PERCENTILE[c.index];
        // sequential: stronger = darker blue
        const step = Math.min(6, Math.floor(p * 7));
        cls += ` seq-${6 - step}`;
      }
      if (this.info === c.index) cls += ' sel';
      return `<button class="${cls}" style="${style}" data-cls="${c.index}" title="${esc(c.name)}">${esc(c.name)}</button>`;
    }).join('');
    const posBtns = POSITIONS.map((p) => `<button class="btn chip-btn ${this.pos === p ? 'on' : ''}" data-rg="pos" data-p="${p}">${p === 'HU' ? 'Heads-up BTN' : p}</button>`).join('');
    const c = this.info !== null ? HAND_CLASSES[this.info] : null;
    let infoHtml = '<p class="muted">Click a hand for details.</p>';
    if (c) {
      const opens = POSITIONS.filter((p) => RFI_SETS[p].has(c.index));
      infoHtml = `<h3>${esc(c.name)}</h3>
        <p>Rank <b>#${RANK_POS[c.index] + 1}</b> of 169 · top <b>${Math.round(PERCENTILE[c.index] * 100)}%</b> of hands · ${c.combos} combos.</p>
        <p>Open-raise from: ${opens.length ? opens.map((p) => `<span class="pos">${p === 'HU' ? 'HU' : p}</span>`).join(' ') : '<span class="muted">never (fold when first in)</span>'}</p>`;
    }
    this.root.innerHTML = `<div class="ranges-layout">
      <div>
        <div class="filters">
          <button class="btn chip-btn ${this.mode === 'rfi' ? 'on' : ''}" data-rg="mode" data-m="rfi">Opening ranges</button>
          <button class="btn chip-btn ${this.mode === 'rank' ? 'on' : ''}" data-rg="mode" data-m="rank">Hand ranking</button>
        </div>
        ${this.mode === 'rfi' ? `<div class="filters">${posBtns}</div><p class="muted">Raise-first-in from <b>${this.pos === 'HU' ? 'the heads-up button' : this.pos}</b> at 100bb: <b>${(RFI_WIDTH[this.pos] * 100).toFixed(1)}%</b> of hands. Highlighted hands are opened; everything else folds.</p>` : '<p class="muted">Darker = stronger. The ranking blends raw equity with playability (suitedness, connectedness); the bots and coach use it to build ranges.</p>'}
        <div class="grid13">${cells}</div>
        <p class="muted small">Upper-right triangle: suited · diagonal: pairs · lower-left: offsuit.</p>
      </div>
      <aside class="range-info panel">${infoHtml}
        <h3>How to use this</h3>
        <ul class="tips">
          <li>Open tighter in early position: more players are left to act behind you.</li>
          <li>The button opens the widest — you are guaranteed position after the flop.</li>
          <li>Facing a raise, continue with a range narrower than the opener's, wider from the big blind (you already have 1bb invested).</li>
          <li>Drill these in <b>Drills → Preflop decisions</b>.</li>
        </ul>
      </aside>
    </div>`;
  }
}
