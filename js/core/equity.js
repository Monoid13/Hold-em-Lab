// Equity calculation: Monte Carlo against weighted ranges / random hands, exact enumeration for drills.

import { evaluate } from './evaluator.js';
import { C1, C2, NUM_COMBOS } from './ranges.js';

const mark = new Int32Array(52);
let stamp = 1;

function nextStamp() {
  stamp++;
  if (stamp > 2e9) {
    mark.fill(0);
    stamp = 1;
  }
  return stamp;
}

/**
 * Build a sampler for a weighted combo range, skipping combos that use dead cards.
 * Returns null if the range is empty (caller treats it as a random hand).
 */
export function buildSampler(weights, dead) {
  const deadSet = new Uint8Array(52);
  for (const c of dead) deadSet[c] = 1;
  const idx = [];
  const cum = [];
  let total = 0;
  for (let k = 0; k < NUM_COMBOS; k++) {
    const w = weights[k];
    if (w <= 0 || deadSet[C1[k]] || deadSet[C2[k]]) continue;
    total += w;
    idx.push(k);
    cum.push(total);
  }
  if (total <= 0) return null;
  return { idx: Int16Array.from(idx), cum: Float64Array.from(cum), total };
}

function sampleCombo(s, rng) {
  const x = rng() * s.total;
  let lo = 0;
  let hi = s.cum.length - 1;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    if (s.cum[mid] > x) hi = mid;
    else lo = mid + 1;
  }
  return s.idx[lo];
}

/**
 * Monte Carlo equity.
 * @param {object} p
 * @param {number[]} p.hero hole cards
 * @param {number[]} p.board 0-5 board cards
 * @param {Array<{range?: Float32Array, cards?: number[]}>} p.opponents
 * @returns {{equity:number, win:number, tie:number, samples:number}}
 */
export function calcEquity({ hero, board = [], opponents, iterations = 1000, rng = Math.random }) {
  const dead = [...hero, ...board];
  for (const o of opponents) if (o.cards) dead.push(...o.cards);
  const samplers = opponents.map((o) => (o.cards ? null : o.range ? buildSampler(o.range, dead) : null));
  const nOpp = opponents.length;
  const need = 5 - board.length;
  const heroHand = new Array(7);
  heroHand[0] = hero[0];
  heroHand[1] = hero[1];
  for (let i = 0; i < board.length; i++) heroHand[2 + i] = board[i];
  const oppHands = opponents.map(() => new Array(7));
  for (let o = 0; o < nOpp; o++) for (let i = 0; i < board.length; i++) oppHands[o][2 + i] = board[i];

  let wins = 0;
  let ties = 0;
  let share = 0;
  let samples = 0;
  const maxTries = iterations * 3;

  for (let it = 0; it < maxTries && samples < iterations; it++) {
    const st = nextStamp();
    for (const c of dead) mark[c] = st;
    let ok = true;
    for (let o = 0; o < nOpp; o++) {
      const op = opponents[o];
      const h = oppHands[o];
      if (op.cards) {
        h[0] = op.cards[0];
        h[1] = op.cards[1];
        continue;
      }
      const s = samplers[o];
      let got = false;
      for (let t = 0; t < 25; t++) {
        let a;
        let b;
        if (s) {
          const k = sampleCombo(s, rng);
          a = C1[k];
          b = C2[k];
        } else {
          a = (rng() * 52) | 0;
          b = (rng() * 52) | 0;
          if (a === b) continue;
        }
        if (mark[a] === st || mark[b] === st) continue;
        mark[a] = st;
        mark[b] = st;
        h[0] = a;
        h[1] = b;
        got = true;
        break;
      }
      if (!got) {
        ok = false;
        break;
      }
    }
    if (!ok) continue;
    for (let k = 0; k < need; k++) {
      let c;
      do c = (rng() * 52) | 0;
      while (mark[c] === st);
      mark[c] = st;
      const pos = 2 + board.length + k;
      heroHand[pos] = c;
      for (let o = 0; o < nOpp; o++) oppHands[o][pos] = c;
    }
    const hs = evaluate(heroHand, 7);
    let best = -1;
    let bestCount = 0;
    for (let o = 0; o < nOpp; o++) {
      const s = evaluate(oppHands[o], 7);
      if (s > best) {
        best = s;
        bestCount = 1;
      } else if (s === best) bestCount++;
    }
    samples++;
    if (hs > best) {
      wins++;
      share += 1;
    } else if (hs === best) {
      ties++;
      share += 1 / (bestCount + 1);
    }
  }
  if (samples === 0) return { equity: 0.5, win: 0, tie: 0, samples: 0 };
  return { equity: share / samples, win: wins / samples, tie: ties / samples, samples };
}

/**
 * Equity of several known hands (all-in display, drills). Exact when <= 2 board cards remain,
 * otherwise Monte Carlo. Returns an array of equities, one per hand.
 */
export function multiwayKnownEquity(hands, board = [], iterations = 20000, rng = Math.random) {
  const n = hands.length;
  const need = 5 - board.length;
  const dead = new Uint8Array(52);
  for (const h of hands) for (const c of h) dead[c] = 1;
  for (const c of board) dead[c] = 1;
  const live = [];
  for (let c = 0; c < 52; c++) if (!dead[c]) live.push(c);
  const eq = new Float64Array(n);
  const bufs = hands.map((h) => {
    const a = new Array(7);
    a[0] = h[0];
    a[1] = h[1];
    for (let i = 0; i < board.length; i++) a[2 + i] = board[i];
    return a;
  });
  const scores = new Array(n);
  let count = 0;
  const settle = () => {
    let best = -1;
    let k = 0;
    for (let i = 0; i < n; i++) {
      scores[i] = evaluate(bufs[i], 7);
      if (scores[i] > best) {
        best = scores[i];
        k = 1;
      } else if (scores[i] === best) k++;
    }
    for (let i = 0; i < n; i++) if (scores[i] === best) eq[i] += 1 / k;
    count++;
  };
  const setCard = (pos, c) => {
    for (let i = 0; i < n; i++) bufs[i][pos] = c;
  };
  const base = 2 + board.length;
  if (need === 0) settle();
  else if (need === 1) {
    for (const c of live) {
      setCard(base, c);
      settle();
    }
  } else if (need === 2) {
    for (let i = 0; i < live.length; i++) {
      setCard(base, live[i]);
      for (let j = i + 1; j < live.length; j++) {
        setCard(base + 1, live[j]);
        settle();
      }
    }
  } else {
    const pool = live.slice();
    for (let it = 0; it < iterations; it++) {
      for (let k = 0; k < need; k++) {
        const j = k + Math.floor(rng() * (pool.length - k));
        const t = pool[k];
        pool[k] = pool[j];
        pool[j] = t;
        setCard(base + k, pool[k]);
      }
      settle();
    }
  }
  return Array.from(eq, (v) => v / count);
}
