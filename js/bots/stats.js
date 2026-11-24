// Player statistics (HUD stats) built purely from public hand logs.
// Used by bots to model opponents (including you) and by the stats dashboard.

export const STAT_DEFS = {
  vpip: { label: 'VPIP', prior: 0.26, strength: 12, opp: 'hands', hit: 'vpip', desc: 'Voluntarily put money in pot preflop' },
  pfr: { label: 'PFR', prior: 0.18, strength: 12, opp: 'hands', hit: 'pfr', desc: 'Preflop raise' },
  threeBet: { label: '3-Bet', prior: 0.07, strength: 15, opp: 'threeBetOpp', hit: 'threeBet', desc: 'Re-raise vs a single raise' },
  foldTo3bet: { label: 'Fold to 3-Bet', prior: 0.55, strength: 8, opp: 'foldTo3betOpp', hit: 'foldTo3bet', desc: 'Fold after opening and facing a 3-bet' },
  steal: { label: 'Steal', prior: 0.35, strength: 10, opp: 'stealOpp', hit: 'steal', desc: 'Open-raise from CO/BTN/SB when folded to' },
  cbet: { label: 'C-Bet', prior: 0.6, strength: 8, opp: 'cbetOpp', hit: 'cbet', desc: 'Bet flop as preflop aggressor' },
  foldToCbet: { label: 'Fold to C-Bet', prior: 0.45, strength: 8, opp: 'foldToCbetOpp', hit: 'foldToCbet', desc: 'Fold facing a flop continuation bet' },
  foldToBet: { label: 'Fold vs Bet', prior: 0.42, strength: 12, opp: 'facingBet', hit: 'foldVsBet', desc: 'Fold when facing any postflop bet' },
  aggFreq: { label: 'Agg %', prior: 0.38, strength: 15, opp: 'postActs', hit: 'aggActs', desc: 'Share of postflop actions that are bets/raises' },
  wtsd: { label: 'WTSD', prior: 0.28, strength: 10, opp: 'sawFlop', hit: 'wtsd', desc: 'Went to showdown after seeing the flop' },
  wsd: { label: 'W$SD', prior: 0.5, strength: 8, opp: 'wtsd', hit: 'wsd', desc: 'Won money at showdown' },
};

const EMPTY = () => ({
  hands: 0, vpip: 0, pfr: 0, threeBetOpp: 0, threeBet: 0, foldTo3betOpp: 0, foldTo3bet: 0,
  stealOpp: 0, steal: 0, cbetOpp: 0, cbet: 0, foldToCbetOpp: 0, foldToCbet: 0,
  facingBet: 0, foldVsBet: 0, postActs: 0, aggActs: 0, callActs: 0, sawFlop: 0, wtsd: 0, wsd: 0,
  net: 0, bbWon: 0,
});

export class StatsBook {
  constructor(data = null) {
    this.players = {};
    if (data && data.players) for (const [id, s] of Object.entries(data.players)) this.players[id] = { ...EMPTY(), ...s };
  }

  get(id) {
    if (!this.players[id]) this.players[id] = EMPTY();
    return this.players[id];
  }

  reset(id) {
    if (id) delete this.players[id];
    else this.players = {};
  }

  /** Bayesian-shrunk rate (what a model should believe). */
  rate(id, stat) {
    const d = STAT_DEFS[stat];
    const s = this.get(id);
    const n = s[d.opp];
    const k = s[d.hit];
    return (k + d.prior * d.strength) / (n + d.strength);
  }

  /** Raw observed rate (what a HUD shows). */
  raw(id, stat) {
    const d = STAT_DEFS[stat];
    const s = this.get(id);
    const n = s[d.opp];
    return { n, k: s[d.hit], pct: n > 0 ? s[d.hit] / n : null };
  }

  af(id) {
    const s = this.get(id);
    return s.callActs > 0 ? s.aggActs / s.callActs : s.aggActs > 0 ? Infinity : null;
  }

