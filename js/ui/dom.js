import { RANKS, SUIT_SYMBOL } from '../core/cards.js';

export const $ = (sel, root = document) => root.querySelector(sel);
export const $$ = (sel, root = document) => [...root.querySelectorAll(sel)];

export function esc(s) {
  return String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
}

const SUIT_CLASS = ['c', 'd', 'h', 's'];

export function cardHTML(c, { hidden = false, small = false, highlight = false, dim = false, cls = '', loc = '' } = {}) {
  if (hidden || c === undefined || c === null) return `<div class="card back${small ? ' small' : ''} ${cls}"${loc ? ` data-card="${c ?? 0}" data-loc="${loc}"` : ''}></div>`;
  const r = RANKS[c >> 2];
  const s = c & 3;
  return `<div class="card s-${SUIT_CLASS[s]}${small ? ' small' : ''}${highlight ? ' hl' : ''}${dim ? ' dim' : ''} ${cls}" data-card="${c}"${loc ? ` data-loc="${loc}"` : ''}><span class="r">${r === 'T' ? '10' : r}</span><span class="s">${SUIT_SYMBOL[s]}</span></div>`;
}

export function cardsInline(cards) {
  return cards.map((c) => cardHTML(c, { small: true })).join('');
}

export function cardText(c) {
  return `${RANKS[c >> 2]}${SUIT_SYMBOL[c & 3]}`;
}

export function fmt(n) {
  if (n === null || n === undefined || Number.isNaN(n)) return '–';
  return Math.round(n).toLocaleString();
}

export function fmtBB(x, digits = 1) {
  if (x === null || x === undefined || Number.isNaN(x)) return '–';
  const v = Number(x);
  return `${v > 0 ? '+' : v < 0 ? '−' : ''}${Math.abs(v).toFixed(digits)} bb`;
}

export function pct(x, digits = 0) {
  if (x === null || x === undefined || Number.isNaN(x)) return '–';
  return `${(x * 100).toFixed(digits)}%`;
}

export function gradeBadge(grade, info) {
  if (!grade) return '';
  return `<span class="grade grade-${grade}" title="${esc(info?.label ?? grade)}"><b>${esc(info?.symbol ?? '')}</b> ${esc(info?.label ?? grade)}</span>`;
}

let toastTimer;
export function toast(msg, ms = 2600) {
  let el = document.getElementById('toast');
  if (!el) {
    el = document.createElement('div');
    el.id = 'toast';
    el.setAttribute('role', 'status');
    document.body.appendChild(el);
  }
  el.textContent = msg;
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove('show'), ms);
}
