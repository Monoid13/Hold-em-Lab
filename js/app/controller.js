// Game loop: deals hands, asks bots for decisions on human-like timers, waits for the hero,
// runs the coach, records history/stats and handles cash-game rebuys or tournament blinds.

import { Table } from '../core/table.js';
import { secureRng } from '../core/cards.js';
import { multiwayKnownEquity } from '../core/equity.js';
import { StatsBook } from '../bots/stats.js';
import { RangeTracker } from '../bots/rangeModel.js';
import { decideBot, BotMemory, chatLine } from '../bots/bot.js';
import { PROFILES, LINEUPS, BOT_NAMES } from '../bots/profiles.js';
import { analyzeSpot, gradeDecision, actionLabel } from '../coach/coach.js';
import { SPEED_FACTOR, BLIND_LEVELS } from './settings.js';
import { createRecord, finalizeRecord, snapshot } from './history.js';
import { load, save } from './storage.js';

export const HERO_ID = 'hero';
const HERO_SEAT = 0;

export class GameController {
  constructor({ settings, progress, history }) {
    this.settings = settings;
    this.progress = progress;
    this.history = history;
    this.stats = new StatsBook(load('stats', null));
    this.rng = secureRng();
    this.listeners = [];
    this.timers = new Set();
    this.paused = false;
    this.heroSeat = HERO_SEAT;
    this.state = this._blankState();
  }

  _blankState() {
    return { awaitingHero: false, spot: null, thinkingSeat: -1, flashes: {}, chats: {}, revealCount: 5, allInEquity: null, lastGrade: null, handOver: false, gameOver: null, heroBusted: false, level: 0, handsThisGame: 0, log: [] };
  }

  onChange(fn) {
    this.listeners.push(fn);
  }

  emit(reason = 'update') {
    for (const fn of this.listeners) fn(reason, this);
  }

  _later(fn, ms) {
    const id = setTimeout(() => {
      this.timers.delete(id);
      fn();
    }, Math.max(0, ms));
    this.timers.add(id);
    return id;
  }

  _clearTimers() {
    for (const id of this.timers) clearTimeout(id);
    this.timers.clear();
  }

  speed(ms) {
    return ms * (SPEED_FACTOR[this.settings.speed] ?? 0.75);
  }

  /** (Re)build the table from settings and start dealing. */
  newGame() {
    this._clearTimers();
    const s = this.settings;
    const nOpp = Math.max(1, Math.min(8, s.numOpponents));
    const seats = nOpp + 1;
    const bb = s.bigBlind;
    this.table = new Table({ seats, smallBlind: bb / 2, bigBlind: bb, rng: this.rng });
    this.startingStack = Math.round(s.startingStackBB * bb);
    this.table.sit(HERO_SEAT, { id: HERO_ID, name: s.heroName || 'You', stack: this.startingStack, isHero: true });
    const lineup = s.lineup === 'custom' ? s.customSeats : LINEUPS[s.lineup]?.seats || LINEUPS.mixed.seats;
    const names = BOT_NAMES.slice();
    // shuffle names so tables feel fresh, but keep names stable per profile for persistent reads
    for (let i = names.length - 1; i > 0; i--) {
      const j = Math.floor(this.rng() * (i + 1));
      [names[i], names[j]] = [names[j], names[i]];
    }
    this.bots = {};
    this.seatInfo = { [HERO_SEAT]: { avatar: '🙂', profile: null } };
    for (let i = 0; i < nOpp; i++) {
      const key = lineup[i % lineup.length] || 'tag';
      const profile = PROFILES[key] || PROFILES.tag;
      const [name, avatar] = names[i % names.length];
      const seat = i + 1;
      this.table.sit(seat, { id: `bot:${name}:${key}`, name, stack: this.startingStack });
      this.bots[seat] = { profile, memory: new BotMemory(), avatar, name };
      this.seatInfo[seat] = { avatar, profile: key };
    }
    this.state = this._blankState();
    if (s.mode === 'tournament') this._applyLevel(0);
    this.progress.data.sessions++;
    this.emit('newGame');
    const greeter = 1 + Math.floor(this.rng() * nOpp);
    this._chat(greeter, chatLine('greet', this.rng));
    this._later(() => this.deal(), 400);
  }

  _applyLevel(level) {
    const lv = BLIND_LEVELS[Math.min(level, BLIND_LEVELS.length - 1)];
    const unit = this.settings.bigBlind / 2;
    this.table.setBlinds(lv[0] * unit, lv[1] * unit, lv[2] * unit);
    this.state.level = level;
  }

  heroPlayer() {
    return this.table.seats[HERO_SEAT];
  }