  /** Update all players' stats from a finished hand. */
  recordHand(hand) {
    const seen = new Set();
    const flags = {};
    const F = (id) => (flags[id] ||= { vpip: false, pfr: false, threeBetOpp: false, foldTo3betOpp: false, stealOpp: false, cbetOpp: false, foldToCbetOpp: false });
    for (const seat of hand.order) {
      const p = hand.players[seat];
      seen.add(p.id);
      this.get(p.id).hands++;
    }
    let raises = 0;
    let opener = null;
    let pfa = null;
    let anyVoluntary = false;
    let flopBetSeen = false;
    let cbetActive = false;
    const posOf = (seat) => hand.players[seat].position;

    for (const e of hand.log) {
      if (e.blind || e.type === 'uncalled') continue;
      const s = this.get(e.id);
      const f = F(e.id);
      if (e.street === 'preflop') {
        if (raises === 0 && !anyVoluntary && !f.stealOpp && ['CO', 'BTN', 'SB'].includes(posOf(e.seat))) {
          f.stealOpp = true;
          s.stealOpp++;
          if (e.type === 'raise') s.steal++;
        }
        if (raises === 1 && e.id !== opener && !f.threeBetOpp) {
          f.threeBetOpp = true;
          s.threeBetOpp++;
          if (e.type === 'raise') s.threeBet++;
        }
        if (raises === 2 && e.id === opener && !f.foldTo3betOpp) {
          f.foldTo3betOpp = true;
          s.foldTo3betOpp++;
          if (e.type === 'fold') s.foldTo3bet++;
        }
        if ((e.type === 'call' || e.type === 'raise' || e.type === 'bet') && !f.vpip) {
          f.vpip = true;
          s.vpip++;
        }
        if (e.type === 'raise' && !f.pfr) {
          f.pfr = true;
          s.pfr++;
        }
        if (e.type === 'call' || e.type === 'raise') anyVoluntary = true;
        if (e.type === 'raise') {
          raises++;
          if (raises === 1) opener = e.id;
          pfa = e.id;
        }
        continue;
      }
      // postflop
      if (e.street === 'flop') {
        if (!flopBetSeen && e.id === pfa && !f.cbetOpp) {
          f.cbetOpp = true;
          s.cbetOpp++;
          if (e.type === 'bet') {
            s.cbet++;
            cbetActive = true;
          }
        } else if (cbetActive && e.id !== pfa && !f.foldToCbetOpp && e.toCall > 0) {
          f.foldToCbetOpp = true;
          s.foldToCbetOpp++;
          if (e.type === 'fold') s.foldToCbet++;
        }
        if (e.type === 'bet' || e.type === 'raise') {
          if (e.type === 'raise') cbetActive = false;
          flopBetSeen = true;
        }
      }
      if (e.toCall > 0) {
        s.facingBet++;
        if (e.type === 'fold') s.foldVsBet++;
      }
      s.postActs++;
      if (e.type === 'bet' || e.type === 'raise') s.aggActs++;
      if (e.type === 'call') s.callActs++;
    }

    // saw flop / showdown
    const reachedFlop = hand.board.length >= 3;
    if (reachedFlop) {
      const foldedPre = new Set(hand.log.filter((e) => e.street === 'preflop' && e.type === 'fold').map((e) => e.seat));
      for (const seat of hand.order) {
        if (foldedPre.has(seat)) continue;
        const p = hand.players[seat];
        this.get(p.id).sawFlop++;
      }
    }
    if (hand.results && !hand.results.uncontested) {
      for (const sd of hand.results.showdown) {
        const p = hand.players[sd.seat];
        const st = this.get(p.id);
        if (reachedFlop) st.wtsd++;
        if ((hand.results.winnings[sd.seat] || 0) > 0 && reachedFlop) st.wsd++;
      }
    }
    for (const seat of hand.order) {
      const p = hand.players[seat];
      const st = this.get(p.id);
      st.net += p.net || 0;
      st.bbWon += (p.net || 0) / hand.bb;
    }
  }

  toJSON() {
    return { players: this.players };
  }
}
