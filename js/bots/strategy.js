// Decision core shared by the bots and the coach.
// Preflop: position-aware charts, range-vs-range thresholds and EV for stack-committing spots.
// Postflop: equity vs Bayesian-estimated ranges, fold equity from observed tendencies, EV per action.

import { calcEquity } from '../core/equity.js';
import { classOf, HAND_CLASSES, PERCENTILE, RFI_SETS, RFI_WIDTH, rfiKey, parseRange, softIn } from '../core/ranges.js';
import { truncateTop } from './rangeModel.js';

const clamp = (x, a, b) => (x < a ? a : x > b ? b : x);

export const BLUFF_3BET = parseRange('A5s-A2s, K9s-K6s, Q9s, J9s, T9s, T8s, 98s, 97s, 87s, 86s, 76s, 75s, 65s, 54s');
const SPECULATIVE = parseRange('22-99, A2s-A9s, KTs-K9s, QTs, Q9s, JTs, J9s, T9s, T8s, 98s, 87s, 76s, 65s, 54s');

const SHOVE_BASE = { UTG: 0.12, 'UTG+1': 0.13, 'UTG+2': 0.14, LJ: 0.16, HJ: 0.19, CO: 0.25, BTN: 0.36, SB: 0.48, BB: 0.5, HU: 0.62 };

/** Postflop action order key: higher acts later. */
export function postflopOrderKey(p, n) {
  return p.orderFromButton === 0 ? n : p.orderFromButton;
}

export function liveOpponents(view) {
  return view.players.filter((p) => p.seat !== view.me.seat && !p.folded);
}

export function isInPosition(view, vsSeat = null) {
  const n = view.numPlayers;
  const meKey = postflopOrderKey(view.me, n);
  const opps = liveOpponents(view).filter((p) => vsSeat === null || p.seat === vsSeat);
  return opps.every((o) => postflopOrderKey(o, n) < meKey);
}

export function effectiveStack(view) {
  const mine = view.me.stack + view.me.bet;
  let other = 0;
  for (const p of liveOpponents(view)) other = Math.max(other, p.stack + p.bet);
  return Math.min(mine, other);
}

export function preflopContext(view) {
  const seat = view.me.seat;
  let raises = 0;
  let limpers = 0;
  let callersAfterRaise = 0;
  let openerSeat = -1;
  let threeBettorSeat = -1;
  let lastRaiserSeat = -1;
  let iRaised = false;
  let lastRaiseTo = view.bb;
  for (const e of view.log) {
    if (e.street !== 'preflop' || e.blind || e.type === 'uncalled') continue;
    if (e.type === 'raise') {
      raises++;
      lastRaiserSeat = e.seat;
      lastRaiseTo = e.to;
      callersAfterRaise = 0;
      if (raises === 1) openerSeat = e.seat;
      if (raises === 2) threeBettorSeat = e.seat;
      if (e.seat === seat) iRaised = true;
    } else if (e.type === 'call') {
      if (raises === 0) limpers++;
      else callersAfterRaise++;
    }
  }
  return { raises, limpers, callersAfterRaise, openerSeat, threeBettorSeat, lastRaiserSeat, iRaised, lastRaiseTo };
}

const pos = (view, seat) => view.players.find((p) => p.seat === seat);

function sizeTo(view, to) {
  const { legal } = view;
  let t = Math.round(to);
  if (t >= (view.me.stack + view.me.bet) * 0.42) t = legal.maxTo; // commit: just shove
  return clamp(t, legal.minTo, legal.maxTo);
}

/**
 * Preflop strategy as a probability distribution over actions.
 * @returns {{options: Array<{type:string, amount?:number, w:number, ev?:number}>, notes: string[], meta: object}}
 */
