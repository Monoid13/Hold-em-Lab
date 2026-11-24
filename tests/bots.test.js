import { test } from 'node:test';
import assert from 'node:assert/strict';
import { Table } from '../js/core/table.js';
import { mulberry32, parseCards } from '../js/core/cards.js';
import { parseRange, RANKING, PERCENTILE, CLASS_BY_NAME, RFI_WIDTH, classOf, comboIndex, NUM_COMBOS } from '../js/core/ranges.js';
import { calcEquity } from '../js/core/equity.js';
import { analyzeHand, countOuts } from '../js/core/handinfo.js';
import { StatsBook } from '../js/bots/stats.js';
import { RangeTracker } from '../js/bots/rangeModel.js';
import { decideBot, BotMemory } from '../js/bots/bot.js';
import { PROFILES, PROFILE_KEYS } from '../js/bots/profiles.js';
import { analyzeSpot, gradeDecision } from '../js/coach/coach.js';
import { DRILLS } from '../js/train/drills.js';

test('hand ranking covers all 169 classes exactly once', () => {
  assert.equal(RANKING.length, 169);
  assert.equal(new Set(RANKING).size, 169);
  assert.ok(Math.abs(PERCENTILE[RANKING[168]] - 1) < 1e-6);
  assert.ok(PERCENTILE[CLASS_BY_NAME.get('AA')] < PERCENTILE[CLASS_BY_NAME.get('72o')]);
});

test('range notation parses', () => {
  assert.equal(parseRange('22+').size, 13);
  assert.equal(parseRange('A2s+').size, 12);
  assert.equal(parseRange('A5s-A2s').size, 4);
  assert.equal(parseRange('AK').size, 2);
  assert.ok(parseRange('KTo+').has(CLASS_BY_NAME.get('KQo')));
});

test('opening ranges widen toward the button', () => {
  assert.ok(RFI_WIDTH.UTG < RFI_WIDTH.HJ && RFI_WIDTH.HJ < RFI_WIDTH.CO && RFI_WIDTH.CO < RFI_WIDTH.BTN);
});

test('equity against a narrow range is lower than against random', () => {
  const hero = parseCards('Ah Qd');
  const tight = new Float32Array(NUM_COMBOS);
  for (const name of ['AA', 'KK', 'QQ', 'AKs', 'AKo']) {
    for (let a = 0; a < 52; a++) for (let b = 0; b < a; b++) if (classOf(a, b) === CLASS_BY_NAME.get(name)) tight[comboIndex(a, b)] = 1;
  }
  const rng = mulberry32(1);
  const vsRandom = calcEquity({ hero, opponents: [{}], iterations: 6000, rng }).equity;
  const vsTight = calcEquity({ hero, opponents: [{ range: tight }], iterations: 6000, rng }).equity;
  assert.ok(vsRandom > 0.6 && vsTight < 0.3, `${vsRandom} ${vsTight}`);
});

test('hand analysis recognises draws and outs', () => {
  const a = analyzeHand(parseCards('9c 8c'), parseCards('Tc Jh 2c'));
  assert.ok(a.draws.flushDraw && a.draws.oesd);
  assert.equal(countOuts(parseCards('9c 8c'), parseCards('Tc Jh 2c')).drawOuts, 15);
});

test('range tracker narrows a raiser and keeps strong hands', () => {
  const t = new Table({ seats: 6, rng: mulberry32(3) });
  for (let i = 0; i < 6; i++) t.sit(i, { id: `p${i}`, name: `P${i}`, stack: 200 });
  const h = t.startHand();
  const tr = new RangeTracker(h, new StatsBook());
  const seat = h.actor;
  tr.observe(t.act({ type: 'raise', amount: 5 }));
  const w = tr.width(seat);
  assert.ok(w < 0.35 && w > 0.03, `open width ${w}`);
  const r = tr.getRange(seat);
  const aa = comboIndex(parseCards('As')[0], parseCards('Ah')[0]);
  const trash = comboIndex(parseCards('7c')[0], parseCards('2d')[0]);
  assert.ok(r[aa] > 0.9 && r[trash] < 0.05);
});