  deal() {
    if (this.paused) return;
    const t = this.table;
    if (t.hand && !t.hand.finished) return;
    const s = this.settings;
    this.state.handOver = false;
    this.state.flashes = {};
    this.state.allInEquity = null;
    this.state.revealCount = 5;
    if (s.mode === 'cash') {
      for (const p of t.seats) {
        if (!p || p.isHero) continue;
        if (p.stack <= 0 || p.stack < this.startingStack * 0.25) {
          p.stack = this.startingStack; // bots rebuy / top up like real cash players
          p.sittingOut = false;
        }
      }
      const hero = this.heroPlayer();
      if (hero.stack <= 0) {
        if (s.autoRebuy) hero.stack = this.startingStack;
        else {
          this.state.heroBusted = true;
          this.emit('heroBusted');
          return;
        }
      }
    } else {
      // tournament
      for (const p of t.seats) if (p && p.stack <= 0) p.sittingOut = true;
      const alive = t.seats.filter((p) => p && p.stack > 0);
      const hero = this.heroPlayer();
      if (hero.stack <= 0) {
        this.state.gameOver = { won: false, place: alive.length + 1, of: t.seats.filter(Boolean).length };
        this.emit('gameOver');
        return;
      }
      if (alive.length === 1) {
        this.state.gameOver = { won: true, place: 1, of: t.seats.filter(Boolean).length };
        this.emit('gameOver');
        return;
      }
      const level = Math.floor(this.state.handsThisGame / Math.max(1, s.levelHands));
      if (level !== this.state.level) {
        this._applyLevel(level);
        this.state.levelUp = true;
      } else this.state.levelUp = false;
    }
    this.state.heroBusted = false;
    const hand = t.startHand();
    this.state.handsThisGame++;
    this.tracker = new RangeTracker(hand, this.stats);
    this.record = createRecord(t, HERO_SEAT, this.seatInfo);
    this.state.log = [`— Hand #${hand.id} · blinds ${hand.sb}/${hand.bb}${hand.ante ? ` ante ${hand.ante}` : ''} —`];
    this.emit('handStart');
    this._next();
  }

  _next() {
    const hand = this.table.hand;
    if (hand.finished) {
      this._handEnd();
      return;
    }
    const seat = hand.actor;
    if (seat === HERO_SEAT) {
      this.state.thinkingSeat = -1;
      this.state.awaitingHero = true;
      try {
        this.state.spot = analyzeSpot({ table: this.table, seat, tracker: this.tracker, rng: Math.random });
      } catch (err) {
        console.error('coach analysis failed', err);
        this.state.spot = null;
      }
      this.emit('heroTurn');
      return;
    }
    const bot = this.bots[seat];
    this.state.thinkingSeat = seat;
    this.state.awaitingHero = false;
    const d = decideBot({ table: this.table, seat, profile: bot.profile, memory: bot.memory, tracker: this.tracker, rng: Math.random });
    this.emit('botThinking');
    const handId = hand.id;
    this._later(() => {
      if (this.paused) {
        this._pendingBot = () => this._applyBot(handId, seat, d);
        return;
      }
      this._applyBot(handId, seat, d);
    }, this.speed(d.thinkMs));
  }

  _applyBot(handId, seat, d) {
    const hand = this.table.hand;
    if (!hand || hand.id !== handId || hand.actor !== seat || hand.finished) return;
    if (d.chat) this._chat(seat, d.chat);
    this._apply(seat, d.action);
  }

  heroAct(action) {
    if (!this.state.awaitingHero || this.paused) return false;
    const spot = this.state.spot;
    this.state.awaitingHero = false;
    if (spot) {
      try {
        const g = gradeDecision(spot, action);
        g.handId = this.table.hand.id;
        this.progress.recordDecision(g);
        this.record.decisions.push({ step: this.record.steps.length, street: g.street, grade: g.grade, heroLabel: g.heroLabel, bestLabel: g.bestLabel, evLossBB: g.evLossBB, lines: g.lines, tags: g.tags, equity: g.equity, needed: g.needed });
        this.state.lastGrade = g;
      } catch (err) {
        console.error('grading failed', err);
      }
    }
    this.state.spot = null;
    this._apply(HERO_SEAT, action);
    return true;
  }

  _apply(seat, action) {
    const t = this.table;
    const hand = t.hand;
    const prevStreet = hand.street;
    const viewBefore = { legal: t.legalActions(seat), potTotal: t.potTotal(), me: { bet: hand.players[seat].bet } };
    const entry = t.act(action);
    this.tracker.observe(entry);
    const label = entry.allIn && entry.type !== 'fold' && entry.type !== 'check' ? `All-in ${entry.to ?? ''}`.trim() : actionLabel({ type: entry.type, amount: entry.type === 'call' ? entry.amount : entry.to }, viewBefore);
    this.state.flashes = { ...this.state.flashes, [seat]: label };
    const name = hand.players[seat].name;
    this.state.log.push(`${name}: ${label}`);
    this.record.actions.push({ street: entry.street, seat, type: entry.type, to: entry.to, amount: entry.amount, allIn: entry.allIn, label });
    this.record.steps.push(snapshot(t, `${name}: ${label}`, seat));
    this.state.thinkingSeat = -1;
    if (!hand.finished && hand.street !== prevStreet) {
      this.state.flashes = {};
      this.state.log.push(`${hand.street[0].toUpperCase()}${hand.street.slice(1)}: ${hand.board.map(cardStr).join(' ')}`);
      this.emit('street');
      this._later(() => this._next(), this.speed(650));
      return;
    }
    this.emit('action');
    if (hand.finished) this._handEnd();
    else this._next();
  }

