// Fast 5-7 card hand evaluator. Returns an integer score; higher is better.
// Layout: category << 20 | r1 << 16 | r2 << 12 | r3 << 8 | r4 << 4 | r5

import { RANK_NAME, RANK_PLURAL } from './cards.js';

export const HIGH_CARD = 0;
export const PAIR = 1;
export const TWO_PAIR = 2;
export const TRIPS = 3;
export const STRAIGHT = 4;
export const FLUSH = 5;
export const FULL_HOUSE = 6;
export const QUADS = 7;
export const STRAIGHT_FLUSH = 8;

export const CATEGORY_NAMES = [
  'High Card', 'Pair', 'Two Pair', 'Three of a Kind', 'Straight',
  'Flush', 'Full House', 'Four of a Kind', 'Straight Flush',
];

export const STRAIGHT_HIGH = new Int8Array(8192).fill(-1);
export const TOP5 = new Int32Array(8192);
export const POPCOUNT = new Uint8Array(8192);
export const HIGHEST = new Int8Array(8192).fill(-1);

for (let m = 0; m < 8192; m++) {
  let pc = 0;
  let enc = 0;
  let taken = 0;
  for (let r = 12; r >= 0; r--) {
    if (m & (1 << r)) {
      pc++;
      if (HIGHEST[m] < 0) HIGHEST[m] = r;
      if (taken < 5) {
        enc |= r << (4 * (4 - taken));
        taken++;
      }
    }
  }
  POPCOUNT[m] = pc;
  TOP5[m] = enc;
  for (let h = 12; h >= 4; h--) {
    const sm = 0x1f << (h - 4);
    if ((m & sm) === sm) {
      STRAIGHT_HIGH[m] = h;
      break;
    }
  }
  if (STRAIGHT_HIGH[m] < 0 && (m & 0x100f) === 0x100f) STRAIGHT_HIGH[m] = 3; // wheel A-5
}

const suitMasks = new Int32Array(4);

/**
 * Evaluate the best 5-card hand from `n` cards of `cards` (5..7 cards).
 */
export function evaluate(cards, n = cards.length) {
  suitMasks[0] = 0;
  suitMasks[1] = 0;
  suitMasks[2] = 0;
  suitMasks[3] = 0;
  let m1 = 0;
  let m2 = 0;
  let m3 = 0;
  let m4 = 0;
  for (let i = 0; i < n; i++) {
    const c = cards[i];
    const b = 1 << (c >> 2);
    suitMasks[c & 3] |= b;
    m4 |= m3 & b;
    m3 |= m2 & b;
    m2 |= m1 & b;
    m1 |= b;
  }

  for (let s = 0; s < 4; s++) {
    const sm = suitMasks[s];
    if (POPCOUNT[sm] >= 5) {
      const sf = STRAIGHT_HIGH[sm];
      if (sf >= 0) return (STRAIGHT_FLUSH << 20) | (sf << 16);
      // With at most 7 cards a flush cannot coexist with quads or a full house.
      return (FLUSH << 20) | TOP5[sm];
    }
  }

  if (m4) {
    const q = HIGHEST[m4];
    const k = HIGHEST[m1 & ~(1 << q)];
    return (QUADS << 20) | (q << 16) | (Math.max(k, 0) << 12);
  }
  if (m3) {
    const t = HIGHEST[m3];
    const rest = m2 & ~(1 << t);
    if (rest) return (FULL_HOUSE << 20) | (t << 16) | (HIGHEST[rest] << 12);
  }
  const st = STRAIGHT_HIGH[m1];
  if (st >= 0) return (STRAIGHT << 20) | (st << 16);
  if (m3) {
    const t = HIGHEST[m3];
    return (TRIPS << 20) | (t << 16) | ((TOP5[m1 & ~(1 << t)] >> 4) & 0xff00);
  }
  if (m2) {
    const p1 = HIGHEST[m2];
    const rem = m2 & ~(1 << p1);
    if (rem) {
      const p2 = HIGHEST[rem];
      const k = HIGHEST[m1 & ~(1 << p1) & ~(1 << p2)];
      return (TWO_PAIR << 20) | (p1 << 16) | (p2 << 12) | (Math.max(k, 0) << 8);
    }
    return (PAIR << 20) | (p1 << 16) | ((TOP5[m1 & ~(1 << p1)] >> 4) & 0xfff0);
  }
  return TOP5[m1];
}

export const categoryOf = (score) => score >> 20;

const nib = (score, i) => (score >> (16 - 4 * i)) & 15;

export function describeScore(score) {
  const cat = categoryOf(score);
  const a = nib(score, 0);
  const b = nib(score, 1);
  switch (cat) {
    case HIGH_CARD: return `${RANK_NAME[a]} high`;
    case PAIR: return `Pair of ${RANK_PLURAL[a]}`;
    case TWO_PAIR: return `Two Pair, ${RANK_PLURAL[a]} and ${RANK_PLURAL[b]}`;
    case TRIPS: return `Three of a Kind, ${RANK_PLURAL[a]}`;
    case STRAIGHT: return `Straight, ${RANK_NAME[a]} high`;
    case FLUSH: return `Flush, ${RANK_NAME[a]} high`;
    case FULL_HOUSE: return `Full House, ${RANK_PLURAL[a]} full of ${RANK_PLURAL[b]}`;
    case QUADS: return `Four of a Kind, ${RANK_PLURAL[a]}`;
    case STRAIGHT_FLUSH: return a === 12 ? 'Royal Flush' : `Straight Flush, ${RANK_NAME[a]} high`;
    default: return 'Unknown';
  }
}

/** Returns the best five cards (for highlighting) and the score. */
export function bestFive(cards) {
  const n = cards.length;
  if (n <= 5) return { cards: cards.slice(), score: evaluate(cards) };
  let best = -1;
  let bestCards = null;
  const pick = new Array(5);
  const rec = (start, depth) => {
    if (depth === 5) {
      const s = evaluate(pick, 5);
      if (s > best) {
        best = s;
        bestCards = pick.slice();
      }
      return;
    }
    for (let i = start; i <= n - (5 - depth); i++) {
      pick[depth] = cards[i];
      rec(i + 1, depth + 1);
    }
  };
  rec(0, 0);
  return { cards: bestCards, score: best };
}
