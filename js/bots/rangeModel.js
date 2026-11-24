// Bayesian hand-range estimation from public actions. Every bot (and the coach) reads opponents
// through this model: it never sees hidden cards, only bets, positions and observed tendencies.

import { evaluate, STRAIGHT_HIGH, categoryOf, FLUSH } from '../core/evaluator.js';
import { C1, C2, COMBO_CLASS, NUM_COMBOS, PERCENTILE, HAND_CLASSES, comboWidth } from '../core/ranges.js';

const STREET_CARDS = { preflop: 0, flop: 3, turn: 4, river: 5, showdown: 5 };

/** Open-raise width relative to a player's overall PFR, by position. */
export const POS_FACTOR = { UTG: 0.55, 'UTG+1': 0.62, 'UTG+2': 0.7, LJ: 0.8, HJ: 0.95, CO: 1.25, BTN: 1.9, SB: 1.4, BB: 0.7 };

const clamp = (x, a, b) => (x < a ? a : x > b ? b : x);

function soft(pct, width, s = 0.03) {
  const v = (width - pct) / (s * Math.max(0.4, width / 0.2) + 0.006) + 0.5;
  return v <= 0 ? 0 : v >= 1 ? 1 : v;
}

const SUITED_PLAYABLE = new Float32Array(169);
for (const c of HAND_CLASSES) {
  if (c.type === 'suited' && (c.hi === 12 || c.hi - c.lo <= 2)) SUITED_PLAYABLE[c.index] = 1;
  if (c.type === 'pair') SUITED_PLAYABLE[c.index] = 0.6;
}

/**
 * Strength of each combo of a weighted range on a board, in [0,1] relative to that range.
 * Made hands use their percentile in the range; draws get credit for their equity.
 * Invalid combos (zero weight or blocked by the board/dead cards) are -1.
 */
export function computeStrengths(weights, board, dead = []) {
  const out = new Float32Array(NUM_COMBOS).fill(-1);
  const blocked = new Uint8Array(52);
  for (const c of board) blocked[c] = 1;
  for (const c of dead) blocked[c] = 1;
  if (board.length < 3) {
    for (let k = 0; k < NUM_COMBOS; k++) {
      if (weights[k] > 0 && !blocked[C1[k]] && !blocked[C2[k]]) out[k] = 1 - PERCENTILE[COMBO_CLASS[k]];
    }
    return out;
  }
  const n = board.length;
  const buf = new Array(n + 2);
  for (let i = 0; i < n; i++) buf[2 + i] = board[i];
  const idx = [];
  const scores = new Int32Array(NUM_COMBOS);
  let total = 0;
  for (let k = 0; k < NUM_COMBOS; k++) {
    const w = weights[k];
    if (w <= 0 || blocked[C1[k]] || blocked[C2[k]]) continue;
    buf[0] = C1[k];
    buf[1] = C2[k];
    scores[k] = evaluate(buf, n + 2);
    idx.push(k);
    total += w;
  }
  if (!idx.length) return out;
  idx.sort((a, b) => scores[a] - scores[b]);
  // weighted percentile with ties sharing a value
  let cum = 0;
  let i = 0;
  while (i < idx.length) {
    let j = i;
    let groupW = 0;
    while (j < idx.length && scores[idx[j]] === scores[idx[i]]) {
      groupW += weights[idx[j]];
      j++;
    }
    const pct = (cum + groupW / 2) / total;
    for (let t = i; t < j; t++) out[idx[t]] = pct;
    cum += groupW;
    i = j;
  }
  if (n < 5) {
    const bs = [0, 0, 0, 0];
    let bm = 0;
    for (const c of board) {
      bs[c & 3]++;
      bm |= 1 << (c >> 2);
    }
    for (const k of idx) {
      const a = C1[k];
      const b = C2[k];
      let draw = 0;
      if (categoryOf(scores[k]) < FLUSH) {
        const sa = a & 3;
        const sb = b & 3;
        const fa = bs[sa] + 1 + (sa === sb ? 1 : 0);
        const fb = bs[sb] + 1 + (sa === sb ? 1 : 0);
        if (fa === 4 || fb === 4) draw = n === 3 ? 0.62 : 0.5;
      }
      const m = bm | (1 << (a >> 2)) | (1 << (b >> 2));
      if (STRAIGHT_HIGH[m] < 0) {
        let outs = 0;
        for (let r = 0; r < 13; r++) {
          if (m & (1 << r)) continue;
          if (STRAIGHT_HIGH[m | (1 << r)] >= 0 && STRAIGHT_HIGH[bm | (1 << r)] < 0) outs++;
        }
        const sd = outs >= 2 ? (n === 3 ? 0.56 : 0.45) : outs === 1 ? (n === 3 ? 0.33 : 0.25) : 0;
        draw = draw > 0 && sd > 0 ? Math.min(0.85, draw + sd * 0.45) : Math.max(draw, sd);
      }
      if (draw > 0) {
        const made = out[k];
        out[k] = made > draw ? Math.min(1, made + draw * 0.15) : Math.min(1, draw + made * 0.15);
      }
    }
  }
  return out;
}