  _handEnd() {
    const t = this.table;
    const hand = t.hand;
    if (this._endingHand === hand) return;
    this._endingHand = hand;
    this.state.awaitingHero = false;
    this.state.thinkingSeat = -1;
    // All-in run-out: show equities and reveal the board card by card.
    if (hand.runoutFrom >= 0 && hand.runoutFrom < 5 && hand.results && !hand.results.uncontested) {
      const live = hand.results.showdown;
      try {
        const eqs = multiwayKnownEquity(live.map((s) => s.hole), hand.board.slice(0, hand.runoutFrom), 6000);
        this.state.allInEquity = Object.fromEntries(live.map((s, i) => [s.seat, eqs[i]]));
      } catch {
        this.state.allInEquity = null;
      }
      this.state.revealCount = hand.runoutFrom;
      this.emit('runout');
      const step = () => {
        const next = this.state.revealCount === 0 ? 3 : this.state.revealCount + 1;
        this.state.revealCount = Math.min(5, next);
        this.emit('runout');
        if (this.state.revealCount < 5) this._later(step, this.speed(1100));
        else this._later(() => this._settle(), this.speed(700));
      };
      this._later(step, this.speed(1300));
      return;
    }
    this._settle();
  }

  _settle() {
    const t = this.table;
    const hand = t.hand;
    this.state.revealCount = 5;
    this.stats.recordHand(hand);
    save('stats', this.stats.toJSON());
    const res = hand.results;
    for (const [seat, amt] of Object.entries(res.winnings)) {
      const hp = hand.players[seat];
      const desc = res.uncontested ? '' : ` with ${res.showdown.find((s) => s.seat === Number(seat))?.handName ?? ''}`;
      this.state.log.push(`${hp.id === HERO_ID ? 'You win' : `${hp.name} wins`} ${amt}${desc}`);
    }
    // bots: tilt + chat
    for (const seatStr of Object.keys(this.bots)) {
      const seat = Number(seatStr);
      const hp = hand.players[seat];
      if (!hp) continue;
      const bot = this.bots[seat];
      const netBB = hp.net / hand.bb;
      const eq = this.state.allInEquity?.[seat];
      const badBeat = eq !== undefined && eq >= 0.65 && hp.net < 0;
      bot.memory.afterHand({ netBB, badBeat, profile: bot.profile });
      if (badBeat && Math.random() < 0.7) this._chat(seat, chatLine('badBeat'));
      else if (netBB > 40 && Math.random() < bot.profile.chat * 3) this._chat(seat, chatLine('winBig'));
      else if (netBB < -40 && Math.random() < bot.profile.chat * 2) this._chat(seat, chatLine('loseBig'));
    }
    // hero progress
    const hero = hand.players[HERO_SEAT];
    if (hero) {
      const vpip = hand.log.some((e) => e.seat === HERO_SEAT && e.street === 'preflop' && (e.type === 'call' || e.type === 'raise'));
      const grades = this.record.decisions.map((d) => d.grade);
      this.progress.recordHand({ id: hand.id, t: Date.now(), netBB: hero.net / hand.bb, position: hero.position, vpip, showdown: !res.uncontested && res.showdown.some((s) => s.seat === HERO_SEAT), grades });
      this.progress.save();
    }
    finalizeRecord(this.record, t);
    this.history.add(this.record);
    this.state.handOver = true;
    this.emit('handOver');
    if (this.settings.autoDeal) {
      const showdown = !res.uncontested;
      this._later(() => this.deal(), this.speed(showdown ? 3200 : 1600) + (this.settings.speed === 'instant' ? 300 : 0));
    }
  }

  _chat(seat, text) {
    if (!text) return;
    this.state.chats = { ...this.state.chats, [seat]: { text, at: Date.now() } };
    this._later(() => {
      if (this.state.chats[seat]?.text === text) {
        const c = { ...this.state.chats };
        delete c[seat];
        this.state.chats = c;
        this.emit('chat');
      }
    }, 3800);
    this.emit('chat');
  }

  rebuyHero() {
    const hero = this.heroPlayer();
    if (hero.stack <= 0) hero.stack = this.startingStack;
    this.state.heroBusted = false;
    this.deal();
  }

  togglePause() {
    this.paused = !this.paused;
    if (!this.paused) {
      const pending = this._pendingBot;
      this._pendingBot = null;
      if (pending) pending();
      else if (this.table.hand?.finished && this.settings.autoDeal) this.deal();
    }
    this.emit('pause');
  }

  resetOpponentKnowledge() {
    this.stats.reset(HERO_ID);
    save('stats', this.stats.toJSON());
  }

  resetAllStats() {
    this.stats.reset();
    save('stats', this.stats.toJSON());
  }
}

function cardStr(c) {
  return '23456789TJQKA'[c >> 2] + '♣♦♥♠'[c & 3];
}