export function preflopStrategy(view, tracker, profile, { rng = Math.random, iterations = 500, tilt = 0 } = {}) {
  const { me, legal, bb } = view;
  const cls = classOf(me.hole[0], me.hole[1]);
  const hc = HAND_CLASSES[cls];
  const pct = PERCENTILE[cls];
  const ctx = preflopContext(view);
  const n = view.numPlayers;
  const loose = profile.looseness * (1 + tilt * 0.5);
  const effBB = effectiveStack(view) / bb;
  const P = view.potTotal;
  const notes = [];
  const meta = { hand: hc.name, pct, position: me.position, effBB, ctx };
  const raiseOpt = (to) => ({ type: 'raise', amount: sizeTo(view, to) });
  const canRaise = legal.canRaise;

  // --- stack-committing decisions use EV against estimated ranges ---
  const myTotal = me.stack + me.bet;
  const committing = legal.toCall > 0 && (legal.toCall >= myTotal * 0.3 || (ctx.raises >= 1 && effBB <= 16) || ctx.raises >= 3);
  if (committing) return allInDecision(view, tracker, profile, { rng, iterations, ctx, pct, hc, meta, notes });

  if (ctx.raises === 0) {
    if (me.position === 'BB' && legal.toCall === 0) {
      const w = clamp(0.1 * loose * (1 + profile.aggression * 3), 0.03, 0.4);
      const pr = canRaise ? softIn(pct, w) : 0;
      meta.width = w;
      notes.push(`Option in the big blind: raise the top ~${Math.round(w * 100)}% vs limpers, otherwise check.`);
      return { options: [{ ...raiseOpt(bb * (3.5 + ctx.limpers)), w: pr }, { type: 'check', w: 1 - pr }], notes, meta };
    }
    const key = rfiKey(me.position, n) || me.position;
    if (effBB <= 13) {
      let w = clamp((SHOVE_BASE[n === 2 ? 'HU' : me.position] ?? 0.2) * Math.pow(10 / Math.max(effBB, 1), 0.7) * loose, 0.04, 0.95);
      if (ctx.limpers) w *= 0.7;
      const ps = softIn(pct, w);
      meta.width = w;
      notes.push(`Short stack (${effBB.toFixed(1)}bb): push/fold. Shove the top ~${Math.round(w * 100)}% from ${me.position}.`);
      return { options: [{ type: 'raise', amount: legal.maxTo, w: ps }, { type: 'fold', w: 1 - ps }], notes, meta };
    }
    const set = RFI_SETS[key];
    const width = RFI_WIDTH[key] ?? 0.2;
    const inChart = set ? set.has(cls) : pct <= width;
    let pRaise;
    let w = width;
    if (ctx.limpers > 0) {
      w = width * 0.75 * loose;
      pRaise = softIn(pct, w);
      notes.push(`${ctx.limpers} limper(s): isolate with a tighter range (~${Math.round(w * 100)}%).`);
    } else if (loose <= 1) {
      pRaise = inChart ? softIn(pct, Math.max(width * loose * 1.25, 0.02)) : 0;
    } else {
      pRaise = inChart ? 1 : softIn(pct, width * loose);
    }
    if (profile.pfrMult && profile.pfrMult < 1) pRaise *= softIn(pct, width * profile.pfrMult * 1.4);
    meta.width = w;
    meta.inChart = inChart;
    if (ctx.limpers === 0) notes.push(`${hc.name} is ${inChart ? '' : 'not '}in the standard ${key === 'HU' ? 'heads-up button' : me.position} opening range (~${Math.round(width * 100)}% of hands).`);
    let pLimp = 0;
    if (profile.limp > 0) {
      const lw = clamp(width * loose * 1.7, 0, 0.95);
      pLimp = (1 - pRaise) * softIn(pct, lw) * profile.limp;
    } else if (ctx.limpers > 0 && SPECULATIVE.has(cls) && isInPosition(view)) {
      pLimp = (1 - pRaise) * 0.6;
    } else if (me.position === 'SB' && n > 2 && (1 - pRaise) > 0 && SPECULATIVE.has(cls) && profile.skill < 0.9) {
      pLimp = (1 - pRaise) * 0.3;
    }
    const openBB = profile.openSize ?? (me.position === 'SB' ? 3 : n === 2 ? 2.5 : 2.3);
    return {
      options: [
        { ...raiseOpt(bb * (openBB + ctx.limpers)), w: canRaise ? pRaise : 0 },
        { type: 'call', amount: legal.toCall, w: pLimp + (canRaise ? 0 : pRaise) },
        { type: 'fold', w: Math.max(0, 1 - pRaise - pLimp) },
      ],
      notes,
      meta,
    };
  }

  if (ctx.raises === 1) {
    const opener = pos(view, ctx.openerSeat);
    const w = tracker ? clamp(tracker.width(ctx.openerSeat), 0.02, 1) : RFI_WIDTH[opener.position] || 0.2;
    const ip = isInPosition(view, ctx.openerSeat);
    const price = legal.toCall / (P + legal.toCall);
    const isBB = me.position === 'BB';
    const isSB = me.position === 'SB';
    let v3 = clamp(w * 0.22, 0.028, 0.11) * (profile.threeBetMult ?? 1) * (1 + tilt * 0.4);
    let cont;
    if (isBB) cont = w * (1 + 1.3 * (1 - w) ** 2) * clamp((0.27 / price) ** 0.6, 0.5, 1.6);
    else if (isSB) cont = w * 0.55;
    else cont = w * (ip ? 0.65 : 0.5);
    cont *= Math.pow(0.85, ctx.callersAfterRaise);
    if (effBB < 25) cont *= 0.7;
    cont = clamp(cont * loose * (1 + (profile.callBias ?? 0) * 4), 0.01, 0.95);
    const pVal = softIn(pct, v3);
    let pBluff = 0;
    if (BLUFF_3BET.has(cls) && pct > v3) pBluff = (profile.bluff3bet ?? 0.25) * (ip || isBB || isSB ? 1 : 0.5) * (w > 0.2 ? 1 : 0.35);
    let p3 = canRaise ? pVal + (1 - pVal) * pBluff : 0;
    let pCall = (1 - p3) * softIn(pct, cont);
    if (isSB && profile.skill >= 0.7) {
      // good players play 3-bet-or-fold from the small blind
      const shift = pCall * 0.6 * (pct < cont * 0.6 ? 1 : 0.3);
      pCall -= shift;
      p3 += canRaise ? shift : 0;
    }
    meta.openerRange = w;
    meta.value3bet = v3;
    meta.continue = cont;
    notes.push(`${opener.position} opened; estimated range ~${Math.round(w * 100)}% of hands.`);
    notes.push(`3-bet for value with the top ~${(v3 * 100).toFixed(1)}%, continue with ~${Math.round(cont * 100)}%${isBB ? ' (big blind gets a discount)' : ''}.`);
    if (pBluff > 0 && pct > v3) notes.push(`${hc.name} has good blockers/playability for an occasional 3-bet bluff.`);
    const mult = ip ? 3 : 3.8;
    const to = ctx.lastRaiseTo * mult + ctx.callersAfterRaise * ctx.lastRaiseTo;
    return {
      options: [
        { ...raiseOpt(effBB < 25 ? legal.maxTo : to), w: p3 },
        { type: 'call', amount: legal.toCall, w: pCall },
        { type: 'fold', w: Math.max(0, 1 - p3 - pCall) },
      ],
      notes,
      meta,
    };
  }

  // facing a 3-bet (raises === 2), not yet stack-committing
  const tbSeat = ctx.lastRaiserSeat;
  const w3 = tracker ? clamp(tracker.width(tbSeat), 0.01, 1) : 0.08;
  const ip = isInPosition(view, tbSeat);
  const myOpenW = ctx.iRaised && tracker ? tracker.width(me.seat) : 0.1;
  const v4 = clamp(w3 * 0.28, 0.018, 0.05) * (profile.threeBetMult ?? 1);
  const callW = ctx.iRaised ? Math.max(w3 * (ip ? 1.5 : 1.1), myOpenW * (ip ? 0.35 : 0.28)) : clamp(w3 * 0.6, 0.015, 0.06);
  const pVal = softIn(pct, v4);
  const pBluff = canRaise && (hc.name === 'A5s' || hc.name === 'A4s' || hc.name === 'A3s') ? (profile.bluff3bet ?? 0.25) * 0.5 : 0;
  const p4 = canRaise ? pVal + (1 - pVal) * pBluff : 0;
  const pCall = (1 - p4) * softIn(pct, clamp(callW * loose * (1 + (profile.callBias ?? 0) * 3), 0.01, 0.6));
  meta.threeBetRange = w3;
  notes.push(`Facing a 3-bet from a ~${Math.round(w3 * 100)}% range: 4-bet the top ~${(v4 * 100).toFixed(1)}%, call ~${Math.round(callW * 100)}%.`);
  const to = ctx.lastRaiseTo * (ip ? 2.3 : 2.6);
  return {
    options: [
      { ...raiseOpt(to), w: p4 },
      { type: 'call', amount: legal.toCall, w: pCall },
      { type: 'fold', w: Math.max(0, 1 - p4 - pCall) },
    ],
    notes,
    meta,
  };
}

