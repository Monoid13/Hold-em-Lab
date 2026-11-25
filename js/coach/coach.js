// The coach analyses your spot with the same information you have (public actions + your cards),
// recommends an action, and grades what you did by estimated EV loss.

import { buildView } from '../core/table.js';
import { preflopStrategy, postflopOptions, preflopContext, isInPosition, effectiveStack } from '../bots/strategy.js';
import { COACH_PROFILE } from '../bots/profiles.js';
import { analyzeHand, outsToEquity } from '../core/handinfo.js';
import { calcEquity } from '../core/equity.js';

export const GRADES = {
  best: { label: 'Best', symbol: '★', score: 1 },
  good: { label: 'Good', symbol: '✓', score: 0.85 },
  inaccuracy: { label: 'Inaccuracy', symbol: '?!', score: 0.5 },
  mistake: { label: 'Mistake', symbol: '?', score: 0.2 },
  blunder: { label: 'Blunder', symbol: '??', score: 0 },
};

export const LEAK_TAGS = {
  'preflop-too-loose': { label: 'Playing too many hands preflop', tip: 'Tighten up, especially out of position. Use the Ranges tab to learn opening charts by position.' },
  'preflop-too-tight': { label: 'Folding playable hands preflop', tip: 'You are passing up profitable opens and defends. Late position and the big blind can play much wider.' },
  'open-limp': { label: 'Open-limping', tip: 'Entering the pot with a call forfeits initiative. Raise or fold when first in.' },
  'passive-preflop': { label: 'Flatting hands that should 3-bet', tip: 'Premium hands make more money as a 3-bet; build the pot when you are ahead.' },
  'loose-call': { label: 'Calling without the right price', tip: 'Compare your equity to the pot odds before calling. The HUD shows the equity you need.' },
  overfold: { label: 'Folding too often vs bets', tip: 'If you fold more than the bet size implies (MDF), opponents profit by bluffing you. Bots will notice.' },
  'missed-value': { label: 'Missing value bets', tip: 'When worse hands will call, bet. Checking strong hands lets opponents realize equity for free.' },
  'missed-bluff': { label: 'Missing good bluffs', tip: 'Weak hands that cannot win at showdown are often the best bluffs when villain folds enough.' },
  'bad-bluff': { label: 'Bluffing in bad spots', tip: 'Bluff when villain\'s range is weak or capped and they fold often. Avoid bluffing calling stations.' },
  passive: { label: 'Calling where raising is better', tip: 'Strong hands and good semi-bluffs often prefer raising for value and fold equity.' },
  sizing: { label: 'Suboptimal bet sizing', tip: 'Size up with strong hands vs. sticky players, size down for thin value and cheap bluffs.' },
  'bad-allin': { label: 'Poor all-in decisions', tip: 'In all-in spots compare equity vs the opponent\'s range with the price you are getting.' },
};

const pct = (x) => `${Math.round(x * 100)}%`;
const fmtChips = (x) => (x >= 0 ? '+' : '−') + Math.abs(Math.round(x));

export function actionLabel(a, view) {
  if (!a) return '';
  const t = a.type;
  if (t === 'fold') return 'Fold';
  if (t === 'check') return 'Check';
  if (t === 'call') return `Call ${a.amount ?? view?.legal?.toCall ?? ''}`.trim();
  if (t === 'bet' || t === 'raise') {
    const allIn = view && a.amount >= view.legal.maxTo;
    if (allIn) return 'All-in';
    if (t === 'bet' && view && view.potTotal > 0) {
      const frac = (a.amount - view.me.bet) / view.potTotal;
      return `Bet ${a.amount} (${sizeName(frac)})`;
    }
    return `${t === 'bet' ? 'Bet' : 'Raise to'} ${a.amount}`;
  }
  return t;
}

function sizeName(f) {
  const named = [[0.25, '¼ pot'], [0.33, '⅓ pot'], [0.5, '½ pot'], [0.66, '⅔ pot'], [0.75, '¾ pot'], [1, 'pot'], [1.5, '1.5× pot'], [2, '2× pot']];
  let best = named[0];
  for (const n of named) if (Math.abs(n[0] - f) < Math.abs(best[0] - f)) best = n;
  return Math.abs(best[0] - f) < 0.06 ? best[1] : `${Math.round(f * 100)}% pot`;
}

const normType = (t) => (t === 'bet' ? 'raise' : t);

/**
 * Analyse the hero's spot before they act.
 */
