// Bot personalities. Each is a set of biases layered over the same EV engine, so weaker profiles
// make the kind of mistakes real players make (calling too much, folding too much, spewing).

export const PROFILES = {
  shark: {
    key: 'shark', label: 'Shark', style: 'Balanced pro',
    desc: 'Plays close to equilibrium, mixes bluffs and value, and adapts quickly to your tendencies.',
    skill: 0.96, adapt: 1, looseness: 1, aggression: 0, callBias: 0, foldBias: 0,
    threeBetMult: 1, bluff3bet: 0.3, trap: 0.1, tiltResist: 0.92, sizingTell: false, limp: 0, overvalue: 0,
    chat: 0.08, hollywood: 0.25,
  },
  tag: {
    key: 'tag', label: 'TAG', style: 'Tight-aggressive regular',
    desc: 'Solid and straightforward. Bets when strong, gives up when weak. Slightly too tight.',
    skill: 0.82, adapt: 0.7, looseness: 0.85, aggression: 0, callBias: -0.01, foldBias: 0.02,
    threeBetMult: 0.9, bluff3bet: 0.18, trap: 0.08, tiltResist: 0.75, sizingTell: false, limp: 0, overvalue: 0,
    chat: 0.08, hollywood: 0.1,
  },
  lag: {
    key: 'lag', label: 'LAG', style: 'Loose-aggressive',
    desc: 'Plays many hands and applies pressure with frequent bets and 3-bets. Hard to read.',
    skill: 0.78, adapt: 0.8, looseness: 1.35, aggression: 0.035, callBias: 0, foldBias: -0.01,
    threeBetMult: 1.4, bluff3bet: 0.5, trap: 0.06, tiltResist: 0.55, sizingTell: false, limp: 0, overvalue: 0,
    chat: 0.12, hollywood: 0.15,
  },
  nit: {
    key: 'nit', label: 'Nit', style: 'Tight-passive rock',
    desc: 'Waits for premium hands. When a nit raises, believe them. Steal their blinds relentlessly.',
    skill: 0.6, adapt: 0.3, looseness: 0.55, aggression: -0.04, callBias: -0.02, foldBias: 0.07,
    threeBetMult: 0.6, bluff3bet: 0.03, trap: 0.25, tiltResist: 0.8, sizingTell: false, limp: 0, overvalue: 0,
    chat: 0.05, hollywood: 0,
  },
  station: {
    key: 'station', label: 'Calling Station', style: 'Loose-passive fish',
    desc: 'Calls far too much and rarely folds a pair. Value bet thinly, never bluff. Bet sizing leaks hand strength.',
    skill: 0.3, adapt: 0.1, looseness: 1.7, aggression: -0.07, callBias: 0.16, foldBias: -0.06,
    threeBetMult: 0.35, bluff3bet: 0.02, trap: 0.2, tiltResist: 0.6, sizingTell: true, limp: 0.75, overvalue: 0.08,
    chat: 0.15, hollywood: 0, openSize: 3.5, pfrMult: 0.35,
  },
  maniac: {
    key: 'maniac', label: 'Maniac', style: 'Hyper-aggressive',
    desc: 'Raises and bluffs constantly. Call down lighter, let them hang themselves, avoid fancy bluffs.',
    skill: 0.38, adapt: 0.2, looseness: 2.0, aggression: 0.16, callBias: 0.04, foldBias: -0.04,
    threeBetMult: 2.2, bluff3bet: 0.75, trap: 0.03, tiltResist: 0.2, sizingTell: false, limp: 0, overvalue: 0.03,
    chat: 0.25, hollywood: 0, openSize: 3.5,
  },
  scared: {
    key: 'scared', label: 'Weak-Tight', style: 'Scared recreational',
    desc: 'Plays a fair number of hands but folds to pressure. Bluff them on scary turns and rivers.',
    skill: 0.42, adapt: 0.15, looseness: 1.15, aggression: -0.05, callBias: -0.02, foldBias: 0.09,
    threeBetMult: 0.5, bluff3bet: 0.05, trap: 0.1, tiltResist: 0.5, sizingTell: true, limp: 0.45, overvalue: 0,
    chat: 0.1, hollywood: 0, pfrMult: 0.6,
  },
};

export const PROFILE_KEYS = Object.keys(PROFILES);

export const BOT_NAMES = [
  ['Viktor', '🦈'], ['Mia', '🦊'], ['Dante', '🐺'], ['Priya', '🦉'], ['Hank', '🐻'], ['Lena', '🐱'],
  ['Omar', '🦁'], ['Greta', '🐢'], ['Jules', '🐙'], ['Ravi', '🐯'], ['Nora', '🦄'], ['Sal', '🐊'],
  ['Kenji', '🐼'], ['Bea', '🐝'], ['Marco', '🦅'], ['Ingrid', '🐧'],
];

export const LINEUPS = {
  beginner: { label: 'Soft table (learn the basics)', seats: ['station', 'scared', 'station', 'nit', 'maniac', 'scared', 'station', 'tag'] },
  mixed: { label: 'Mixed table (realistic)', seats: ['tag', 'station', 'shark', 'scared', 'lag', 'nit', 'maniac', 'station'] },
  tough: { label: 'Tough table (regs)', seats: ['shark', 'lag', 'tag', 'shark', 'tag', 'lag', 'shark', 'nit'] },
  sharks: { label: 'Shark tank (strongest bots)', seats: ['shark', 'shark', 'shark', 'shark', 'shark', 'shark', 'shark', 'shark'] },
};

/** The coach evaluates your play with an idealized, unbiased version of the shark. */
export const COACH_PROFILE = { ...PROFILES.shark, key: 'coach', skill: 1, adapt: 1, aggression: 0, callBias: 0, foldBias: 0, overvalue: 0 };

export const CHAT = {
  winBig: ['nh me 😎', 'ship it!', 'thanks for the chips', 'too easy', 'I knew it'],
  badBeat: ['unreal...', 'every single time 🙄', 'how do you call that??', 'rigged', 'sick river', 'ok I am done being nice'],
  loseBig: ['nice hand', 'gg', 'wow', 'ugh', 'should have folded'],
  tiltPlay: ['let\'s gamble', 'I\'m coming for you', 'all day', 'not again'],
  fold: ['fold', 'not this time', 'you got it'],
  greet: ['gl all', 'hi', 'let\'s go', 'gl'],
};
