// Training drills: question generators + graders. Pure logic; the UI lives in js/ui/drillsView.js.

import { fullDeck, shuffle } from '../core/cards.js';
import { evaluate, describeScore } from '../core/evaluator.js';
import { multiwayKnownEquity } from '../core/equity.js';
import { countOuts, madeHandDetail, outsToEquity } from '../core/handinfo.js';
import { classOf, HAND_CLASSES, PERCENTILE, positionLabels, RFI_WIDTH } from '../core/ranges.js';
import { preflopStrategy } from '../bots/strategy.js';
import { COACH_PROFILE } from '../bots/profiles.js';

const pick = (arr, rng) => arr[Math.floor(rng() * arr.length)];
const deal = (rng) => shuffle(fullDeck(), rng);
const pct = (x) => `${Math.round(x * 100)}%`;

// ---------- preflop ----------

/** Build a synthetic 6-max 100bb preflop view for drills. */
export function makePreflopView({ heroPos, openerPos = null, hole, stackBB = 100, bb = 2 }) {
  const n = 6;
  const labels = positionLabels(n); // index = orderFromButton = seat
  const seatOf = (p) => labels.indexOf(p);
  const actionOrder = [3, 4, 5, 0, 1, 2];
  const heroSeat = seatOf(heroPos);
  const openerSeat = openerPos ? seatOf(openerPos) : -1;
  const stack = stackBB * bb;
  const bets = { 1: bb / 2, 2: bb };
  const log = [
    { street: 'preflop', seat: 1, type: 'sb', amount: bb / 2, to: bb / 2, blind: true },
    { street: 'preflop', seat: 2, type: 'bb', amount: bb, to: bb, blind: true },
  ];
  const folded = new Set();
  const openTo = Math.round(bb * (openerPos === 'SB' ? 3 : 2.5));
  for (const s of actionOrder) {
    if (s === heroSeat) break;
    if (s === openerSeat) {
      log.push({ street: 'preflop', seat: s, type: 'raise', to: openTo, amount: openTo - (bets[s] || 0), raiseCount: 0, toCall: bb - (bets[s] || 0) });
      bets[s] = openTo;
    } else {
      log.push({ street: 'preflop', seat: s, type: 'fold' });
      folded.add(s);
    }
  }
  const currentBet = openerSeat >= 0 ? openTo : bb;
  const heroBet = bets[heroSeat] || 0;
  const potTotal = Object.values(bets).reduce((a, b) => a + b, 0);
  const players = labels.map((p, seat) => ({ seat, id: `s${seat}`, name: p, position: p, orderFromButton: seat, stack: stack - (bets[seat] || 0), bet: bets[seat] || 0, folded: folded.has(seat), allIn: false }));
  return {
    street: 'preflop',
    board: [],
    pot: 0,
    potTotal,
    currentBet,
    bb,
    sb: bb / 2,
    numPlayers: n,
    raiseCount: openerSeat >= 0 ? 1 : 0,
    log,
    players,
    streetAggressor: {},
    me: { seat: heroSeat, hole, stack: stack - heroBet, bet: heroBet, position: heroPos, orderFromButton: heroSeat },
    legal: { toCall: currentBet - heroBet, canCheck: currentBet === heroBet, canCall: currentBet > heroBet, canRaise: true, canBet: false, minTo: currentBet * 2, maxTo: stack, currentBet, potTotal },
  };
}

function randomHoleNear(rng, width) {
  // Bias toward hands near the decision boundary — that's where learning happens.
  for (let t = 0; t < 200; t++) {
    const d = deal(rng);
    const hole = [d[0], d[1]].sort((a, b) => b - a);
    const p = PERCENTILE[classOf(hole[0], hole[1])];
    if (rng() < 0.3 || (p > width * 0.45 && p < width * 2.2)) return hole;
  }
  const d = deal(rng);
  return [d[0], d[1]];
}

