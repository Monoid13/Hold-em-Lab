// Long-term training progress for the hero: results, decision quality, leaks and drill scores.
import { load, save } from './storage.js';

const STREETS = ['preflop', 'flop', 'turn', 'river'];
const emptyGrades = () => ({ best: 0, good: 0, inaccuracy: 0, mistake: 0, blunder: 0 });

function empty() {
  return {
    hands: [],
    decisions: Object.fromEntries(STREETS.map((s) => [s, emptyGrades()])),
    tags: {},
    evLossBB: 0,
    drills: {},
    byPosition: {},
    sessions: 0,
  };
}

export class Progress {
  constructor() {
    this.data = { ...empty(), ...load('progress', {}) };
    for (const s of STREETS) this.data.decisions[s] ||= emptyGrades();
  }

  save() {
    if (this.data.hands.length > 5000) this.data.hands = this.data.hands.slice(-5000);
    save('progress', this.data);
  }

  reset() {
    const drills = this.data.drills;
    this.data = empty();
    this.data.drills = drills;
    this.save();
  }

  resetDrills() {
    this.data.drills = {};
    this.save();
  }

  recordDecision(g) {
    const d = this.data.decisions[g.street] || (this.data.decisions[g.street] = emptyGrades());
    d[g.grade]++;
    if (g.evLossBB && g.grade !== 'best' && g.grade !== 'good') this.data.evLossBB += g.evLossBB;
    for (const t of g.tags) this.data.tags[t] = (this.data.tags[t] || 0) + 1;
  }

  recordHand(h) {
    this.data.hands.push(h);
    const p = (this.data.byPosition[h.position] ||= { hands: 0, netBB: 0, vpip: 0 });
    p.hands++;
    p.netBB += h.netBB;
    if (h.vpip) p.vpip++;
  }

  recordDrill(key, { correct, score }) {
    const d = (this.data.drills[key] ||= { attempts: 0, correct: 0, streak: 0, bestStreak: 0, scoreSum: 0 });
    d.attempts++;
    if (correct) {
      d.correct++;
      d.streak++;
      d.bestStreak = Math.max(d.bestStreak, d.streak);
    } else d.streak = 0;
    d.scoreSum += score ?? (correct ? 100 : 0);
    this.save();
    return d;
  }

  totals() {
    const hands = this.data.hands;
    const net = hands.reduce((s, h) => s + h.netBB, 0);
    const grades = emptyGrades();
    for (const s of STREETS) for (const g of Object.keys(grades)) grades[g] += this.data.decisions[s][g];
    const n = Object.values(grades).reduce((a, b) => a + b, 0);
    const score = n ? (grades.best + grades.good * 0.85 + grades.inaccuracy * 0.5 + grades.mistake * 0.2) / n : null;
    return { hands: hands.length, netBB: net, bb100: hands.length ? (net / hands.length) * 100 : 0, grades, decisions: n, accuracy: score };
  }
}