/** Keep the strongest `q` fraction of a range's weight mass. */
export function truncateTop(weights, strengths, q) {
  const out = new Float32Array(NUM_COMBOS);
  if (q >= 0.999) {
    for (let k = 0; k < NUM_COMBOS; k++) if (strengths[k] >= 0) out[k] = weights[k];
    return out;
  }
  const idx = [];
  let total = 0;
  for (let k = 0; k < NUM_COMBOS; k++) {
    if (strengths[k] >= 0 && weights[k] > 0) {
      idx.push(k);
      total += weights[k];
    }
  }
  idx.sort((a, b) => strengths[b] - strengths[a]);
  const target = Math.max(q, 0.02) * total;
  let cum = 0;
  for (const k of idx) {
    if (cum >= target) break;
    const w = weights[k];
    const take = Math.min(w, target - cum);
    out[k] = take;
    cum += take;
  }
  return out;
}

export class RangeTracker {
  /**
   * @param {object} hand live hand object from Table
   * @param {import('./stats.js').StatsBook} stats
   */
  constructor(hand, stats) {
    this.hand = hand;
    this.stats = stats;
    this.ranges = {};
    this.version = {};
    this.cache = new Map();
    for (const seat of hand.order) {
      this.ranges[seat] = new Float32Array(NUM_COMBOS).fill(1);
      this.version[seat] = 0;
    }
  }

  boardAt(street) {
    return this.hand.board.slice(0, STREET_CARDS[street] ?? 0);
  }

  getRange(seat) {
    return this.ranges[seat];
  }

  width(seat) {
    return comboWidth(this.ranges[seat]);
  }

  strengths(seat, board, dead = []) {
    const key = `${seat}|${this.version[seat]}|${board.join(',')}|${dead.join(',')}`;
    let s = this.cache.get(key);
    if (!s) {
      s = computeStrengths(this.ranges[seat], board, dead);
      if (this.cache.size > 64) this.cache.clear();
      this.cache.set(key, s);
    }
    return s;
  }

  _multiplyClass(seat, L) {
    const r = this.ranges[seat];
    let max = 0;
    for (let k = 0; k < NUM_COMBOS; k++) {
      r[k] *= L[COMBO_CLASS[k]];
      if (r[k] > max) max = r[k];
    }
    this._normalize(seat, max);
  }

  _normalize(seat, max) {
    const r = this.ranges[seat];
    if (max <= 1e-9) {
      // Contradictory evidence (e.g. a player far outside their profile): reset to a broad range.
      r.fill(0.2);
    } else if (max < 0.5) {
      const inv = 1 / max;
      for (let k = 0; k < NUM_COMBOS; k++) r[k] *= inv;
    }
    this.version[seat]++;
  }

  /** Update the acting player's range from a table log entry (call after Table.act). */
  observe(entry) {
    if (!entry || entry.blind || entry.type === 'uncalled' || entry.type === 'fold') return;
    if (entry.street === 'preflop') this._observePreflop(entry);
    else this._observePostflop(entry);
  }

