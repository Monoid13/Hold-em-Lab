// Headless bot-vs-bot simulation: `npm run sim -- --hands 2000 --lineup mixed --players 6`
import { Table } from '../js/core/table.js';
import { mulberry32 } from '../js/core/cards.js';
import { StatsBook } from '../js/bots/stats.js';
import { RangeTracker } from '../js/bots/rangeModel.js';
import { decideBot, BotMemory } from '../js/bots/bot.js';
import { PROFILES, LINEUPS } from '../js/bots/profiles.js';

const args = Object.fromEntries(process.argv.slice(2).join(' ').split('--').filter(Boolean).map((s) => s.trim().split(/\s+/)));
const HANDS = Number(args.hands ?? 1000);
const N = Number(args.players ?? 6);
const lineup = args.seats ? args.seats.split(',') : LINEUPS[args.lineup ?? 'mixed'].seats.slice(0, N);
const RUNS = Number(args.runs ?? 1);
const baseSeed = Number(args.seed ?? 1);
const scale = Number(args.scale ?? 0.6);

const totals = {};
const statTotals = {};
for (let run = 0; run < RUNS; run++) {
const seed = baseSeed + run * 1000;
const rng = mulberry32(seed);
const STACK = 200;
const table = new Table({ seats: N, smallBlind: 1, bigBlind: 2, rng: mulberry32(seed + 99) });
const stats = new StatsBook();
const bots = lineup.map((key, i) => ({ key, profile: PROFILES[key], memory: new BotMemory(), net: 0 }));
bots.forEach((b, i) => table.sit(i, { id: `${b.key}#${i}`, name: `${b.key}#${i}`, stack: STACK }));

let decisions = 0;
const t0 = Date.now();
for (let h = 0; h < HANDS; h++) {
  for (const p of table.seats) if (p.stack < STACK) { bots[p.seat].net -= STACK - p.stack; p.stack = STACK; }
  const hand = table.startHand();
  const tracker = new RangeTracker(hand, stats);
  let guard = 0;
  while (!hand.finished) {
    const seat = hand.actor;
    const b = bots[seat];
    const d = decideBot({ table, seat, profile: b.profile, memory: b.memory, tracker, rng, iterationsScale: scale });
    const entry = table.act(d.action);
    tracker.observe(entry);
    decisions++;
    if (++guard > 300) throw new Error('hand did not terminate');
  }
  stats.recordHand(hand);
  for (const s of hand.order) {
    const hp = hand.players[s];
    bots[s].memory.afterHand({ netBB: hp.net / 2, badBeat: false, profile: bots[s].profile });
  }
}
for (const p of table.seats) bots[p.seat].net += p.stack - STACK;
const ms = Date.now() - t0;
if (RUNS === 1 || args.verbose) console.log(`${HANDS} hands, ${decisions} decisions, ${(ms / decisions).toFixed(2)} ms/decision\n`);
const agg = {};
bots.forEach((b, i) => {
  const id = `${b.key}#${i}`;
  const r = (s) => stats.raw(id, s).pct;
  const f = (x) => (x == null ? '  -  ' : (x * 100).toFixed(0).padStart(4) + '%');
  if (RUNS === 1 || args.verbose) console.log(`${id.padEnd(12)} bb/100 ${((b.net / 2 / HANDS) * 100).toFixed(1).padStart(7)} | VPIP ${f(r('vpip'))} PFR ${f(r('pfr'))} 3B ${f(r('threeBet'))} CB ${f(r('cbet'))} FvB ${f(r('foldToBet'))} WTSD ${f(r('wtsd'))} AF ${(stats.af(id) ?? 0).toFixed(1)}`);
  (agg[b.key] ||= []).push((b.net / 2 / HANDS) * 100);
  (totals[b.key] ||= []).push((b.net / 2 / HANDS) * 100);
});
}
if (RUNS > 1) {
  console.log('\nAverage bb/100 by profile over', RUNS, 'runs:');
  for (const [k, v] of Object.entries(totals)) console.log(k.padEnd(10), (v.reduce((a, b) => a + b, 0) / v.length).toFixed(1).padStart(8), ` (n=${v.length})`);
}
