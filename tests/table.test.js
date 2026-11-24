import { test } from 'node:test';
import assert from 'node:assert/strict';
import { Table } from '../js/core/table.js';
import { mulberry32, parseCards } from '../js/core/cards.js';

function makeTable(n, stacks, seed = 1) {
  const t = new Table({ seats: n, smallBlind: 1, bigBlind: 2, rng: mulberry32(seed) });
  for (let i = 0; i < n; i++) t.sit(i, { id: `p${i}`, name: `P${i}`, stack: stacks[i] ?? 200 });
  return t;
}

const total = (t) => t.seats.reduce((s, p) => s + (p ? p.stack : 0), 0);

test('blinds and first to act (6-handed)', () => {
  const t = makeTable(6, []);
  t.buttonSeat = 5; // next button = 0
  const h = t.startHand();
  assert.equal(h.buttonSeat, 0);
  assert.equal(h.players[1].bet, 1);
  assert.equal(h.players[2].bet, 2);
  assert.equal(h.actor, 3);
  assert.equal(h.players[3].position, 'LJ');
  assert.equal(h.players[0].position, 'BTN');
});

test('heads-up: button posts small blind and acts first preflop, last postflop', () => {
  const t = makeTable(2, []);
  t.buttonSeat = 1;
  const h = t.startHand();
  assert.equal(h.buttonSeat, 0);
  assert.equal(h.players[0].bet, 1);
  assert.equal(h.players[1].bet, 2);
  assert.equal(h.actor, 0);
  t.act({ type: 'call' });
  assert.equal(h.actor, 1); // BB option
  t.act({ type: 'check' });
  assert.equal(h.street, 'flop');
  assert.equal(h.actor, 1);
});

test('everyone folds to big blind: BB wins blinds, uncalled returned', () => {
  const t = makeTable(4, []);
  t.buttonSeat = 3;
  t.startHand();
  t.act({ type: 'fold' });
  t.act({ type: 'fold' });
  t.act({ type: 'fold' });
  const h = t.hand;
  assert.ok(h.finished);
  assert.equal(t.seats[2].stack, 201);
  assert.equal(t.seats[1].stack, 199);
  assert.equal(total(t), 800);
});

test('min-raise rules and incomplete all-in raise does not reopen action', () => {
  const t = makeTable(3, [200, 200, 31]);
  t.buttonSeat = 2; // button 0, sb 1, bb 2
  const h = t.startHand();
  assert.equal(h.actor, 0);
  t.act({ type: 'raise', amount: 3 }); // clamps to min raise-to 4
  assert.equal(h.players[0].bet, 4);
  t.act({ type: 'raise', amount: 24 }); // SB raises to 24 (raise of 20)
  assert.equal(h.minRaise, 20);
  t.act({ type: 'allin' }); // BB all-in to 31: incomplete raise of 7
  assert.equal(h.currentBet, 31);
  const legal = t.legalActions();
  assert.equal(h.actor, 0);
  assert.ok(legal.canRaise, 'BTN has not acted since full raise, may raise');
  t.act({ type: 'call' });
  const legal2 = t.legalActions();
  assert.equal(h.actor, 1);
  assert.equal(legal2.canRaise, false, 'SB already acted; incomplete raise does not reopen');
  t.act({ type: 'call' });
  assert.equal(h.street, 'flop');
});

test('side pots are built and awarded correctly', () => {
  const t = makeTable(3, [50, 100, 200], 7);
  t.buttonSeat = 2;
  const h = t.startHand();
  // rig cards: seat0 best, seat1 second, seat2 worst
  h.players[0].hole = parseCards('As Ad');
  h.players[1].hole = parseCards('Ks Kd');
  h.players[2].hole = parseCards('6c 2d');
  h.deck = [...h.deck.slice(0, h.deckPos), ...parseCards('3h 9c Th 4s 5c Jd 2h 8s'), ...h.deck.slice(h.deckPos)];
  // remove duplicate cards so the deck stays valid enough for this test
  t.act({ type: 'allin' }); // seat0 50
  t.act({ type: 'allin' }); // seat1 100
  t.act({ type: 'call' }); // seat2 calls 100
  assert.ok(h.finished);
  const pots = h.results.pots;
  assert.equal(pots.length, 2);
  assert.equal(pots[0].amount, 150);
  assert.equal(pots[1].amount, 100);
  assert.equal(t.seats[0].stack, 150);
  assert.equal(t.seats[1].stack, 100);
  assert.equal(t.seats[2].stack, 100);
});

test('random play conserves chips over many hands', () => {
  const rng = mulberry32(42);
  const t = makeTable(6, [200, 200, 200, 200, 200, 200], 3);
  const start = total(t);
  for (let hand = 0; hand < 3000; hand++) {
    for (const p of t.seats) if (p.stack === 0) p.stack = 200;
    const before = total(t);
    t.startHand();
    let guard = 0;
    while (!t.hand.finished) {
      const l = t.legalActions();
      const r = rng();
      let a;
      if (r < 0.15 && l.toCall > 0) a = { type: 'fold' };
      else if (r < 0.65) a = { type: l.toCall > 0 ? 'call' : 'check' };
      else if (r < 0.95 && (l.canBet || l.canRaise)) a = { type: 'raise', amount: l.minTo + Math.floor(rng() * (l.maxTo - l.minTo + 1)) };
      else a = { type: 'allin' };
      t.act(a);
      assert.ok(++guard < 200, 'hand should terminate');
    }
    assert.equal(total(t), before, `chips conserved in hand ${hand}`);
    for (const s of t.hand.order) assert.ok(t.hand.players[s].stack >= 0);
    if (!t.hand.results.uncontested) assert.equal(t.hand.board.length, 5);
  }
  assert.ok(start > 0);
});