  _observePreflop(e) {
    const hp = this.hand.players[e.seat];
    const id = hp.id;
    const st = this.stats;
    const vpip = st.rate(id, 'vpip');
    const pfr = st.rate(id, 'pfr');
    const tb = st.rate(id, 'threeBet');
    const posF = POS_FACTOR[hp.position] ?? 1;
    const headsUp = this.hand.order.length === 2;
    const hu = headsUp ? 1.8 : 1;
    const L = new Float32Array(169);
    const rc = e.raiseCount;
    if (e.type === 'raise') {
      let width;
      if (rc === 0) width = clamp(pfr * posF * hu, 0.03, 0.9);
      else if (rc === 1) width = clamp(tb * (posF > 1 ? 1.2 : 0.9) * hu, 0.02, 0.35);
      else width = clamp(tb * 0.4 * hu, 0.012, 0.1);
      if (e.allIn && e.to >= hp.startStack * 0.9 && rc === 0) width = clamp(width * 1.3, 0.04, 0.9);
      for (let i = 0; i < 169; i++) {
        let w = soft(PERCENTILE[i], width);
        if (rc >= 1 && PERCENTILE[i] > width) w = Math.max(w, SUITED_PLAYABLE[i] * 0.18 * clamp(tb / 0.07, 0.3, 2));
        L[i] = w;
      }
    } else if (e.type === 'call' || e.type === 'check') {
      if (e.type === 'check') {
        const top = clamp(pfr * 0.6, 0.03, 0.3);
        for (let i = 0; i < 169; i++) L[i] = PERCENTILE[i] <= top ? 0.35 : 1;
      } else if (rc === 0) {
        const hi = clamp(vpip * posF * 1.1 * hu, 0.05, 0.95);
        const lo = clamp(pfr * posF, 0.02, 0.5);
        for (let i = 0; i < 169; i++) L[i] = soft(PERCENTILE[i], hi) * (PERCENTILE[i] <= lo ? 0.3 : 1);
      } else if (rc === 1) {
        const isBB = hp.position === 'BB';
        const price = e.toCall / Math.max(1, e.potBefore + e.toCall);
        const bbBoost = isBB ? clamp(1.6 - price * 1.5, 1.0, 1.7) : 0.85;
        const hi = clamp(vpip * posF * bbBoost * hu, 0.04, 0.9);
        const lo = clamp(tb, 0.015, 0.2);
        for (let i = 0; i < 169; i++) L[i] = soft(PERCENTILE[i], hi) * (PERCENTILE[i] <= lo ? 0.35 : 1);
      } else {
        const hi = clamp(tb * 1.4 + 0.025, 0.03, 0.22);
        const lo = clamp(tb * 0.3, 0.01, 0.04);
        for (let i = 0; i < 169; i++) L[i] = soft(PERCENTILE[i], hi) * (PERCENTILE[i] <= lo ? 0.45 : 1);
      }
      if (e.allIn && e.type === 'call' && e.amount >= hp.startStack * 0.5) {
        const w = clamp(vpip * 0.45, 0.04, 0.5);
        for (let i = 0; i < 169; i++) L[i] = Math.min(L[i] + 0.0001, soft(PERCENTILE[i], w));
      }
    } else return;
    this._multiplyClass(e.seat, L);
  }

  _observePostflop(e) {
    const seat = e.seat;
    const hp = this.hand.players[seat];
    const id = hp.id;
    const board = this.boardAt(e.street);
    const s = this.strengths(seat, board);
    const r = this.ranges[seat];
    const agg = this.stats.rate(id, 'aggFreq');
    const foldVsBet = this.stats.rate(id, 'foldToBet');
    const bluffW = clamp(0.06 + agg * 0.55, 0.06, 0.6);
    const stationW = clamp(0.62 - foldVsBet, 0.04, 0.5);
    const big = e.amount / Math.max(1, e.potBefore) > 0.9 || e.allIn;
    let max = 0;
    for (let k = 0; k < NUM_COMBOS; k++) {
      const st = s[k];
      if (st < 0) {
        r[k] = 0;
        continue;
      }
      let L = 1;
      if (e.type === 'bet') {
        L = st >= 0.68 ? 1 : st >= 0.45 ? (big ? 0.18 : 0.38) : bluffW * (big ? 0.8 : 1);
      } else if (e.type === 'raise') {
        L = st >= 0.8 ? 1 : st >= 0.6 ? 0.3 : st >= 0.45 ? 0.12 : bluffW * 0.55;
      } else if (e.type === 'call') {
        L = st >= 0.92 ? 0.5 : st >= 0.35 ? 1 : st >= 0.2 ? 0.25 + stationW * 0.5 : stationW;
        if (e.allIn || big) L *= st >= 0.5 ? 1 : 0.4;
      } else if (e.type === 'check') {
        L = st >= 0.88 ? clamp(0.75 - agg, 0.25, 0.6) : 1;
      }
      r[k] *= L;
      if (r[k] > max) max = r[k];
    }
    this._normalize(seat, max);
  }
}