/** EV-driven decision for big preflop spots: calling or making an all-in. */
function allInDecision(view, tracker, profile, { rng, iterations, ctx, pct, hc, meta, notes }) {
  const { me, legal } = view;
  const P = view.potTotal;
  const opps = liveOpponents(view);
  const ranges = opps.map((o) => ({ range: tracker ? tracker.getRange(o.seat) : null }));
  const eq = calcEquity({ hero: me.hole, board: [], opponents: ranges, iterations, rng }).equity;
  const callAmt = legal.toCall;
  const callAllIn = callAmt >= me.stack;
  const real = callAllIn ? 1 : 0.9;
  const callEV = eq * real * (P + callAmt) - callAmt;
  const options = [
    { type: 'fold', w: 0, ev: 0 },
    { type: 'call', amount: callAmt, w: 0, ev: callEV },
  ];
  if (legal.canRaise) {
    const to = legal.maxTo;
    const add = to - me.bet;
    const aggressor = pos(view, ctx.lastRaiserSeat);
    const contFrac = ctx.raises >= 3 ? 0.7 : ctx.raises === 2 ? 0.5 : 0.4;
    let foldAll = 1;
    const called = [];
    let potCalled = P + add;
    for (const o of opps) {
      const r = tracker ? tracker.getRange(o.seat) : null;
      if (o.allIn) {
        foldAll = 0;
        called.push({ range: r });
        continue;
      }
      const isAgg = aggressor && o.seat === aggressor.seat;
      const f = isAgg ? 1 - contFrac : 0.92;
      foldAll *= f;
      if (r && tracker) {
        const s = tracker.strengths(o.seat, [], me.hole);
        called.push({ range: truncateTop(r, s, 1 - f) });
      } else called.push({ range: null });
      potCalled += (1 - f) * Math.min(o.stack + o.bet, to) - (1 - f) * o.bet;
    }
    const eqC = calcEquity({ hero: me.hole, board: [], opponents: called, iterations: Math.round(iterations * 0.7), rng }).equity;
    const ev = foldAll * P + (1 - foldAll) * (eqC * potCalled - add);
    options.push({ type: 'raise', amount: to, w: 0, ev });
  }
  const needed = callAmt / (P + callAmt);
  meta.equity = eq;
  meta.needed = needed;
  notes.push(`Big decision: ${hc.name} has ~${Math.round(eq * 100)}% equity vs the estimated range(s); calling needs ${Math.round(needed * 100)}%.`);
  return { options: chooseWeights(options, profile, P, 0), notes, meta, evBased: true };
}

