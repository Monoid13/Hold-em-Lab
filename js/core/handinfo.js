// Human-readable hand analysis: relative made-hand strength, draws, outs and board texture.

import { evaluate, categoryOf, describeScore, STRAIGHT_HIGH, POPCOUNT, HIGHEST, PAIR, TWO_PAIR, TRIPS, STRAIGHT, HIGH_CARD } from './evaluator.js';
import { RANK_NAME, RANK_PLURAL } from './cards.js';

const rankMask = (cards) => cards.reduce((m, c) => m | (1 << (c >> 2)), 0);

export function drawInfo(hole, board) {
  const res = { flushDraw: false, nutFlushDraw: false, backdoorFlush: false, oesd: false, gutshot: false, straightOuts: 0, description: [] };
  if (board.length < 3 || board.length > 4) return res;
  const all = [...hole, ...board];
  const cur = evaluate(all);
  for (let s = 0; s < 4; s++) {
    const holeS = hole.filter((c) => (c & 3) === s);
    const boardS = board.filter((c) => (c & 3) === s);
    const n = holeS.length + boardS.length;
    if (holeS.length === 0) continue;
    if (n === 4 && categoryOf(cur) < 5) {
      res.flushDraw = true;
      const used = new Set(boardS.map((c) => c >> 2));
      let top = 12;
      while (used.has(top)) top--;
      res.nutFlushDraw = holeS.some((c) => c >> 2 === top);
    } else if (n === 3 && board.length === 3) res.backdoorFlush = true;
  }
  const m = rankMask(all);
  const bm = rankMask(board);
  if (STRAIGHT_HIGH[m] < 0) {
    let outsRanks = 0;
    for (let r = 0; r < 13; r++) {
      if (m & (1 << r)) continue;
      if (STRAIGHT_HIGH[m | (1 << r)] >= 0 && STRAIGHT_HIGH[bm | (1 << r)] < 0) outsRanks++;
    }
    if (outsRanks >= 2) res.oesd = true;
    else if (outsRanks === 1) res.gutshot = true;
    res.straightOuts = outsRanks;
  }
  if (res.flushDraw) res.description.push(res.nutFlushDraw ? 'nut flush draw' : 'flush draw');
  if (res.oesd) res.description.push('open-ended straight draw');
  else if (res.gutshot) res.description.push('gutshot');
  if (res.backdoorFlush && !res.flushDraw) res.description.push('backdoor flush draw');
  return res;
}

/**
 * Count outs. `drawOuts` = unseen cards that give a straight or better using a hole card,
 * `improveOuts` = unseen cards that improve the hand category using a hole card.
 */
export function countOuts(hole, board, extraDead = []) {
  if (board.length < 3 || board.length > 4) return { drawOuts: 0, improveOuts: 0, cards: [] };
  const all = [...hole, ...board];
  const used = new Set([...all, ...extraDead]);
  const cur = evaluate(all);
  const curCat = categoryOf(cur);
  let drawOuts = 0;
  let improveOuts = 0;
  const cards = [];
  for (let c = 0; c < 52; c++) {
    if (used.has(c)) continue;
    const next = evaluate([...all, c]);
    const nc = categoryOf(next);
    const boardNext = [...board, c];
    const boardScore = boardNext.length >= 5 ? evaluate(boardNext) : -1;
    const boardCat = boardScore >= 0 ? categoryOf(boardScore) : boardOnlyCategory(boardNext);
    const usesHole = nc > boardCat;
    if (nc > curCat && usesHole) {
      improveOuts++;
      if (nc >= STRAIGHT && curCat < STRAIGHT) {
        drawOuts++;
        cards.push(c);
      }
    }
  }
  return { drawOuts, improveOuts, cards };
}

function boardOnlyCategory(cards) {
  const counts = new Map();
  for (const c of cards) counts.set(c >> 2, (counts.get(c >> 2) || 0) + 1);
  const v = [...counts.values()].sort((a, b) => b - a);
  if (v[0] >= 4) return 7;
  if (v[0] === 3 && v[1] >= 2) return 6;
  if (v[0] === 3) return TRIPS;
  if (v[0] === 2 && v[1] === 2) return TWO_PAIR;
  if (v[0] === 2) return PAIR;
  return HIGH_CARD;
}

