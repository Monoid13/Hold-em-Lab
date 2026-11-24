// No-limit Texas Hold'em table engine. UI-agnostic and deterministic given an RNG.

import { fullDeck, shuffle, secureRng } from './cards.js';
import { evaluate, bestFive, describeScore } from './evaluator.js';
import { positionLabels } from './ranges.js';

export const STREETS = ['preflop', 'flop', 'turn', 'river'];

export class Table {
  constructor({ seats = 6, smallBlind = 1, bigBlind = 2, ante = 0, rng = secureRng() } = {}) {
    this.numSeats = seats;
    this.seats = new Array(seats).fill(null);
    this.sb = smallBlind;
    this.bb = bigBlind;
    this.ante = ante;
    this.rng = rng;
    this.buttonSeat = -1;
    this.handNumber = 0;
    this.hand = null;
    this.listeners = [];
  }

  on(fn) {
    this.listeners.push(fn);
    return () => {
      this.listeners = this.listeners.filter((f) => f !== fn);
    };
  }

  emit(type, data = {}) {
    for (const fn of this.listeners) fn(type, data, this);
  }

  sit(seat, player) {
    if (this.seats[seat]) throw new Error(`Seat ${seat} taken`);
    this.seats[seat] = { sittingOut: false, ...player, seat };
    return this.seats[seat];
  }

  leave(seat) {
    this.seats[seat] = null;
  }

  setBlinds(sb, bb, ante = this.ante) {
    this.sb = sb;
    this.bb = bb;
    this.ante = ante;
  }

  eligibleSeats() {
    return this.seats.filter((p) => p && p.stack > 0 && !p.sittingOut).map((p) => p.seat);
  }

  /** Next seat clockwise after `from` satisfying `pred` (by seat index). */
  nextSeat(from, pred) {
    for (let i = 1; i <= this.numSeats; i++) {
      const s = (from + i) % this.numSeats;
      if (pred(s)) return s;
    }
    return -1;
  }

  startHand() {
    if (this.hand && !this.hand.finished) throw new Error('Hand in progress');
    const eligible = this.eligibleSeats();
    if (eligible.length < 2) throw new Error('Need at least two players with chips');
    const isElig = (s) => eligible.includes(s);
    this.buttonSeat = this.buttonSeat < 0 ? eligible[Math.floor(this.rng() * eligible.length)] : this.nextSeat(this.buttonSeat, isElig);

    const deck = shuffle(fullDeck(), this.rng);
    const order = [this.buttonSeat];
    for (let s = this.nextSeat(this.buttonSeat, isElig); s !== this.buttonSeat; s = this.nextSeat(s, isElig)) order.push(s);
    const labels = positionLabels(order.length);

    const players = {};
    order.forEach((seat, i) => {
      const p = this.seats[seat];
      players[seat] = {
        seat,
        id: p.id,
        name: p.name,
        startStack: p.stack,
        stack: p.stack,
        hole: [],
        bet: 0,
        contrib: 0,
        folded: false,
        allIn: false,
        hasActed: false,
        raiseLocked: false,
        position: labels[i],
        orderFromButton: i,
      };
    });

    const hand = {
      id: ++this.handNumber,
      buttonSeat: this.buttonSeat,
      sb: this.sb,
      bb: this.bb,
      ante: this.ante,
      deck,
      deckPos: 0,
      board: [],
      street: 'preflop',
      players,
      order, // seats clockwise starting at button
      pot: 0,
      currentBet: 0,
      minRaise: this.bb,
      actor: -1,
      lastAggressor: -1,
      raiseCount: 0, // preflop raises (open = 1)
      streetAggressor: { preflop: -1, flop: -1, turn: -1, river: -1 },
      log: [],
      finished: false,
      results: null,
      runoutFrom: -1,
      startedAt: Date.now(),
    };
    this.hand = hand;

    // antes
    if (this.ante > 0) {
      for (const seat of order) {
        const hp = players[seat];
        const a = Math.min(this.ante, hp.stack);
        hp.stack -= a;
        hp.contrib += a;
        hand.pot += a;
        if (hp.stack === 0) hp.allIn = true;
        hand.log.push({ street: 'preflop', seat, id: hp.id, type: 'ante', amount: a, blind: true });
      }
    }

    const headsUp = order.length === 2;
    const sbSeat = headsUp ? order[0] : order[1];
    const bbSeat = headsUp ? order[1] : order[2];
    hand.sbSeat = sbSeat;
    hand.bbSeat = bbSeat;
    this._post(players[sbSeat], this.sb, 'sb');
    this._post(players[bbSeat], this.bb, 'bb');
    hand.currentBet = this.bb;
    hand.minRaise = this.bb;

    // deal two cards each starting left of the button
    const dealOrder = headsUp ? [order[0], order[1]] : [...order.slice(1), order[0]];
    for (let r = 0; r < 2; r++) for (const seat of dealOrder) players[seat].hole.push(this._draw());

    const firstPre = headsUp ? order[0] : order[3 % order.length];
    hand.actor = this._firstToAct(firstPre, true);
    this.emit('handStart', { hand });
    if (hand.actor < 0) this._endStreet();
    return hand;
  }

