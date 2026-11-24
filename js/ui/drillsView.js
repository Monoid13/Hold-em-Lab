// Interactive drills UI.
import { cardHTML, esc, pct } from './dom.js';
import { DRILLS } from '../train/drills.js';

export class DrillsView {
  constructor(root, { progress }) {
    this.root = root;
    this.progress = progress;
    this.active = null;
    this.q = null;
    this.result = null;
    this.guess = 50;
    root.addEventListener('click', (e) => {
      const b = e.target.closest('[data-dr]');
      if (!b) return;
      const a = b.dataset.dr;
      if (a === 'open') this.start(b.dataset.k);
      else if (a === 'back') {
        this.active = null;
        this.render();
      } else if (a === 'next') this.next();
      else if (a === 'answer') this.answer(b.dataset.v);
      else if (a === 'submit-eq') this.answer(Number(this.root.querySelector('#eq-guess').value));
      else if (a === 'submit-outs') this.answer(Number(this.root.querySelector('#outs-guess').value));
    });
    root.addEventListener('input', (e) => {
      if (e.target.id === 'eq-guess') {
        this.guess = Number(e.target.value);
        const o = this.root.querySelector('#eq-out');
        if (o) o.textContent = `${this.guess}%`;
      }
    });
    root.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && e.target.id === 'outs-guess') this.answer(Number(e.target.value));
    });
  }

  drill() {
    return DRILLS.find((d) => d.key === this.active);
  }

  start(key) {
    this.active = key;
    this.next();
  }

  next() {
    this.q = this.drill().generate(Math.random);
    this.result = null;
    this.render();
    this.root.querySelector('#outs-guess')?.focus();
  }

  answer(v) {
    if (this.result || !this.q) return;
    let val = v;
    if (this.q.kind === 'potodds') val = Number(v);
    if (this.q.kind === 'showdown') val = v === 'split' ? 'split' : Number(v);
    this.result = this.drill().grade(this.q, val);
    this.result.answered = val;
    this.stats = this.progress.recordDrill(this.active, this.result);
    this.render();
  }

  key(e) {
    if (!this.active) return false;
    if ((e.key === 'Enter' || e.key === ' ') && this.result) {
      this.next();
      return true;
    }
    return false;
  }

  render() {
    if (!this.active) {
      this.root.innerHTML = `<p class="muted">Short, focused exercises. Each one trains a skill you use every hand.</p><div class="drill-cards">${DRILLS.map((d) => {
        const s = this.progress.data.drills[d.key];
        return `<button class="drill-card" data-dr="open" data-k="${d.key}"><h3>${esc(d.title)}</h3><p>${esc(d.desc)}</p><div class="muted small">${s ? `${s.attempts} tries · ${pct(s.correct / s.attempts)} correct · best streak ${s.bestStreak}` : 'Not tried yet'}</div></button>`;
      }).join('')}</div>`;
      return;
    }
    const d = this.drill();
    const s = this.progress.data.drills[d.key];
    const head = `<div class="drill-head"><button class="btn ghost" data-dr="back">← All drills</button><h2>${esc(d.title)}</h2><span class="spacer"></span>${s ? `<span class="muted">Streak <b>${s.streak}</b> · ${pct(s.correct / s.attempts)} of ${s.attempts}</span>` : ''}</div>`;
    this.root.innerHTML = `${head}<div class="drill-body panel">${this._question()}${this._result()}</div>`;
  }

  _choice(v, label) {
    const r = this.result;
    let cls = 'btn big-choice';
    if (r) {
      if (String(v) === String(r.answered)) cls += r.correct ? ' right' : ' wrong';
      if (r.best !== undefined && String(v) === String(r.best) && !r.correct) cls += ' was-best';
      if (this.q.kind === 'potodds' && Number(v) === Math.round(this.q.answer * 100)) cls += ' was-best';
      if (this.q.kind === 'showdown' && String(v) === String(this.q.answer)) cls += ' was-best';
    }
    return `<button class="${cls}" data-dr="answer" data-v="${esc(v)}" ${r ? 'disabled' : ''}>${esc(label)}</button>`;
  }

  _question() {
    const q = this.q;
    const cards = (cs) => `<div class="cards-row">${cs.map((c) => cardHTML(c)).join('')}</div>`;
    switch (q.kind) {
      case 'preflop':
        return `<p class="q">${esc(q.prompt)}</p>${cards(q.hole)}<p class="muted">${esc(q.hand)}</p>
          <div class="choices">${q.choices.map((c) => this._choice(c, q.choiceLabels[c])).join('')}</div>`;
      case 'equity':
        return `<p class="q">What is your equity (win% + half of ties) ${q.board.length ? `on this ${q.street}` : 'preflop'}?</p>
          <div class="eq-matchup"><div><div class="muted">You</div>${cards(q.hero)}</div><div class="vs">vs</div><div><div class="muted">Villain</div>${cards(q.villain)}</div></div>
          ${q.board.length ? `<div class="muted">Board</div>${cards(q.board)}` : ''}
          <div class="eq-input"><input type="range" id="eq-guess" min="0" max="100" value="${this.result ? this.result.answered : this.guess}" ${this.result ? 'disabled' : ''}><output id="eq-out">${this.result ? this.result.answered : this.guess}%</output>
          <button class="btn primary" data-dr="submit-eq" ${this.result ? 'disabled' : ''}>Submit</button></div>`;
      case 'potodds':
        return `<p class="q">${esc(q.prompt)}</p><div class="choices">${q.options.map((o) => this._choice(o, `${o}%`)).join('')}</div>`;
      case 'outs':
        return `<p class="q">How many outs do you have to make a <b>straight or flush</b> (or better)?</p>
          <div class="muted">Your hand</div>${cards(q.hole)}<div class="muted">Board</div>${cards(q.board)}
          <div class="eq-input"><input type="number" id="outs-guess" min="0" max="25" ${this.result ? `value="${this.result.answered}" disabled` : ''} aria-label="Number of outs"><button class="btn primary" data-dr="submit-outs" ${this.result ? 'disabled' : ''}>Submit</button></div>
          ${this.result ? `<div class="muted">Outs:</div><div class="cards-row">${q.outCards.map((c) => cardHTML(c, { small: true })).join('')}</div>` : ''}`;
      case 'showdown':
        return `<p class="q">Who wins at showdown?</p><div class="muted">Board</div>${cards(q.board)}
          <div class="sd-hands">${q.hands.map((h, i) => `<div><div class="muted">Player ${i + 1}</div>${cards(h)}</div>`).join('')}</div>
          <div class="choices">${q.hands.map((_, i) => this._choice(i, `Player ${i + 1}`)).join('')}${this._choice('split', 'Split pot')}</div>`;
      default:
        return '';
    }
  }

  _result() {
    const r = this.result;
    if (!r) return '';
    return `<div class="drill-result ${r.correct ? 'right' : 'wrong'}" role="status">
      <h3>${r.correct ? '✓ Correct' : '✗ Not quite'}${r.score !== undefined && this.q.kind === 'equity' ? ` · score ${r.score}` : ''}</h3>
      <ul>${r.explanation.map((l) => `<li>${esc(l)}</li>`).join('')}</ul>
      <button class="btn primary" data-dr="next">Next <kbd>Enter</kbd></button>
    </div>`;
  }
}