/**
 * Convert EVs to a mixed strategy via softmax; temperature falls as skill rises.
 * Softmax is applied first across action categories (fold / check / call / aggressive), then across
 * sizes within the aggressive category, so having many bet sizes does not inflate aggression.
 */
export function chooseWeights(options, profile, pot, tilt = 0) {
  const skill = clamp((profile.skill ?? 0.8) * (1 - tilt * 0.4), 0.05, 1);
  const tau = (0.012 + (1 - skill) * 0.2) * (1 + tilt);
  const cat = (o) => (o.type === 'bet' || o.type === 'raise' ? 'aggr' : o.type);
  const utils = options.map((o) => {
    let u = o.ev / Math.max(1, pot);
    if (o.type === 'call') u += profile.callBias ?? 0;
    if (o.type === 'fold') u += profile.foldBias ?? 0;
    if (cat(o) === 'aggr') u += (profile.aggression ?? 0) + tilt * 0.08;
    return u;
  });
  const groups = new Map();
  options.forEach((o, i) => {
    const c = cat(o);
    if (!groups.has(c)) groups.set(c, []);
    groups.get(c).push(i);
  });
  const catU = new Map([...groups].map(([c, idx]) => [c, Math.max(...idx.map((i) => utils[i]))]));
  const m = Math.max(...catU.values());
  const catEx = new Map([...catU].map(([c, u]) => [c, Math.exp((u - m) / tau)]));
  const catSum = [...catEx.values()].reduce((a, b) => a + b, 0);
  const sizeTau = tau * 0.6;
  return options.map((o, i) => {
    const c = cat(o);
    const idx = groups.get(c);
    const top = catU.get(c);
    const inner = idx.reduce((a, j) => a + Math.exp((utils[j] - top) / sizeTau), 0);
    const w = (catEx.get(c) / catSum) * (Math.exp((utils[i] - top) / sizeTau) / inner);
    return { ...o, w };
  });
}

