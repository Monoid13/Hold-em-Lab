// Starting-hand classes (169), combos (1326), hand rankings, range notation and preflop charts.

import { RANKS } from './cards.js';

export const NUM_COMBOS = 1326;

/** 13x13 grid: row/col 0 = Ace. Upper-right triangle suited, lower-left offsuit. */
export const HAND_CLASSES = [];
export const CLASS_BY_NAME = new Map();
for (let row = 0; row < 13; row++) {
  for (let col = 0; col < 13; col++) {
    const rr = 12 - row;
    const cr = 12 - col;
    let name;
    let type;
    if (row === col) {
      name = RANKS[rr] + RANKS[rr];
      type = 'pair';
    } else if (col > row) {
      name = RANKS[rr] + RANKS[cr] + 's';
      type = 'suited';
    } else {
      name = RANKS[cr] + RANKS[rr] + 'o';
      type = 'offsuit';
    }
    const hi = Math.max(rr, cr);
    const lo = Math.min(rr, cr);
    const cls = { index: row * 13 + col, name, type, hi, lo, row, col, combos: type === 'pair' ? 6 : type === 'suited' ? 4 : 12 };
    HAND_CLASSES.push(cls);
    CLASS_BY_NAME.set(name, cls.index);
  }
}

export function classOf(c1, c2) {
  const r1 = c1 >> 2;
  const r2 = c2 >> 2;
  const hi = r1 > r2 ? r1 : r2;
  const lo = r1 > r2 ? r2 : r1;
  if (hi === lo) return (12 - hi) * 14;
  if ((c1 & 3) === (c2 & 3)) return (12 - hi) * 13 + (12 - lo);
  return (12 - lo) * 13 + (12 - hi);
}

export const classNameOf = (c1, c2) => HAND_CLASSES[classOf(c1, c2)].name;

// --- combos ---
export const C1 = new Uint8Array(NUM_COMBOS);
export const C2 = new Uint8Array(NUM_COMBOS);
export const COMBO_CLASS = new Uint8Array(NUM_COMBOS);
const COMBO_INDEX = new Int16Array(52 * 52).fill(-1);
{
  let k = 0;
  for (let a = 1; a < 52; a++) {
    for (let b = 0; b < a; b++) {
      C1[k] = a;
      C2[k] = b;
      COMBO_CLASS[k] = classOf(a, b);
      COMBO_INDEX[a * 52 + b] = k;
      COMBO_INDEX[b * 52 + a] = k;
      k++;
    }
  }
}
export const comboIndex = (a, b) => COMBO_INDEX[a * 52 + b];

/** Combo indices belonging to each class. */
export const CLASS_COMBOS = HAND_CLASSES.map(() => []);
for (let k = 0; k < NUM_COMBOS; k++) CLASS_COMBOS[COMBO_CLASS[k]].push(k);

// --- preflop hand ranking (strength + playability, roughly 6-max equilibrium ordering) ---
const RANKING_LIST = `
AA KK QQ AKs JJ AQs KQs AJs AKo TT
KJs ATs QJs AQo KTs 99 QTs JTs A9s AJo
KQo 88 A8s K9s T9s A7s Q9s J9s A5s ATo
KJo A6s A4s 77 QJo A3s K8s KTo T8s 98s
A2s J8s Q8s K7s 66 QTo JTo A9o 87s K6s
97s T7s K5s 55 76s Q7s K4s A8o 86s J7s
65s K3s Q6s 44 K9o 96s K2s Q5s 54s 75s
T9o A7o Q4s J9o Q9o A5o 33 T6s J6s 85s
Q3s 64s A6o 95s J5s Q2s A4o 22 74s 53s
J4s K8o T8o 98o J8o A3o T5s 84s 63s J3s
43s A2o K7o 94s J2s T4s 73s Q8o 52s 87o
T3s 97o 93s K6o 62s 42s T2s 83s 92s 76o
K5o 82s J7o 32s 86o T7o 72s Q7o K4o 65o
96o K3o 75o Q6o 54o J6o 85o K2o T6o 64o
Q5o 95o 53o 74o Q4o J5o 43o 84o Q3o J4o
63o T5o 94o Q2o J3o 73o 52o T4o J2o 93o
42o 83o T3o 62o 92o 32o T2o 82o 72o`
  .trim()
  .split(/\s+/);

export const RANKING = RANKING_LIST.map((n) => {
  const idx = CLASS_BY_NAME.get(n);
  if (idx === undefined) throw new Error(`Unknown hand in ranking: ${n}`);
  return idx;
});

/** PERCENTILE[class] = fraction of all combos at least as strong (inclusive). Lower = stronger. */
export const PERCENTILE = new Float32Array(169);
/** RANK_POS[class] = 0-based position in RANKING. */
export const RANK_POS = new Int16Array(169);
{
  let cum = 0;
  RANKING.forEach((idx, i) => {
    cum += HAND_CLASSES[idx].combos;
    PERCENTILE[idx] = cum / NUM_COMBOS;
    RANK_POS[idx] = i;
  });
}

// --- range notation ---
const R = (ch) => RANKS.indexOf(ch.toUpperCase());

function addClass(set, hi, lo, type) {
  if (hi === lo) {
    set.add((12 - hi) * 14);
    return;
  }
  if (type !== 'o') set.add((12 - hi) * 13 + (12 - lo));
  if (type !== 's') set.add((12 - lo) * 13 + (12 - hi));
}