export const preflopDrill = {
  key: 'preflop',
  title: 'Preflop decisions',
  desc: '6-max, 100bb. Choose fold, call or raise. Answers come from the same charts and range logic the coach uses.',
  generate(rng) {
    const vsOpen = rng() < 0.5;
    let heroPos;
    let openerPos = null;
    if (!vsOpen) heroPos = pick(['LJ', 'HJ', 'CO', 'BTN', 'SB'], rng);
    else {
      const order = ['LJ', 'HJ', 'CO', 'BTN', 'SB', 'BB'];
      const oi = Math.floor(rng() * 5);
      openerPos = order[oi];
      heroPos = order[oi + 1 + Math.floor(rng() * (5 - oi))];
    }
    const width = vsOpen ? RFI_WIDTH[openerPos] * 0.6 : RFI_WIDTH[heroPos];
    const hole = randomHoleNear(rng, width);
    const view = makePreflopView({ heroPos, openerPos, hole });
    const strat = preflopStrategy(view, null, COACH_PROFILE, { rng, iterations: 400 });
    const byType = { fold: 0, call: 0, raise: 0 };
    for (const o of strat.options) byType[o.type === 'check' ? 'call' : o.type] += o.w;
    return {
      kind: 'preflop',
      heroPos,
      openerPos,
      hole,
      hand: HAND_CLASSES[classOf(hole[0], hole[1])].name,
      pct: PERCENTILE[classOf(hole[0], hole[1])],
      choices: vsOpen ? ['fold', 'call', 'raise'] : ['fold', 'raise'],
      choiceLabels: vsOpen ? { fold: 'Fold', call: 'Call', raise: '3-Bet' } : { fold: 'Fold', raise: 'Raise' },
      weights: byType,
      notes: strat.notes,
      prompt: vsOpen ? `${openerPos} opens to 2.5bb. You are in the ${heroPos} with` : `Folded to you in the ${heroPos}. You hold`,
    };
  },
  grade(q, answer) {
    const w = q.weights[answer] ?? 0;
    const best = Object.entries(q.weights).reduce((a, b) => (b[1] > a[1] ? b : a))[0];
    const correct = w >= 0.3 || answer === best;
    const mixed = Object.values(q.weights).filter((x) => x > 0.15).length > 1;
    const mix = Object.entries(q.weights).filter(([, x]) => x > 0.02).map(([k, x]) => `${q.choiceLabels[k] ?? k} ${pct(x)}`).join(' · ');
    return {
      correct,
      score: Math.round(Math.min(1, w / Math.max(0.01, q.weights[best])) * 100),
      explanation: [`${q.hand} is in the top ${pct(q.pct)} of starting hands.`, ...q.notes, `Strategy: ${mix}${mixed ? ' (a mixed spot — either is fine)' : ''}.`],
      best,
    };
  },
};

// ---------- equity estimation ----------

export const equityDrill = {
  key: 'equity',
  title: 'Equity estimation',
  desc: 'Estimate how often your hand wins against the opponent\'s exact hand. Builds the intuition behind every call.',
  generate(rng) {
    const d = deal(rng);
    const street = pick(['preflop', 'preflop', 'flop', 'flop', 'turn'], rng);
    const boardN = street === 'preflop' ? 0 : street === 'flop' ? 3 : 4;
    const hero = [d[0], d[1]];
    const villain = [d[2], d[3]];
    const board = d.slice(4, 4 + boardN);
    const [eh] = multiwayKnownEquity([hero, villain], board, 30000, rng);
    return { kind: 'equity', hero, villain, board, street, answer: eh };
  },
  grade(q, guess) {
    const err = Math.abs(guess / 100 - q.answer) * 100;
    const score = Math.max(0, Math.round(100 - err * 4));
    const lines = [`Actual equity: ${(q.answer * 100).toFixed(1)}% (you said ${guess}%). Off by ${err.toFixed(1)} points.`];
    if (q.board.length) {
      const o = countOuts(q.hero, q.board, q.villain);
      const mh = madeHandDetail(q.hero, q.board);
      lines.push(`You have ${mh.name}${mh.detail ? ` (${mh.detail})` : ''}${o.drawOuts ? ` with ${o.drawOuts} draw outs` : ''}.`);
    } else {
      lines.push('Benchmarks: pair vs two overcards ≈ 55/45, dominated hand (AK vs AQ) ≈ 70/30, pair vs smaller pair ≈ 80/20.');
    }
    return { correct: err <= 7, score, explanation: lines };
  },
};

// ---------- pot odds & bluff math ----------

const pot = (rng) => pick([20, 30, 40, 50, 60, 80, 100, 120, 150, 200], rng);

