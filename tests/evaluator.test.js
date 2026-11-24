import { test } from 'node:test';
import assert from 'node:assert/strict';
import { evaluate, categoryOf, describeScore, bestFive } from '../js/core/evaluator.js';
import { parseCards } from '../js/core/cards.js';

const ev = (s) => evaluate(parseCards(s));

test('exhaustive 5-card category counts match combinatorics', () => {
  const counts = new Array(9).fill(0);
  const h = new Array(5);
  for (let a = 0; a < 52; a++) for (let b = a + 1; b < 52; b++) for (let c = b + 1; c < 52; c++)
    for (let d = c + 1; d < 52; d++) for (let e = d + 1; e < 52; e++) {
      h[0] = a; h[1] = b; h[2] = c; h[3] = d; h[4] = e;
      counts[categoryOf(evaluate(h, 5))]++;
    }
  assert.deepEqual(counts, [1302540, 1098240, 123552, 54912, 10200, 5108, 3744, 624, 40]);
});

test('hand comparisons', () => {
  assert.ok(ev('As Ks Qs Js Ts 2c 3d') > ev('9s Ks Qs Js Ts 2c 3d'));
  assert.ok(ev('Ah 2d 3c 4s 5h 9c Kd') < ev('2d 3c 4s 5h 6h 9c Kd'));
  assert.ok(ev('Ah Ad Kc Ks 2h 2d 7c') === ev('Ah Ad Kc Ks 2h 3d 7c')); // 3rd pair irrelevant, 7 kicker
  assert.ok(ev('Ah Ad Kc Ks 2h 2d Qc') > ev('Ah Ad Kc Ks 2h 2d Jc'));
  assert.ok(ev('7h 7d 7c Ks Kh 2d 2c') > ev('7h 7d 7c Ks Qh 2d 2c'));
  assert.ok(ev('7h 7d 7c 8s 8h 8d 2c') === ev('8s 8h 8d 7h 7d 2c 3c'));
  assert.ok(ev('Ah Kh 2h 3h 9h 9d 9c') > ev('9s 9d 9c Kd Ks 2c 3c') === false);
  assert.equal(categoryOf(ev('Ah Kh 2h 3h 9h 9d 9c')), 5);
  assert.equal(describeScore(ev('Ts Js Qs Ks As')), 'Royal Flush');
  assert.equal(describeScore(ev('Ah 2d 3c 4s 5h')), 'Straight, Five high');
  assert.equal(describeScore(ev('Kh Kd 7c 7s 2h')), 'Two Pair, Kings and Sevens');
});

test('bestFive picks the scoring cards', () => {
  const r = bestFive(parseCards('As Ks Qs Js Ts 2c 3d'));
  assert.equal(describeScore(r.score), 'Royal Flush');
  assert.equal(r.cards.length, 5);
});