  _draw() {
    const h = this.hand;
    return h.deck[h.deckPos++];
  }

  _post(hp, amount, kind) {
    const a = Math.min(amount, hp.stack);
    hp.stack -= a;
    hp.bet += a;
    hp.contrib += a;
    if (hp.stack === 0) hp.allIn = true;
    this.hand.log.push({ street: 'preflop', seat: hp.seat, id: hp.id, type: kind, amount: a, to: hp.bet, blind: true });
  }

  /** First player at/after `seat` (inclusive if `inclusive`) who can act. */
  _firstToAct(seat, inclusive) {
    const h = this.hand;
    const can = (s) => h.players[s] && !h.players[s].folded && !h.players[s].allIn;
    if (inclusive && can(seat)) return this._needsAction(seat) ? seat : this._nextActor(seat);
    return this._nextActor(seat);
  }

  _needsAction(seat) {
    const h = this.hand;
    const p = h.players[seat];
    return p && !p.folded && !p.allIn && (!p.hasActed || p.bet < h.currentBet);
  }

  _nextActor(from) {
    const h = this.hand;
    if (this._roundComplete()) return -1;
    const s = this.nextSeat(from, (x) => this._needsAction(x));
    return s;
  }

  livePlayers() {
    const h = this.hand;
    return h.order.map((s) => h.players[s]).filter((p) => !p.folded);
  }

  _roundComplete() {
    const h = this.hand;
    const live = this.livePlayers();
    if (live.length <= 1) return true;
    const canAct = live.filter((p) => !p.allIn);
    if (canAct.length === 0) return true;
    if (canAct.length === 1 && canAct[0].bet >= h.currentBet) {
      // Nobody left to respond to; only needs to act if facing a bet.
      return true;
    }
    return canAct.every((p) => p.hasActed && p.bet === h.currentBet);
  }

  potTotal() {
    const h = this.hand;
    let s = h.pot;
    for (const seat of h.order) s += h.players[seat].bet;
    return s;
  }

  legalActions(seat = this.hand?.actor) {
    const h = this.hand;
    if (!h || h.finished || seat !== h.actor) return null;
    const p = h.players[seat];
    const toCall = Math.min(Math.max(0, h.currentBet - p.bet), p.stack);
    const othersCanAct = this.livePlayers().some((o) => o.seat !== seat && !o.allIn);
    const maxTo = p.bet + p.stack;
    const canAggress = !p.raiseLocked && othersCanAct && p.stack > toCall;
    const minTo = Math.min(h.currentBet === 0 ? h.bb : h.currentBet + h.minRaise, maxTo);
    return {
      seat,
      toCall,
      canCheck: toCall === 0,
      canCall: toCall > 0,
      canFold: true,
      canBet: canAggress && h.currentBet === 0,
      canRaise: canAggress && h.currentBet > 0,
      minTo,
      maxTo,
      pot: this.potTotal(),
      currentBet: h.currentBet,
      myBet: p.bet,
      stack: p.stack,
    };
  }

