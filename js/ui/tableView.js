// Renders the poker table (seats, cards, bets, board, pot) from controller state.
import { cardHTML, esc, fmt, pct } from './dom.js';
import { PROFILES } from '../bots/profiles.js';
import { HERO_ID } from '../app/controller.js';

function seatGeometry(i, n) {
  const a = ((90 + (i * 360) / n) * Math.PI) / 180;
  const c = Math.cos(a);
  const s = Math.sin(a);
  return {
    seat: { x: 50 + 44 * c, y: 50 + 40 * s },
    bet: { x: 50 + 29 * c, y: 50 + 25 * s },
    dealer: { x: 50 + 34 * Math.cos(a - 0.28), y: 50 + 30 * Math.sin(a - 0.28) },
  };
}

export function hudLine(stats, id) {
  const s = stats.get(id);
  if (!s.hands) return 'no reads yet';
  const r = (k) => stats.raw(id, k).pct;
  const v = (x) => (x === null ? '–' : Math.round(x * 100));
  const af = stats.af(id);
  return `${v(r('vpip'))}/${v(r('pfr'))}/${af === null ? '–' : af === Infinity ? '∞' : af.toFixed(1)} · ${s.hands}h`;
}

let seenKey = '';
let seen = new Set();

/** Mark cards that were not on screen in the previous render so only they animate in. */
function freshCards(html, handKey) {
  if (handKey !== seenKey) {
    seenKey = handKey;
    seen = new Set();
  }
  const now = new Set();
  const out = html.replace(/data-card="(\d+)" data-loc="([^"]+)"/g, (m, c, loc) => {
    const k = `${loc}:${c}`;
    now.add(k);
    return seen.has(k) ? m : `${m} data-new="1"`;
  });
  for (const k of now) seen.add(k);
  return out;
}