test('bots play thousands of decisions legally with every profile', () => {
  const rng = mulberry32(11);
  const t = new Table({ seats: 7, rng: mulberry32(12) });
  const stats = new StatsBook();
  PROFILE_KEYS.forEach((k, i) => t.sit(i, { id: k, name: k, stack: 200 }));
  const mem = PROFILE_KEYS.map(() => new BotMemory());
  for (let hand = 0; hand < 150; hand++) {
    for (const p of t.seats) if (p.stack < 50) p.stack = 200;
    const h = t.startHand();
    const tr = new RangeTracker(h, stats);
    let guard = 0;
    while (!h.finished) {
      const seat = h.actor;
      const legal = t.legalActions();
      const d = decideBot({ table: t, seat, profile: PROFILES[PROFILE_KEYS[seat]], memory: mem[seat], tracker: tr, rng, iterationsScale: 0.4 });
      if (d.action.type === 'check') assert.equal(legal.toCall, 0);
      if (d.action.type === 'bet' || d.action.type === 'raise') assert.ok(d.action.amount >= legal.minTo && d.action.amount <= legal.maxTo);
      assert.ok(d.thinkMs >= 200 && d.thinkMs <= 9000);
      tr.observe(t.act(d.action));
      assert.ok(++guard < 300);
    }
    stats.recordHand(h);
  }
  const station = stats.raw('station', 'vpip').pct;
  const nit = stats.raw('nit', 'vpip').pct;
  assert.ok(station > nit, `station VPIP ${station} should exceed nit ${nit}`);
});

test('coach punishes folding the nuts and rewards raising it', () => {
  const t = new Table({ seats: 2, rng: mulberry32(5) });
  t.sit(0, { id: 'hero', name: 'Hero', stack: 200 });
  t.sit(1, { id: 'v', name: 'V', stack: 200 });
  t.buttonSeat = 1;
  const h = t.startHand(); // button = seat 0
  const stats = new StatsBook();
  const tr = new RangeTracker(h, stats);
  h.players[0].hole = parseCards('Ah Kh');
  h.players[1].hole = parseCards('7c 2d');
  h.deck = [...h.deck.slice(0, h.deckPos), ...parseCards('3c Qh Jh Th 4d 2s 5c 9s'), ...h.deck.slice(h.deckPos)];
  tr.observe(t.act({ type: 'call' }));
  tr.observe(t.act({ type: 'check' }));
  // flop Qh Jh Th: hero has a royal flush
  tr.observe(t.act({ type: 'bet', amount: 4 })); // villain (BB) acts first postflop
  assert.equal(h.actor, 0);
  const spot = analyzeSpot({ table: t, seat: 0, tracker: tr, rng: mulberry32(2) });
  assert.ok(spot.equity > 0.97);
  const fold = gradeDecision(spot, { type: 'fold' });
  assert.ok(['mistake', 'blunder'].includes(fold.grade), fold.grade);
  assert.ok(fold.tags.includes('overfold'));
  const raise = gradeDecision(spot, { type: 'raise', amount: 16 });
  assert.ok(['best', 'good'].includes(raise.grade), raise.grade);
});

test('coach says fold 72o first in from UTG', () => {
  const t = new Table({ seats: 6, rng: mulberry32(8) });
  for (let i = 0; i < 6; i++) t.sit(i, { id: `p${i}`, name: `P${i}`, stack: 200 });
  t.buttonSeat = 2; // button 3 -> SB 4, BB 5, first to act 0
  const h = t.startHand();
  assert.equal(h.actor, 0);
  h.players[0].hole = parseCards('7c 2d');
  const tr = new RangeTracker(h, new StatsBook());
  const spot = analyzeSpot({ table: t, seat: 0, tracker: tr });
  assert.equal(spot.recommended.type, 'fold');
  assert.equal(gradeDecision(spot, { type: 'fold' }).grade, 'best');
  assert.ok(['mistake', 'blunder', 'inaccuracy'].includes(gradeDecision(spot, { type: 'raise', amount: 5 }).grade));
});

test('every drill generates and grades', () => {
  const rng = mulberry32(4);
  for (const d of DRILLS) {
    for (let i = 0; i < 15; i++) {
      const q = d.generate(rng);
      const answer = { preflop: 'fold', equity: 50, potodds: q.options?.[0], outs: 8, showdown: 'split' }[q.kind];
      const g = d.grade(q, answer);
      assert.equal(typeof g.correct, 'boolean');
      assert.ok(Array.isArray(g.explanation) && g.explanation.length > 0);
    }
  }
});