export function analyzeSpot({ table, seat, tracker, rng = Math.random, iterations = 1400 }) {
  const view = buildView(table, seat);
  const info = analyzeHand(view.me.hole, view.board);
  const opps = view.players.filter((p) => p.seat !== seat && !p.folded);
  const spot = {
    view,
    info,
    street: view.street,
    pot: view.potTotal,
    toCall: view.legal.toCall,
    needed: view.legal.toCall > 0 ? view.legal.toCall / (view.potTotal + view.legal.toCall) : 0,
    spr: effectiveStack(view) / Math.max(1, view.potTotal),
    ip: isInPosition(view),
    oppRanges: opps.map((o) => ({ seat: o.seat, name: o.name, width: tracker.width(o.seat) })),
    cardsToCome: view.street === 'flop' ? 2 : view.street === 'turn' ? 1 : 0,
  };
  if (view.street === 'preflop') {
    const strat = preflopStrategy(view, tracker, COACH_PROFILE, { rng, iterations });
    spot.preflop = strat;
    spot.equity = strat.meta.equity ?? calcEquity({ hero: view.me.hole, board: [], opponents: opps.map((o) => ({ range: tracker.getRange(o.seat) })), iterations: 800, rng }).equity;
    const best = strat.options.reduce((a, b) => (b.w > a.w ? b : a));
    spot.recommended = { type: best.type, amount: best.amount };
    spot.recommendedLabel = actionLabel(best, view);
    spot.mix = strat.options.filter((o) => o.w > 0.04).map((o) => ({ label: actionLabel(o, view), w: o.w }));
  } else {
    const res = postflopOptions(view, tracker, COACH_PROFILE, { rng, iterations });
    spot.postflop = res;
    spot.equity = res.equity;
    const best = res.options.reduce((a, b) => (b.ev > a.ev ? b : a));
    spot.recommended = { type: best.type, amount: best.amount };
    spot.recommendedLabel = actionLabel(best, view);
    spot.bestEV = best.ev;
  }
  return spot;
}