export function renderTable(root, ctrl) {
  const t = ctrl.table;
  if (!t) {
    root.innerHTML = '';
    return;
  }
  const st = ctrl.state;
  const set = ctrl.settings;
  const h = t.hand;
  const n = t.numSeats;
  const runningOut = h && h.finished && st.revealCount < 5;
  const finishedShown = h && h.finished && !runningOut && st.handOver;
  const showdownSeats = new Set(h?.results && !h.results.uncontested ? h.results.showdown.map((s) => s.seat) : []);
  const winners = h?.results ? h.results.winnings : {};
  const bestCards = new Set();
  if (finishedShown && h.results && !h.results.uncontested) {
    const top = h.results.showdown.filter((s) => (winners[s.seat] || 0) > 0);
    for (const s of top) for (const c of s.bestCards) bestCards.add(c);
  }

  let html = '<div class="felt"><div class="felt-inner"></div></div>';
  let potTotal = 0;
  for (let i = 0; i < n; i++) {
    const p = t.seats[i];
    if (!p) continue;
    const g = seatGeometry(i, n);
    const hp = h ? h.players[i] : null;
    const inHand = !!hp;
    const isHero = p.id === HERO_ID;
    const bot = ctrl.bots[i];
    const classes = ['seat'];
    if (isHero) classes.push('hero');
    if (!inHand || hp.folded) classes.push('folded');
    if (h && !h.finished && h.actor === i) classes.push('acting');
    if (finishedShown && (winners[i] || 0) > 0) classes.push('winner');
    if (p.stack <= 0 && !inHand) classes.push('busted');
    if (g.seat.y > 70) classes.push('low');

    let stack = inHand ? hp.stack : p.stack;
    let bet = inHand ? hp.bet : 0;
    if (runningOut && inHand) {
      stack = hp.startStack - hp.contrib;
      bet = 0;
    }
    if (inHand) potTotal += runningOut ? hp.contrib : bet;

    // cards
    let cards = '';
    if (inHand && hp.hole.length) {
      const reveal = isHero || set.showAllCards || ((runningOut || finishedShown) && showdownSeats.has(i));
      if (!hp.folded || isHero || set.showAllCards) {
        cards = hp.hole.map((c) => cardHTML(c, { hidden: !reveal, highlight: finishedShown && bestCards.has(c), dim: hp.folded, loc: `s${i}${reveal ? 'u' : 'd'}` })).join('');
      }
    }
    const sd = finishedShown && h.results && !h.results.uncontested ? h.results.showdown.find((s) => s.seat === i) : null;
    const prof = bot ? PROFILES[bot.profile.key] : null;
    const tags = [];
    if (hp?.position) tags.push(`<span class="pos pos-${hp.position.replace('+', '')}">${esc(hp.position)}</span>`);
    const flash = st.flashes[i];
    const chat = st.chats[i];
    const eq = st.allInEquity?.[i];

    html += `<div class="${classes.join(' ')}" style="left:${g.seat.x}%;top:${g.seat.y}%" data-seat="${i}">
      <div class="seat-cards">${cards}</div>
      <div class="seat-box">
        <div class="avatar">${esc(isHero ? '🙂' : bot?.avatar ?? '🤖')}</div>
        <div class="seat-info">
          <div class="name">${esc(p.name)} ${tags.join('')}</div>
          <div class="stack">${p.stack <= 0 && !inHand ? 'busted' : fmt(stack)}</div>
        </div>
        ${st.thinkingSeat === i ? '<div class="thinking" aria-label="thinking"><i></i><i></i><i></i></div>' : ''}
      </div>
      ${!isHero && set.showOppStats ? `<div class="seat-hud" title="VPIP / PFR / Aggression factor · hands observed">${esc(hudLine(ctrl.stats, p.id))}</div>` : ''}
      ${!isHero && set.revealProfiles && prof ? `<div class="profile-tag">${esc(prof.label)}${bot.memory.tilt > 0.35 ? ' 😡' : ''}</div>` : ''}
      ${sd ? `<div class="sd-name">${esc(sd.handName)}</div>` : ''}
      ${finishedShown && (winners[i] || 0) > 0 ? `<div class="win-amt">+${fmt(winners[i])}</div>` : ''}
      ${eq !== undefined && set.allInEquity ? `<div class="eq-badge">${pct(eq)}</div>` : ''}
      ${flash && !finishedShown ? `<div class="flash">${esc(flash)}</div>` : ''}
      ${chat ? `<div class="chat-bubble">${esc(chat.text)}</div>` : ''}
    </div>`;
    if (bet > 0) html += `<div class="bet" style="left:${g.bet.x}%;top:${g.bet.y}%"><span class="chip"></span>${fmt(bet)}</div>`;
    if (h && h.buttonSeat === i) html += `<div class="dealer-btn" style="left:${g.dealer.x}%;top:${g.dealer.y}%">D</div>`;
  }

  // center
  let boardCards = [];
  let pot = 0;
  if (h) {
    const count = runningOut ? st.revealCount : h.board.length;
    boardCards = h.board.slice(0, count);
    pot = runningOut ? potTotal : h.finished ? 0 : h.pot;
  }
  const slots = [];
  for (let k = 0; k < 5; k++) {
    const c = boardCards[k];
    slots.push(c !== undefined ? cardHTML(c, { highlight: finishedShown && bestCards.has(c), cls: 'board-card', loc: 'b' }) : '<div class="card slot"></div>');
  }
  let msg = '';
  if (finishedShown && h.results) {
    const ws = Object.entries(h.results.winnings).map(([s, a]) => `${h.players[s].id === HERO_ID ? 'You win' : `${esc(h.players[s].name)} wins`} ${fmt(a)}`);
    const sdw = h.results.uncontested ? '' : (() => {
      const w = h.results.showdown.find((s) => (winners[s.seat] || 0) > 0);
      return w ? ` — ${esc(w.handName)}` : '';
    })();
    msg = `${ws.join(' · ')}${sdw}`;
  } else if (st.levelUp && h && h.log.length <= 3) msg = `Blinds up: ${h.sb}/${h.bb}${h.ante ? ` (ante ${h.ante})` : ''}`;
  const potShown = h && !h.finished ? t.potTotal() : pot;
  html += `<div class="center">
    <div class="board">${slots.join('')}</div>
    <div class="pot${potShown > 0 ? '' : ' empty'}">${potShown > 0 ? `Pot <b>${fmt(potShown)}</b>` : '&nbsp;'}</div>
    <div class="table-msg">${msg}</div>
  </div>`;
  if (st.gameOver) {
    html += `<div class="overlay"><div class="overlay-box"><h2>${st.gameOver.won ? '🏆 You won the tournament!' : `You finished ${ordinal(st.gameOver.place)} of ${st.gameOver.of}`}</h2><button class="btn primary" data-act="newgame">Play again</button></div></div>`;
  }
  root.innerHTML = freshCards(html, h ? `${h.id}` : '');
}

function ordinal(n) {
  const s = ['th', 'st', 'nd', 'rd'];
  const v = n % 100;
  return n + (s[(v - 20) % 10] || s[v] || s[0]);
}
