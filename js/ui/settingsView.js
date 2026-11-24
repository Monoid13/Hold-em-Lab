// Settings form.
import { esc } from './dom.js';
import { PROFILES, PROFILE_KEYS, LINEUPS } from '../bots/profiles.js';
import { DEFAULT_SETTINGS } from '../app/settings.js';

export class SettingsView {
  constructor(root, { settings, onApply, onLive, onResetAll }) {
    this.root = root;
    this.settings = settings;
    this.onApply = onApply;
    this.onLive = onLive;
    this.onResetAll = onResetAll;
    root.addEventListener('change', (e) => {
      const el = e.target;
      if (!el.name) return;
      const live = ['speed', 'coachMode', 'showHud', 'showOppStats', 'revealProfiles', 'showAllCards', 'fourColor', 'autoDeal', 'allInEquity', 'autoRebuy'];
      const v = el.type === 'checkbox' ? el.checked : el.type === 'number' ? Number(el.value) : el.value;
      if (el.name.startsWith('seat-')) {
        if (this.draft.lineup !== 'custom' && LINEUPS[this.draft.lineup]) this.draft.customSeats = LINEUPS[this.draft.lineup].seats.slice();
        this.draft.customSeats[Number(el.name.slice(5))] = v;
        this.draft.lineup = 'custom';
        this.render();
        return;
      }
      this.draft[el.name] = v;
      if (live.includes(el.name)) {
        this.settings[el.name] = v;
        this.onLive?.(el.name);
      }
      if (el.name === 'lineup' || el.name === 'numOpponents' || el.name === 'mode') this.render();
    });
    root.addEventListener('click', (e) => {
      const b = e.target.closest('[data-set]');
      if (!b) return;
      if (b.dataset.set === 'apply') {
        Object.assign(this.settings, this.draft, { customSeats: this.draft.customSeats.slice() });
        this.onApply?.();
      }
      if (b.dataset.set === 'defaults') {
        this.draft = { ...DEFAULT_SETTINGS, customSeats: DEFAULT_SETTINGS.customSeats.slice() };
        this.render();
      }
      if (b.dataset.set === 'wipe' && confirm('Delete ALL saved data (stats, history, drills, settings)?')) this.onResetAll?.();
    });
  }

  render() {
    if (!this.draft) this.draft = { ...this.settings, customSeats: this.settings.customSeats.slice() };
    const d = this.draft;
    const sel = (name, opts) => `<select name="${name}">${opts.map(([v, l]) => `<option value="${esc(v)}" ${String(d[name]) === String(v) ? 'selected' : ''}>${esc(l)}</option>`).join('')}</select>`;
    const chk = (name, label, hint = '') => `<label class="check"><input type="checkbox" name="${name}" ${d[name] ? 'checked' : ''}> <span>${label}${hint ? `<small>${hint}</small>` : ''}</span></label>`;
    const lineupSeats = d.lineup === 'custom' ? d.customSeats : LINEUPS[d.lineup]?.seats || [];
    const seatEditors = Array.from({ length: d.numOpponents }, (_, i) => `<label>Seat ${i + 1} ${sel(`seat-${i}`, PROFILE_KEYS.map((k) => [k, PROFILES[k].label])).replace(/selected/g, '').replace(`value="${lineupSeats[i]}"`, `value="${lineupSeats[i]}" selected`)}</label>`).join('');
    this.root.innerHTML = `<div class="settings-grid">
      <section class="panel"><header><h3>Table</h3><span class="muted small">Applied when you start a new game</span></header>
        <label>Your name <input name="heroName" value="${esc(d.heroName)}" maxlength="16"></label>
        <label>Opponents ${sel('numOpponents', [1, 2, 3, 4, 5, 6, 7, 8].map((n) => [n, `${n} (${n + 1}-handed${n === 1 ? ', heads-up' : n === 5 ? ', 6-max' : n === 8 ? ', full ring' : ''})`]))}</label>
        <label>Opponent lineup ${sel('lineup', [...Object.entries(LINEUPS).map(([k, v]) => [k, v.label]), ['custom', 'Custom (choose per seat)']])}</label>
        <div class="seat-editors">${seatEditors}</div>
        <label>Game ${sel('mode', [['cash', 'Cash game'], ['tournament', 'Sit & Go tournament (rising blinds)']])}</label>
        <label>Big blind <input type="number" name="bigBlind" min="2" step="2" value="${d.bigBlind}"></label>
        <label>Starting stack (big blinds) <input type="number" name="startingStackBB" min="10" max="500" value="${d.startingStackBB}"></label>
        ${d.mode === 'tournament' ? `<label>Hands per blind level <input type="number" name="levelHands" min="3" max="50" value="${d.levelHands}"></label>` : chk('autoRebuy', 'Auto-rebuy when I bust')}
        <div class="row-btns"><button class="btn primary" data-set="apply">Start new game with these settings</button><button class="btn ghost" data-set="defaults">Defaults</button></div>
      </section>
      <section class="panel"><header><h3>Training aids</h3><span class="muted small">Applied immediately</span></header>
        <label>Coach feedback ${sel('coachMode', [['all', 'After every decision'], ['mistakes', 'Only when I make a mistake'], ['off', 'Off (still recorded for review)']])}</label>
        ${chk('showHud', 'Show training HUD', 'Hand strength, equity vs estimated ranges, pot odds, outs')}
        ${chk('showOppStats', 'Show opponent HUD stats', 'VPIP / PFR / AF on each seat')}
        ${chk('revealProfiles', 'Reveal bot styles', 'Show each bot\'s personality — turn off to practise reading players')}
        ${chk('allInEquity', 'Show equities when all-in')}
        ${chk('showAllCards', 'Show all hole cards (practice mode)', 'See what bots hold while they play')}
      </section>
      <section class="panel"><header><h3>Display & pace</h3></header>
        <label>Bot speed ${sel('speed', [['instant', 'Instant'], ['fast', 'Fast'], ['normal', 'Normal'], ['realistic', 'Realistic (human think times)']])}</label>
        ${chk('autoDeal', 'Deal next hand automatically')}
        ${chk('fourColor', 'Four-colour deck', 'Clubs green, diamonds blue')}
      </section>
      <section class="panel"><header><h3>Data</h3></header>
        <p class="muted">Everything is stored locally in your browser. Bots remember your tendencies between sessions.</p>
        <button class="btn danger" data-set="wipe">Delete all saved data</button>
      </section>
    </div>`;
  }
}