  /**
   * Apply an action for the current actor.
   * action: { type: 'fold'|'check'|'call'|'bet'|'raise'|'allin', amount?: number (raise-to total) }
   */
  act(action) {
    const h = this.hand;
    if (!h || h.finished) throw new Error('No hand in progress');
    const seat = h.actor;
    const p = h.players[seat];
    const legal = this.legalActions(seat);
    let { type } = action;
    let to = action.amount;
    const potBefore = this.potTotal();
    const toCallBefore = legal.toCall;
    const raiseCountBefore = h.raiseCount;

    if (type === 'allin') {
      if (legal.canBet || legal.canRaise) {
        type = h.currentBet === 0 ? 'bet' : 'raise';
        to = legal.maxTo;
      } else type = legal.toCall > 0 ? 'call' : 'check';
    }
    if (type === 'call' && legal.toCall === 0) type = 'check';
    if (type === 'check' && legal.toCall > 0) throw new Error('Cannot check facing a bet');
    if (type === 'bet' && h.currentBet > 0) type = 'raise';
    if (type === 'raise' && h.currentBet === 0) type = 'bet';
    if ((type === 'bet' || type === 'raise') && !(legal.canBet || legal.canRaise)) {
      type = legal.toCall > 0 ? 'call' : 'check';
    }

    const entry = { street: h.street, seat, id: p.id, type, potBefore, toCall: toCallBefore, raiseCount: raiseCountBefore, stackBefore: p.stack, betBefore: p.bet };

    if (type === 'fold') {
      p.folded = true;
    } else if (type === 'check') {
      // nothing
    } else if (type === 'call') {
      const a = legal.toCall;
      p.stack -= a;
      p.bet += a;
      p.contrib += a;
      if (p.stack === 0) p.allIn = true;
      entry.amount = a;
      entry.to = p.bet;
    } else if (type === 'bet' || type === 'raise') {
      to = Math.round(Number(to));
      if (!Number.isFinite(to)) to = legal.minTo;
      to = Math.max(legal.minTo, Math.min(legal.maxTo, to));
      const added = to - p.bet;
      const raiseSize = to - h.currentBet;
      p.stack -= added;
      p.bet = to;
      p.contrib += added;
      if (p.stack === 0) p.allIn = true;
      const full = raiseSize >= h.minRaise;
      for (const s of h.order) {
        const o = h.players[s];
        if (s === seat || o.folded || o.allIn) continue;
        if (full) {
          o.hasActed = false;
          o.raiseLocked = false;
        } else if (o.hasActed) {
          o.hasActed = false;
          o.raiseLocked = true;
        }
      }
      if (full) h.minRaise = raiseSize;
      h.currentBet = Math.max(h.currentBet, to);
      h.lastAggressor = seat;
      h.streetAggressor[h.street] = seat;
      if (h.street === 'preflop') h.raiseCount++;
      entry.amount = added;
      entry.to = to;
      entry.fullRaise = full;
    } else {
      throw new Error(`Unknown action ${type}`);
    }
    entry.allIn = p.allIn;
    entry.potAfter = this.potTotal();
    p.hasActed = true;
    h.log.push(entry);
    this.emit('action', { entry, hand: h });
    this._advance(seat);
    return entry;
  }

  _advance(fromSeat) {
    const h = this.hand;
    const live = this.livePlayers();
    if (live.length === 1) {
      this._finishUncontested(live[0]);
      return;
    }
    if (this._roundComplete()) {
      this._endStreet();
      return;
    }
    h.actor = this._nextActor(fromSeat);
    if (h.actor < 0) this._endStreet();
  }

  _returnUncalled() {
    const h = this.hand;
    let top = null;
    let second = 0;
    for (const s of h.order) {
      const p = h.players[s];
      if (!top || p.bet > top.bet) {
        if (top) second = Math.max(second, top.bet);
        top = p;
      } else second = Math.max(second, p.bet);
    }
    if (top && top.bet > second) {
      const refund = top.bet - second;
      top.bet -= refund;
      top.contrib -= refund;
      top.stack += refund;
      if (top.stack > 0) top.allIn = false;
      h.log.push({ street: h.street, seat: top.seat, id: top.id, type: 'uncalled', amount: refund });
      this.emit('uncalled', { seat: top.seat, amount: refund });
    }
  }

  _collect() {
    const h = this.hand;
    for (const s of h.order) {
      h.pot += h.players[s].bet;
      h.players[s].bet = 0;
    }
  }

  _endStreet() {
    const h = this.hand;
    this._returnUncalled();
    this._collect();
    const live = this.livePlayers();
    if (live.length === 1) {
      this._finishUncontested(live[0]);
      return;
    }
    if (h.street === 'river') {
      this._showdown();
      return;
    }
    const canAct = live.filter((p) => !p.allIn);
    if (canAct.length <= 1) {
      h.runoutFrom = h.board.length;
      this.emit('runout', { hand: h, fromStreet: h.street });
      while (h.board.length < 5) this._dealStreet();
      this._showdown();
      return;
    }
    this._dealStreet();
    h.currentBet = 0;
    h.minRaise = h.bb;
    for (const s of h.order) {
      h.players[s].hasActed = false;
      h.players[s].raiseLocked = false;
    }
    h.actor = this._nextActor(h.buttonSeat);
    this.emit('street', { hand: h, street: h.street });
    if (h.actor < 0) this._endStreet();
  }