export function boardTexture(board) {
  if (board.length < 3) return null;
  const suits = [0, 0, 0, 0];
  for (const c of board) suits[c & 3]++;
  const maxSuit = Math.max(...suits);
  const ranks = board.map((c) => c >> 2).sort((a, b) => b - a);
  const paired = new Set(ranks).size < ranks.length;
  const m = rankMask(board);
  // connectivity: how many 5-rank windows contain >= 3 board ranks
  let windows = 0;
  for (let lo = -1; lo <= 8; lo++) {
    let cnt = 0;
    for (let r = lo; r < lo + 5; r++) {
      const rr = r === -1 ? 12 : r;
      if (m & (1 << rr)) cnt++;
    }
    if (cnt >= 3) windows++;
  }
  const monotone = maxSuit >= 3;
  const twoTone = maxSuit === 2;
  let wet = 0;
  if (monotone) wet += 0.45;
  else if (twoTone) wet += 0.25;
  wet += Math.min(0.45, windows * 0.12);
  if (paired) wet -= 0.1;
  const high = ranks[0];
  const labels = [];
  if (maxSuit === board.length) labels.push('monotone');
  else if (maxSuit >= 4) labels.push('four-flush');
  else if (maxSuit === 3) labels.push('flush possible');
  else labels.push(twoTone ? 'two-tone' : 'rainbow');
  if (paired) labels.push('paired');
  if (windows >= 2) labels.push('connected');
  if (high >= 10) labels.push(`${RANK_NAME[high]}-high`);
  else labels.push('low');
  return { paired, monotone, twoTone, rainbow: maxSuit === 1, connected: windows >= 2, wetness: Math.max(0, Math.min(1, wet)), high, labels, description: labels.join(', ') };
}

/** Relative description of a made hand, e.g. "Top pair, good kicker". */
export function madeHandDetail(hole, board) {
  const all = [...hole, ...board];
  const score = evaluate(all);
  const cat = categoryOf(score);
  if (board.length === 0) {
    const [a, b] = hole.map((c) => c >> 2).sort((x, y) => y - x);
    if (a === b) return { score, category: PAIR, name: `Pocket ${RANK_PLURAL[a]}`, detail: 'pocket pair' };
    const suited = (hole[0] & 3) === (hole[1] & 3);
    return { score, category: HIGH_CARD, name: `${RANK_NAME[a]}-${RANK_NAME[b]} ${suited ? 'suited' : 'offsuit'}`, detail: '' };
  }
  const name = describeScore(score);
  const boardRanks = [...new Set(board.map((c) => c >> 2))].sort((x, y) => y - x);
  const h = hole.map((c) => c >> 2).sort((x, y) => y - x);
  const boardScore = board.length >= 5 ? evaluate(board) : -1;
  if (boardScore === score) return { score, category: cat, name, detail: 'playing the board' };
  let detail = '';
  if (cat === PAIR) {
    if (h[0] === h[1]) {
      if (h[0] > boardRanks[0]) detail = 'overpair';
      else if (h[0] < boardRanks[boardRanks.length - 1]) detail = 'underpair';
      else detail = 'pocket pair below top card';
    } else {
      const pr = boardRanks.find((r) => h.includes(r));
      if (pr === undefined) detail = 'pair on board';
      else {
        const kicker = h[0] === pr ? h[1] : h[0];
        const idx = boardRanks.indexOf(pr);
        if (idx === 0) {
          const best = [12, 11, 10].find((r) => !boardRanks.includes(r));
          detail = kicker === best ? 'top pair, top kicker' : kicker >= 9 ? 'top pair, good kicker' : 'top pair, weak kicker';
        } else if (idx === boardRanks.length - 1) detail = 'bottom pair';
        else detail = 'middle pair';
      }
    }
  } else if (cat === TWO_PAIR) {
    const usesBoth = h[0] !== h[1] && boardRanks.includes(h[0]) && boardRanks.includes(h[1]);
    detail = usesBoth ? 'two pair (both cards)' : 'two pair';
  } else if (cat === TRIPS) {
    detail = h[0] === h[1] ? 'set' : 'trips';
  } else if (cat === HIGH_CARD) {
    const overs = h.filter((r) => r > boardRanks[0]).length;
    detail = overs === 2 ? 'two overcards' : overs === 1 ? 'one overcard' : 'no pair';
  }
  return { score, category: cat, name, detail };
}

export function analyzeHand(hole, board) {
  const made = madeHandDetail(hole, board);
  const draws = drawInfo(hole, board);
  const outs = countOuts(hole, board);
  return { ...made, draws, outs, texture: boardTexture(board) };
}

/** Rule of 2 and 4 approximation vs exact outs math. */
export function outsToEquity(outs, cardsToCome, unseen = 47) {
  if (cardsToCome <= 0 || outs <= 0) return 0;
  if (cardsToCome === 1) return outs / unseen;
  const miss = ((unseen - outs) / unseen) * ((unseen - 1 - outs) / (unseen - 1));
  return 1 - miss;
}

export { POPCOUNT, HIGHEST };