export function realization(street, ip, nOpp) {
  if (street === 'river') return 1;
  const base = street === 'flop' ? (ip ? 0.97 : 0.86) : ip ? 0.99 : 0.92;
  return clamp(base - 0.04 * (nOpp - 1), 0.6, 1);
}

/**
 * Evaluate postflop options with EV estimates.
 * @returns {{options:Array, equity:number, ip:boolean, real:number, needed:number, evalAmount:function}}
 */
export function postflopOptions(view, tracker, profile, { rng = Math.random, iterations = 700, extraAmounts = [], tilt = 0 } = {}) {
  const { me, legal, board } = view;
  const P = view.potTotal;
  const opps = liveOpponents(view);
  const ip = isInPosition(view);
  const ranges = opps.map((o) => tracker.getRange(o.seat));
  const strengths = opps.map((o) => tracker.strengths(o.seat, board, me.hole));
  const skill = profile.skill ?? 0.8;
  const noiseScale = (1 - skill) * 0.07 + tilt * 0.03;
  const gauss = () => {
    let u = 0;
    for (let i = 0; i < 4; i++) u += rng();
    return (u - 2) * 0.866;
  };
  const percNoise = noiseScale > 0.001 ? gauss() * noiseScale : 0;

  const equity = calcEquity({ hero: me.hole, board, opponents: ranges.map((r) => ({ range: r })), iterations, rng }).equity;
  const perceived = clamp(equity + percNoise + (profile.overvalue ?? 0) * (equity > 0.35 ? 1 : 0), 0, 1);
  const real = realization(view.street, ip, opps.length);
  const options = [];
  const needed = legal.toCall > 0 ? legal.toCall / (P + legal.toCall) : 0;

  if (legal.toCall === 0) options.push({ type: 'check', ev: perceived * real * P, eq: perceived });
  else {
    options.push({ type: 'fold', ev: 0 });
    const callAmt = legal.toCall;
    const r = callAmt >= me.stack || view.street === 'river' ? 1 : real;
    options.push({ type: 'call', amount: callAmt, ev: perceived * r * (P + callAmt) - callAmt, eq: perceived });
  }

  let maxReach = 0;
  for (const o of opps) maxReach = Math.max(maxReach, o.stack + o.bet);
  const adapt = profile.adapt ?? skill;

  const evalAmount = (toRaw) => {
    let to = Math.min(Math.round(toRaw), legal.maxTo, maxReach);
    to = Math.max(to, Math.min(legal.minTo, legal.maxTo));
    const add = to - me.bet;
    let foldAll = 1;
    let anyAllIn = false;
    let callerAdd = 0;
    let callerProb = 0;
    const calledRanges = [];
    opps.forEach((o, i) => {
      if (o.allIn) {
        anyAllIn = true;
        calledRanges.push({ range: ranges[i] });
        return;
      }
      const facing = to - o.bet;
      let f = facing / (P + facing);
      const observed = tracker.stats.rate(o.id, 'foldToBet') / 0.42;
      const tend = 1 + (clamp(observed, 0.3, 2.2) - 1) * adapt;
      f *= tend;
      if (view.streetAggressor[view.street] === o.seat) f *= 0.8; // they bet this street: stronger, priced in
      if (view.street === 'river') f *= 1.08;
      // Even vs huge overbets players keep the top of their range; the cap rises only slowly with size.
      f = clamp(f, 0.02, clamp(0.86 + 0.03 * (facing / Math.max(1, P)), 0.9, 0.97));
      foldAll *= f;
      calledRanges.push({ range: truncateTop(ranges[i], strengths[i], 1 - f) });
      callerAdd += (1 - f) * Math.min(facing, o.stack);
      callerProb += 1 - f;
    });
    if (anyAllIn) foldAll = 0;
    const eqCalled = calcEquity({ hero: me.hole, board, opponents: calledRanges, iterations: Math.round(iterations * 0.6), rng }).equity;
    const eqC = clamp(eqCalled + percNoise, 0, 1);
    const condCallers = foldAll < 1 ? Math.max(1, callerProb / (1 - foldAll)) : 1;
    const avgCallerAdd = callerProb > 0 ? callerAdd / callerProb : 0;
    const potCalled = P + add + condCallers * avgCallerAdd;
    const r = to >= legal.maxTo || view.street === 'river' ? 1 : Math.min(1, real + 0.05);
    // Risk of being raised off our equity (not possible when all-in): weak bets lose the bet.
    const canBeRaised = to < legal.maxTo && opps.some((o) => !o.allIn && o.stack > to - o.bet);
    const raiseRisk = canBeRaised && eqC < 0.55 ? 0.14 * (1 - foldAll) * (view.street === 'river' ? 0.7 : 1) : 0;
    const showdownPart = eqC * r * potCalled - add;
    const ev = foldAll * P + (1 - foldAll) * ((1 - raiseRisk) * showdownPart + raiseRisk * -add);
    return { type: legal.canBet ? 'bet' : 'raise', amount: to, ev, foldAll, eqCalled: eqC, sizeFrac: add / Math.max(1, P) };
  };

  if (legal.canBet || legal.canRaise) {
    const amounts = new Set();
    if (legal.canBet) {
      const fr = [0.33, 0.5, 0.75, 1.0];
      if (view.street === 'river' && skill >= 0.7) fr.push(1.5);
      for (const f of fr) amounts.add(Math.round(me.bet + f * P));
    } else {
      amounts.add(Math.round(legal.currentBet * 2.5));
      amounts.add(Math.round(legal.currentBet + (P + legal.toCall)));
    }
    const spr = (me.stack + me.bet) / Math.max(1, P);
    const shoveAdd = legal.maxTo - me.bet;
    if (spr < 3 || shoveAdd <= 2 * (P + legal.toCall)) amounts.add(legal.maxTo);
    for (const a of extraAmounts) amounts.add(Math.round(a));
    const seen = new Set();
    for (const a of [...amounts].sort((x, y) => x - y)) {
      const opt = evalAmount(a);
      if (seen.has(opt.amount)) continue;
      seen.add(opt.amount);
      options.push(opt);
    }
  }
  return { options, equity, perceived, ip, real, needed, evalAmount, numOpp: opps.length };
}