  _dealStreet() {
    const h = this.hand;
    const idx = STREETS.indexOf(h.street);
    h.street = STREETS[idx + 1];
    this._draw(); // burn
    const n = h.street === 'flop' ? 3 : 1;
    for (let i = 0; i < n; i++) h.board.push(this._draw());
  }

  _finishUncontested(winner) {
    const h = this.hand;
    this._returnUncalled();
    this._collect();
    const amount = h.pot;
    winner.stack += amount;
    h.results = {
      uncontested: true,
      showdown: [],
      pots: [{ amount, winners: [winner.seat], eligible: [winner.seat] }],
      winnings: { [winner.seat]: amount },
    };
    this._finish();
  }

  /** Build main pot and side pots from total contributions. */
  buildPots() {
    const h = this.hand;
    const all = h.order.map((s) => h.players[s]);
    const levels = [...new Set(all.filter((p) => !p.folded).map((p) => p.contrib))].sort((a, b) => a - b);
    const pots = [];
    let prev = 0;
    for (const lvl of levels) {
      let amount = 0;
      for (const p of all) amount += Math.max(0, Math.min(p.contrib, lvl) - Math.min(p.contrib, prev));
      const eligible = all.filter((p) => !p.folded && p.contrib >= lvl).map((p) => p.seat);
      if (amount > 0) pots.push({ amount, eligible });
      prev = lvl;
    }
    const total = all.reduce((s, p) => s + p.contrib, 0);
    const distributed = pots.reduce((s, p) => s + p.amount, 0);
    if (pots.length && total > distributed) pots[pots.length - 1].amount += total - distributed;
    return pots;
  }

  _showdown() {
    const h = this.hand;
    h.street = 'showdown';
    const live = this.livePlayers();
    const scores = {};
    const showdown = [];
    for (const p of live) {
      const cards = [...p.hole, ...h.board];
      const s = evaluate(cards);
      scores[p.seat] = s;
      const bf = bestFive(cards);
      showdown.push({ seat: p.seat, hole: p.hole.slice(), score: s, handName: describeScore(s), bestCards: bf.cards });
    }
    const pots = this.buildPots();
    const winnings = {};
    for (const pot of pots) {
      let best = -1;
      for (const s of pot.eligible) best = Math.max(best, scores[s]);
      const winners = pot.eligible.filter((s) => scores[s] === best);
      // Odd chips go to the first winner clockwise from the button.
      const leftOfButton = h.order.slice(1).concat(h.order[0]).filter((s) => winners.includes(s));
      const share = Math.floor(pot.amount / winners.length);
      let rem = pot.amount - share * winners.length;
      for (const s of leftOfButton) {
        let amt = share;
        if (rem > 0) {
          amt++;
          rem--;
        }
        winnings[s] = (winnings[s] || 0) + amt;
        h.players[s].stack += amt;
      }
      pot.winners = winners;
    }
    h.results = { uncontested: false, showdown, pots, winnings };
    this._finish();
  }

  _finish() {
    const h = this.hand;
    h.finished = true;
    h.actor = -1;
    for (const s of h.order) {
      const hp = h.players[s];
      const p = this.seats[s];
      if (p) p.stack = hp.stack;
      hp.net = hp.stack - hp.startStack;
    }
    this.emit('handEnd', { hand: h });
  }
}

/** A sanitized, read-only view of the hand for one seat (no peeking at other hole cards). */
export function buildView(table, seat) {
  const h = table.hand;
  const me = h.players[seat];
  const legal = table.legalActions(seat);
  const players = h.order.map((s) => {
    const p = h.players[s];
    return { seat: s, id: p.id, name: p.name, stack: p.stack, bet: p.bet, contrib: p.contrib, folded: p.folded, allIn: p.allIn, position: p.position, orderFromButton: p.orderFromButton, startStack: p.startStack };
  });
  return {
    handId: h.id,
    street: h.street,
    board: h.board.slice(),
    pot: h.pot,
    potTotal: table.potTotal(),
    currentBet: h.currentBet,
    minRaise: h.minRaise,
    bb: h.bb,
    sb: h.sb,
    buttonSeat: h.buttonSeat,
    raiseCount: h.raiseCount,
    lastAggressor: h.lastAggressor,
    streetAggressor: { ...h.streetAggressor },
    order: h.order.slice(),
    log: h.log.map((e) => ({ ...e })),
    players,
    me: { seat, id: me.id, hole: me.hole.slice(), stack: me.stack, bet: me.bet, position: me.position, orderFromButton: me.orderFromButton, startStack: me.startStack },
    legal,
    numPlayers: h.order.length,
  };
}