/** Grade the hero's chosen action against the analysis. */
export function gradeDecision(spot, action) {
  const { view } = spot;
  const bb = view.bb;
  const P = view.potTotal;
  const lines = [];
  const tags = [];
  let grade;
  let evLoss = null;
  let heroEV = null;
  let bestEV = null;
  const heroType = normType(action.type === 'allin' ? (view.legal.canRaise || view.legal.canBet ? 'raise' : 'call') : action.type);
  const heroAmount = action.type === 'allin' ? view.legal.maxTo : action.amount;
  const heroLabel = actionLabel({ ...action, type: action.type === 'allin' ? (view.legal.canBet ? 'bet' : 'raise') : action.type, amount: heroAmount ?? view.legal.toCall }, view);

  const { info } = spot;
  const handLine = info.detail ? `${info.name} — ${info.detail}` : info.name;
  lines.push(`Your hand: ${handLine}${info.draws.description.length ? ` + ${info.draws.description.join(', ')}` : ''}.`);

  if (spot.preflop) {
    const strat = spot.preflop;
    lines.push(...strat.notes);
    const byType = {};
    for (const o of strat.options) byType[normType(o.type)] = (byType[normType(o.type)] || 0) + o.w;
    if (heroType === 'check' && byType.check === undefined) byType.check = byType.call || 0;
    const wHero = byType[heroType] ?? 0;
    const bestType = Object.entries(byType).reduce((a, b) => (b[1] > a[1] ? b : a))[0];
    const bestW = byType[bestType];
    if (strat.evBased) {
      const evs = {};
      for (const o of strat.options) evs[normType(o.type)] = o.ev;
      bestEV = Math.max(...Object.values(evs));
      heroEV = evs[heroType] ?? evs.call ?? 0;
      evLoss = Math.max(0, bestEV - heroEV);
      lines.push(`EV (relative to folding): ${Object.entries(evs).map(([t, v]) => `${t} ${fmtChips(v)}`).join(', ')}.`);
    }
    if (wHero >= 0.45 || wHero >= bestW - 0.08) grade = heroType === bestType ? 'best' : 'good';
    else if (wHero >= 0.18) grade = 'good';
    else if (wHero >= 0.05) grade = 'inaccuracy';
    else grade = 'mistake';
    const hp = strat.meta.pct;
    if (heroType === 'fold' && hp <= 0.035 && view.legal.toCall < (view.me.stack + view.me.bet) * 0.5) grade = 'blunder';
    if (heroType !== 'fold' && heroType !== 'check' && hp > 0.8 && bestType === 'fold' && strat.meta.ctx.raises >= 1) grade = 'blunder';
    if (evLoss !== null && grade !== 'best') {
      const lossBB = evLoss / bb;
      grade = lossBB < 0.5 ? 'good' : lossBB < 2 ? 'inaccuracy' : lossBB < 6 ? 'mistake' : 'blunder';
      if (strat.evBased) tags.push('bad-allin');
    }
    if (grade !== 'best' && grade !== 'good') {
      const ctx = strat.meta.ctx;
      if (heroType === 'fold') tags.push('preflop-too-tight');
      else if (heroType === 'call' && ctx.raises === 0 && view.me.position !== 'BB' && view.me.position !== 'SB') tags.push('open-limp');
      else if (heroType === 'call' && bestType === 'raise') tags.push('passive-preflop');
      else if (bestType === 'fold') tags.push('preflop-too-loose');
    }
    const mix = spot.mix.map((m) => `${m.label} ${pct(m.w)}`).join(' · ');
    lines.push(`Coach strategy: ${mix}.`);
  } else {
    const res = spot.postflop;
    const opts = res.options;
    const best = opts.reduce((a, b) => (b.ev > a.ev ? b : a));
    bestEV = best.ev;
    let heroOpt;
    if (heroType === 'raise') {
      heroOpt = opts.find((o) => normType(o.type) === 'raise' && o.amount === Math.min(heroAmount, view.legal.maxTo));
      if (!heroOpt) heroOpt = res.evalAmount(heroAmount);
    } else heroOpt = opts.find((o) => normType(o.type) === heroType) || opts.find((o) => o.type === 'call') || opts[0];
    heroEV = heroOpt.ev;
    evLoss = Math.max(0, bestEV - heroEV);
    const rel = evLoss / Math.max(P, 4 * bb);
    const lossBB = evLoss / bb;
    const sameType = normType(best.type) === heroType;
    const sameSize = !sameType || heroType !== 'raise' || Math.abs(best.amount - heroAmount) <= Math.max(bb, best.amount * 0.15);
    if (sameType && sameSize) grade = 'best';
    else if (rel < 0.04 || lossBB < 0.35) grade = 'good';
    else if (rel < 0.14) grade = 'inaccuracy';
    else if (rel < 0.35) grade = 'mistake';
    else grade = 'blunder';

    const rangeTxt = spot.oppRanges.length > 1 ? 'their estimated ranges' : 'the estimated range';
    lines.push(`Equity vs ${rangeTxt}: ${pct(res.equity)}${spot.cardsToCome ? '' : ' (river: no more cards)'}.`);
    if (view.legal.toCall > 0) {
      lines.push(`Pot odds: call ${view.legal.toCall} to win ${P} → you need ${pct(spot.needed)} equity${res.real < 0.95 ? ` (out of position you realise only ~${pct(res.real)} of your equity)` : ''}.`);
    }
    if (spot.cardsToCome && info.outs.drawOuts > 0) {
      const e1 = outsToEquity(info.outs.drawOuts, 1);
      const e2 = outsToEquity(info.outs.drawOuts, spot.cardsToCome);
      lines.push(`Draw: ${info.outs.drawOuts} outs to a straight/flush ≈ ${pct(e1)} next card${spot.cardsToCome === 2 ? `, ${pct(e2)} by the river (rule of 2 & 4)` : ''}.`);
    }
    const optSummary = opts
      .slice()
      .sort((a, b) => b.ev - a.ev)
      .slice(0, 4)
      .map((o) => `${actionLabel(o, view)} ${fmtChips(o.ev)}`)
      .join(' · ');
    lines.push(`EV estimates (chips, relative to folding): ${optSummary}.`);
    if (normType(best.type) === 'raise') {
      lines.push(`Best: ${actionLabel(best, view)} — villain folds ~${pct(best.foldAll)}; when called you have ~${pct(best.eqCalled)}. A pure bluff needs ${pct((best.amount - view.me.bet) / (P + best.amount - view.me.bet))} folds to break even.`);
    } else lines.push(`Best: ${actionLabel(best, view)}.`);

    if (grade !== 'best' && grade !== 'good') {
      const bt = normType(best.type);
      if (heroType === 'fold' && (bt === 'call' || bt === 'raise')) tags.push('overfold');
      else if (heroType === 'call' && bt === 'fold') tags.push('loose-call');
      else if (heroType === 'check' && bt === 'raise') tags.push(best.eqCalled >= 0.5 ? 'missed-value' : 'missed-bluff');
      else if (heroType === 'raise' && bt === 'raise') tags.push('sizing');
      else if (heroType === 'raise' && (heroOpt.eqCalled ?? res.equity) < 0.45) tags.push('bad-bluff');
      else if (heroType === 'call' && bt === 'raise') tags.push(best.eqCalled >= 0.5 ? 'passive' : 'missed-bluff');
      else if (heroType === 'raise') tags.push('bad-bluff');
    }
    if (tags.includes('overfold')) lines.push('Folding here gives up a profitable call — the price is good enough for your equity.');
    if (tags.includes('loose-call')) lines.push('Calling here loses money long-term: your equity is below the price you are getting.');
    if (tags.includes('missed-value')) lines.push('You checked a hand that is ahead of villain\'s calling range — bet for value.');
  }

  return {
    grade,
    gradeInfo: GRADES[grade],
    evLoss,
    evLossBB: evLoss !== null ? evLoss / bb : null,
    heroEV,
    bestEV,
    heroLabel,
    bestLabel: spot.recommendedLabel,
    street: view.street,
    lines,
    tags,
    equity: spot.equity,
    needed: spot.needed,
  };
}
