// Card primitives. A card is an int 0..51: rank = c >> 2 (0 = deuce .. 12 = ace), suit = c & 3.

export const RANKS = '23456789TJQKA';
export const SUITS = 'cdhs';
export const SUIT_SYMBOL = ['♣', '♦', '♥', '♠'];
export const SUIT_NAME = ['clubs', 'diamonds', 'hearts', 'spades'];
export const RANK_NAME = ['Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine', 'Ten', 'Jack', 'Queen', 'King', 'Ace'];
export const RANK_PLURAL = ['Twos', 'Threes', 'Fours', 'Fives', 'Sixes', 'Sevens', 'Eights', 'Nines', 'Tens', 'Jacks', 'Queens', 'Kings', 'Aces'];

export const rankOf = (c) => c >> 2;
export const suitOf = (c) => c & 3;
export const makeCard = (rank, suit) => (rank << 2) | suit;

export function cardToString(c) {
  return RANKS[c >> 2] + SUITS[c & 3];
}

export function cardsToString(cards) {
  return cards.map(cardToString).join(' ');
}

export function parseCard(str) {
  const r = RANKS.indexOf(str[0].toUpperCase());
  const s = SUITS.indexOf(str[1].toLowerCase());
  if (r < 0 || s < 0) throw new Error(`Bad card: ${str}`);
  return makeCard(r, s);
}

/** Parses "As Kd" / "AsKd" / "As,Kd" into card ints. */
export function parseCards(str) {
  return str
    .trim()
    .split(/[\s,]+/)
    .filter(Boolean)
    .flatMap((tok) => tok.match(/.{2}/g).map(parseCard));
}

export function fullDeck() {
  return Array.from({ length: 52 }, (_, i) => i);
}

/** Deterministic PRNG for tests and simulations. */
export function mulberry32(seed) {
  let a = seed >>> 0;
  return function rng() {
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/** Cryptographically strong RNG for dealing real hands (falls back to Math.random). */
export function secureRng() {
  const c = globalThis.crypto;
  if (c && typeof c.getRandomValues === 'function') {
    const buf = new Uint32Array(64);
    let i = buf.length;
    return function rng() {
      if (i >= buf.length) {
        c.getRandomValues(buf);
        i = 0;
      }
      return buf[i++] / 4294967296;
    };
  }
  return Math.random;
}

export function shuffle(arr, rng = Math.random) {
  for (let i = arr.length - 1; i > 0; i--) {
    const j = Math.floor(rng() * (i + 1));
    const t = arr[i];
    arr[i] = arr[j];
    arr[j] = t;
  }
  return arr;
}
