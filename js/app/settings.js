import { load, save } from './storage.js';

export const DEFAULT_SETTINGS = {
  heroName: 'You',
  numOpponents: 5,
  lineup: 'mixed',
  customSeats: ['shark', 'station', 'tag', 'lag', 'nit', 'maniac', 'scared', 'tag'],
  startingStackBB: 100,
  bigBlind: 2,
  mode: 'cash', // 'cash' | 'tournament'
  autoRebuy: true,
  levelHands: 10,
  speed: 'normal', // 'instant' | 'fast' | 'normal' | 'realistic'
  coachMode: 'all', // 'all' | 'mistakes' | 'off'
  showHud: true,
  showOppStats: true,
  revealProfiles: false,
  showAllCards: false,
  fourColor: false,
  autoDeal: true,
  allInEquity: true,
  sound: false,
};

export const SPEED_FACTOR = { instant: 0.04, fast: 0.35, normal: 0.75, realistic: 1.15 };

export function loadSettings() {
  return { ...DEFAULT_SETTINGS, ...load('settings', {}) };
}

export function saveSettings(s) {
  save('settings', s);
}

/** Tournament blind schedule (in big blinds of the starting level). */
export const BLIND_LEVELS = [
  [1, 2, 0], [2, 4, 0], [3, 6, 0], [4, 8, 1], [5, 10, 1], [6, 12, 2], [8, 16, 2], [10, 20, 3],
  [15, 30, 4], [20, 40, 5], [25, 50, 6], [30, 60, 8], [40, 80, 10], [50, 100, 12], [75, 150, 20], [100, 200, 25],
];