/** Parse range notation such as "22+, A2s+, KTo+, A5s-A2s, T9s, AK" into a Set of class indices. */
export function parseRange(str) {
  const set = new Set();
  for (const raw of str.split(/[,\s]+/).filter(Boolean)) {
    const m = raw.match(/^([2-9TJQKA])([2-9TJQKA])([so]?)(\+?)(?:-([2-9TJQKA])([2-9TJQKA])([so]?))?$/i);
    if (!m) throw new Error(`Bad range token: ${raw}`);
    let a = R(m[1]);
    let b = R(m[2]);
    const type = m[3].toLowerCase();
    const plus = m[4] === '+';
    if (a === b) {
      if (plus) for (let r = a; r <= 12; r++) addClass(set, r, r);
      else if (m[5]) {
        const end = R(m[5]);
        for (let r = Math.min(a, end); r <= Math.max(a, end); r++) addClass(set, r, r);
      } else addClass(set, a, a);
      continue;
    }
    if (b > a) [a, b] = [b, a];
    if (plus) for (let k = b; k < a; k++) addClass(set, a, k, type);
    else if (m[5]) {
      const k2 = R(m[6]);
      for (let k = Math.min(b, k2); k <= Math.max(b, k2); k++) addClass(set, a, k, type);
    } else addClass(set, a, b, type);
  }
  return set;
}

export function rangeWidth(set) {
  let c = 0;
  for (const idx of set) c += HAND_CLASSES[idx].combos;
  return c / NUM_COMBOS;
}

// --- positions ---
export const POSITION_ORDER_9 = ['UTG', 'UTG+1', 'UTG+2', 'LJ', 'HJ', 'CO'];

/** Position labels for n players, in seat order starting from the button. */
export function positionLabels(n) {
  if (n === 2) return ['BTN', 'BB'];
  if (n === 3) return ['BTN', 'SB', 'BB'];
  const early = POSITION_ORDER_9.slice(POSITION_ORDER_9.length - (n - 3));
  return ['BTN', 'SB', 'BB', ...early];
}

// --- raise-first-in charts (100bb) ---
export const RFI_CHARTS = {
  UTG: '55+, A9s+, A5s, KTs+, QTs+, JTs, T9s, AJo+, KQo',
  'UTG+1': '44+, A8s+, A5s-A4s, KTs+, QTs+, JTs, T9s, 98s, ATo+, KQo',
  'UTG+2': '33+, A7s+, A5s-A4s, K9s+, QTs+, JTs, T9s, 98s, 87s, ATo+, KJo+',
  LJ: '22+, A2s+, K9s+, QTs+, JTs, T9s, 98s, 87s, 76s, 65s, ATo+, KJo+',
  HJ: '22+, A2s+, K7s+, Q9s+, J9s+, T8s+, 98s, 87s, 76s, 65s, 54s, ATo+, KTo+, QJo',
  CO: '22+, A2s+, K5s+, Q8s+, J8s+, T8s+, 97s+, 86s+, 75s+, 65s, 54s, A8o+, A5o, KTo+, QTo+, JTo',
  BTN: '22+, A2s+, K2s+, Q4s+, J6s+, T6s+, 96s+, 85s+, 74s+, 64s+, 53s+, 43s, A2o+, K8o+, Q9o+, J9o+, T8o+, 98o, 87o',
  SB: '22+, A2s+, K4s+, Q6s+, J7s+, T7s+, 96s+, 86s+, 75s+, 64s+, 54s, A5o+, K9o+, Q9o+, J9o+, T9o',
};
// Heads-up the button is the small blind and opens very wide.
export const HU_BTN_CHART = '22+, A2s+, K2s+, Q2s+, J2s+, T3s+, 95s+, 85s+, 74s+, 63s+, 53s+, 43s, A2o+, K4o+, Q6o+, J7o+, T7o+, 97o+, 87o, 76o, 65o';

export const RFI_SETS = {};
export const RFI_WIDTH = {};
for (const [pos, str] of Object.entries(RFI_CHARTS)) {
  RFI_SETS[pos] = parseRange(str);
  RFI_WIDTH[pos] = rangeWidth(RFI_SETS[pos]);
}
RFI_SETS.HU = parseRange(HU_BTN_CHART);
RFI_WIDTH.HU = rangeWidth(RFI_SETS.HU);

/** Which chart applies for an unopened pot from this position. */
export function rfiKey(position, numPlayers) {
  if (numPlayers === 2 && position === 'BTN') return 'HU';
  if (position === 'BTN' && numPlayers === 3) return 'BTN';
  return RFI_SETS[position] ? position : null;
}

/** Soft membership: 1 well inside the width, 0 well outside, linear blend around the edge. */
export function softIn(pct, width, softness = 0.025) {
  if (width <= 0) return 0;
  const s = softness * Math.max(0.3, Math.min(1.5, width / 0.2)) + 0.004;
  const v = (width - pct) / s + 0.5;
  return v <= 0 ? 0 : v >= 1 ? 1 : v;
}

/** Class weights for the strongest `width` fraction of hands. */
export function topClassWeights(width, softness = 0.02) {
  const w = new Float32Array(169);
  for (let i = 0; i < 169; i++) w[i] = softIn(PERCENTILE[i], width, softness);
  return w;
}

export function classWeightsToCombos(cw, out = new Float32Array(NUM_COMBOS)) {
  for (let k = 0; k < NUM_COMBOS; k++) out[k] = cw[COMBO_CLASS[k]];
  return out;
}

/** Fraction of all 1326 combos represented by a weighted combo range. */
export function comboWidth(weights) {
  let s = 0;
  for (let k = 0; k < NUM_COMBOS; k++) s += weights[k];
  return s / NUM_COMBOS;
}

/** Class-level view of a combo range (average weight per class) for grid displays. */
export function combosToClassWeights(weights) {
  const out = new Float32Array(169);
  for (let i = 0; i < 169; i++) {
    const list = CLASS_COMBOS[i];
    let s = 0;
    for (const k of list) s += weights[k];
    out[i] = s / list.length;
  }
  return out;
}
