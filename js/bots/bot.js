// Bot decision-making: wraps the strategy core with human-like behaviour —
// mixed strategies, tilt, bet-sizing tells, rounding to "human" amounts, and realistic think time.

import { buildView } from '../core/table.js';
import { preflopStrategy, postflopOptions, chooseWeights } from './strategy.js';
import { CHAT } from './profiles.js';

const clamp = (x, a, b) => (x < a ? a : x > b ? b : x);

export function sample(options, rng) {
  const total = options.reduce((s, o) => s + Math.max(0, o.w), 0);
  if (total <= 0) return options[options.length - 1];
  let x = rng() * total;
  for (const o of options) {
    x -= Math.max(0, o.w);
    if (x <= 0) return o;
  }
  return options[options.length - 1];
}

/** Round to amounts people actually bet. */
export function humanRound(amount, bb, rng, skill) {
  const jitter = 1 + (rng() - 0.5) * 0.12 * (1.2 - skill);
  let a = amount * jitter;
  const unit = a < 10 * bb ? Math.max(1, bb / 2) : a < 40 * bb ? bb : a < 150 * bb ? bb * 5 : bb * 10;
  a = Math.round(a / unit) * unit;
  return Math.max(1, Math.round(a));
}

export class BotMemory {
  constructor() {
    this.tilt = 0;
    this.lastChatHand = -10;
  }

  /** Update tilt after a hand. `badBeat` = lost an all-in as a big favourite. */
  afterHand({ netBB, badBeat, profile }) {
    this.tilt *= 0.88;
    const resist = profile.tiltResist ?? 0.7;
    if (badBeat) this.tilt += 0.45 * (1 - resist);
    if (netBB < -25) this.tilt += Math.min(0.4, (-netBB / 100) * (1 - resist));
    if (netBB > 30) this.tilt *= 0.7;
    this.tilt = clamp(this.tilt, 0, 1);
  }
}

/**
 * Decide an action for a bot seat.
 * @returns {{action:{type:string, amount?:number}, thinkMs:number, chat?:string, debug:object}}
 */
export function decideBot({ table, seat, profile, memory, tracker, rng = Math.random, iterationsScale = 1 }) {
  const view = buildView(table, seat);
  const { legal } = view;
  const tilt = memory ? memory.tilt : 0;
  const skill = profile.skill;
  const iterations = Math.round((200 + skill * 600) * iterationsScale);
  const P = view.potTotal;
  let choice;
  let difficulty = 0.3;
  let debug = {};
  let strength = 0.5;

  if (view.street === 'preflop') {
    const strat = preflopStrategy(view, tracker, profile, { rng, iterations, tilt });
    choice = sample(strat.options, rng);
    const ws = strat.options.map((o) => o.w).sort((a, b) => b - a);
    difficulty = 1 - (ws[0] - (ws[1] || 0));
    strength = 1 - strat.meta.pct;
    debug = { strat };
  } else {
    const res = postflopOptions(view, tracker, profile, { rng, iterations, tilt });
    let opts = res.options;
    strength = res.equity;
    // Slow-play monsters sometimes, mostly on dry boards.
    if (legal.toCall === 0 && res.equity > 0.85 && rng() < (profile.trap ?? 0)) {
      choice = opts.find((o) => o.type === 'check');
    }
    if (!choice) {
      const weighted = chooseWeights(opts, profile, P, tilt);
      // Sizing tells: weak players pick bet size by hand strength, not by EV.
      if (profile.sizingTell) {
        const aggr = weighted.filter((o) => o.type === 'bet' || o.type === 'raise');
        const aggrW = aggr.reduce((s, o) => s + o.w, 0);
        if (aggr.length > 1 && aggrW > 0) {
          const sorted = aggr.slice().sort((a, b) => a.amount - b.amount);
          const pick = res.equity > 0.72 ? sorted[Math.min(sorted.length - 1, Math.floor(sorted.length * 0.75))] : res.equity < 0.4 ? sorted[0] : sorted[Math.floor(sorted.length / 3)];
          for (const o of aggr) o.w = o === pick ? aggrW : 0;
        }
      }
      choice = sample(weighted, rng);
      const ws = weighted.map((o) => o.w).sort((a, b) => b - a);
      difficulty = 1 - (ws[0] - (ws[1] || 0));
      opts = weighted;
    }
    debug = { equity: res.equity, options: opts };
  }

  let action = { type: choice.type };
  if (choice.type === 'raise' || choice.type === 'bet') {
    let amt = choice.amount;
    if (amt < legal.maxTo) amt = humanRound(amt, view.bb, rng, skill);
    amt = clamp(amt, legal.minTo, legal.maxTo);
    if (amt >= legal.maxTo * 0.93) amt = legal.maxTo;
    action = { type: legal.canBet ? 'bet' : 'raise', amount: amt };
  } else if (choice.type === 'call') action = { type: legal.toCall > 0 ? 'call' : 'check' };
  else if (choice.type === 'check' && legal.toCall > 0) action = { type: 'fold' };
  else if (choice.type === 'fold' && legal.toCall === 0) action = { type: 'check' };

  const thinkMs = thinkTime({ view, action, difficulty, strength, profile, rng });
  let chat = null;
  if (tilt > 0.5 && (action.type === 'raise' || action.type === 'bet') && rng() < 0.15) chat = pick(CHAT.tiltPlay, rng);
  return { action, thinkMs, chat, debug };
}

function pick(list, rng) {
  return list[Math.floor(rng() * list.length)];
}

function thinkTime({ view, action, difficulty, strength, profile, rng }) {
  const potBB = view.potTotal / view.bb;
  let ms = view.street === 'preflop' ? 450 : 800;
  ms += difficulty * 2200;
  ms += Math.log1p(potBB) * 220;
  if (view.street === 'river') ms += 500;
  if (action.type === 'fold' && view.street === 'preflop' && difficulty < 0.2) ms = 250 + rng() * 400; // snap fold trash
  if ((action.type === 'raise' || action.type === 'bet') && action.amount >= view.legal.maxTo) ms += 900;
  // Hollywooding: some players tank with monsters.
  if (strength > 0.85 && rng() < (profile.hollywood ?? 0)) ms += 1500 + rng() * 2000;
  ms *= 0.7 + rng() * 0.7;
  return Math.round(clamp(ms, 200, 9000));
}

export function chatLine(kind, rng = Math.random) {
  const list = CHAT[kind];
  return list ? pick(list, rng) : null;
}
