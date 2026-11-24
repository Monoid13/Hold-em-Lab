// Hand histories with per-action snapshots for step-by-step replay and coach annotations.
import { load, save } from './storage.js';

const MAX = 150;

export function snapshot(table, label, seat = -1) {
  const h = table.hand;
  return {
    label,
    seat,
    street: h.street,
    board: h.board.slice(),
    pot: h.pot,
    players: Object.fromEntries(h.order.map((s) => {
      const p = h.players[s];
      return [s, { stack: p.stack, bet: p.bet, folded: p.folded, allIn: p.allIn }];
    })),
  };
}

export function createRecord(table, heroSeat, seatInfo) {
  const h = table.hand;
  return {
    id: `${Date.now().toString(36)}-${h.id}`,
    handNo: h.id,
    time: Date.now(),
    sb: h.sb,
    bb: h.bb,
    ante: h.ante,
    buttonSeat: h.buttonSeat,
    heroSeat,
    order: h.order.slice(),
    players: h.order.map((s) => {
      const p = h.players[s];
      return { seat: s, id: p.id, name: p.name, position: p.position, startStack: p.startStack, hole: p.hole.slice(), avatar: seatInfo[s]?.avatar, profile: seatInfo[s]?.profile };
    }),
    steps: [snapshot(table, 'Blinds posted')],
    actions: [],
    decisions: [],
    board: [],
    results: null,
  };
}

export function finalizeRecord(rec, table) {
  const h = table.hand;
  rec.board = h.board.slice();
  rec.runoutFrom = h.runoutFrom;
  rec.results = {
    uncontested: h.results.uncontested,
    winnings: h.results.winnings,
    pots: h.results.pots.map((p) => ({ amount: p.amount, winners: p.winners })),
    showdown: h.results.showdown.map((s) => ({ seat: s.seat, handName: s.handName, bestCards: s.bestCards })),
  };
  rec.nets = Object.fromEntries(h.order.map((s) => [s, h.players[s].net]));
  rec.steps.push({ ...snapshot(table, 'Result'), result: true });
  return rec;
}

export class HistoryStore {
  constructor() {
    this.hands = load('history', []);
  }

  add(rec) {
    this.hands.push(rec);
    if (this.hands.length > MAX) this.hands = this.hands.slice(-MAX);
    if (!save('history', this.hands)) {
      this.hands = this.hands.slice(-40);
      save('history', this.hands);
    }
  }

  clear() {
    this.hands = [];
    save('history', []);
  }
}