export const potOddsDrill = {
  key: 'potodds',
  title: 'Pot odds & bet math',
  desc: 'Required equity to call, break-even bluff frequency, minimum defence frequency and outs → equity.',
  generate(rng) {
    const type = pick(['call', 'bluff', 'mdf', 'outs'], rng);
    const P = pot(rng);
    const frac = pick([0.25, 0.33, 0.5, 0.66, 0.75, 1, 1.5], rng);
    const B = Math.round(P * frac);
    let prompt;
    let answer;
    let explain;
    if (type === 'call') {
      prompt = `The pot is ${P}. Villain bets ${B}. What equity do you need to call?`;
      answer = B / (P + 2 * B);
      explain = `You call ${B} to win ${P + B} (the pot plus the bet): ${B} / (${P} + ${B} + ${B}) = ${pct(answer)}.`;
    } else if (type === 'bluff') {
      prompt = `You bluff ${B} into a pot of ${P}. How often must villain fold for the bluff to break even?`;
      answer = B / (P + B);
      explain = `Risk ${B} to win ${P}: ${B} / (${P} + ${B}) = ${pct(answer)}.`;
    } else if (type === 'mdf') {
      prompt = `Villain bets ${B} into ${P}. What is your minimum defence frequency (call or raise) so their bluffs don't auto-profit?`;
      answer = P / (P + B);
      explain = `MDF = pot / (pot + bet) = ${P} / ${P + B} = ${pct(answer)}.`;
    } else {
      const outs = pick([4, 6, 8, 9, 12, 15], rng);
      const two = rng() < 0.5;
      prompt = `On the ${two ? 'flop (two cards to come, all-in)' : 'turn (one card to come)'} you have ${outs} outs. Roughly what is your equity?`;
      answer = outsToEquity(outs, two ? 2 : 1, two ? 47 : 46);
      explain = `Rule of ${two ? '4' : '2'}: ${outs} × ${two ? 4 : 2} ≈ ${outs * (two ? 4 : 2)}%. Exact: ${(answer * 100).toFixed(1)}%.`;
    }
    const opts = new Set([Math.round(answer * 100)]);
    const deltas = [-20, -12, -8, 8, 12, 20, 25, -25];
    shuffle(deltas, rng);
    for (const dlt of deltas) {
      if (opts.size >= 4) break;
      const v = Math.round(answer * 100) + dlt;
      if (v > 2 && v < 98) opts.add(v);
    }
    return { kind: 'potodds', type, prompt, answer, options: shuffle([...opts], rng), explain };
  },
  grade(q, choice) {
    const correct = choice === Math.round(q.answer * 100);
    return { correct, score: correct ? 100 : 0, explanation: [q.explain] };
  },
};

// ---------- outs ----------

export const outsDrill = {
  key: 'outs',
  title: 'Counting outs',
  desc: 'Count the cards that complete a straight or flush (or better) for you.',
  generate(rng) {
    for (let t = 0; t < 2000; t++) {
      const d = deal(rng);
      const hole = [d[0], d[1]];
      const board = d.slice(2, 2 + (rng() < 0.6 ? 3 : 4));
      const o = countOuts(hole, board);
      if (o.drawOuts >= 3 && evaluate([...hole, ...board]) >> 20 < 4) return { kind: 'outs', hole, board, answer: o.drawOuts, outCards: o.cards };
    }
    throw new Error('could not generate outs question');
  },
  grade(q, n) {
    const correct = Number(n) === q.answer;
    return { correct, score: correct ? 100 : Math.max(0, 100 - Math.abs(Number(n) - q.answer) * 25), explanation: [`${q.answer} outs. Rule of 2/4 → ≈ ${q.answer * (q.board.length === 3 ? 4 : 2)}% to hit by the river${q.board.length === 3 ? ' (two cards)' : ''}.`] };
  },
};

// ---------- showdown ----------

export const showdownDrill = {
  key: 'showdown',
  title: 'Read the showdown',
  desc: 'Who wins? Spot the best five-card hand fast — kickers, counterfeits and board plays included.',
  generate(rng) {
    const d = deal(rng);
    const nPlayers = rng() < 0.5 ? 2 : 3;
    const hands = [];
    for (let i = 0; i < nPlayers; i++) hands.push([d[i * 2], d[i * 2 + 1]]);
    const board = d.slice(nPlayers * 2, nPlayers * 2 + 5);
    const scores = hands.map((h) => evaluate([...h, ...board]));
    const best = Math.max(...scores);
    const winners = scores.map((s, i) => (s === best ? i : -1)).filter((i) => i >= 0);
    return { kind: 'showdown', hands, board, scores, names: scores.map(describeScore), answer: winners.length > 1 ? 'split' : winners[0] };
  },
  grade(q, choice) {
    const correct = choice === q.answer;
    return { correct, score: correct ? 100 : 0, explanation: q.hands.map((h, i) => `Player ${i + 1}: ${q.names[i]}${q.scores[i] === Math.max(...q.scores) ? ' ← best' : ''}`) };
  },
};

export const DRILLS = [preflopDrill, equityDrill, potOddsDrill, outsDrill, showdownDrill];
