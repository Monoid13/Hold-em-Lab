// App bootstrap: views, tabs, keyboard shortcuts.
import { loadSettings, saveSettings } from './app/settings.js';
import { Progress } from './app/progress.js';
import { HistoryStore } from './app/history.js';
import { GameController } from './app/controller.js';
import { clearAll } from './app/storage.js';
import { renderTable } from './ui/tableView.js';
import { ActionBar } from './ui/actionBar.js';
import { Sidebar } from './ui/sidebar.js';
import { ReviewView } from './ui/reviewView.js';
import { StatsView } from './ui/statsView.js';
import { DrillsView } from './ui/drillsView.js';
import { RangesView } from './ui/rangesView.js';
import { SettingsView } from './ui/settingsView.js';
import { $, $$, toast } from './ui/dom.js';

const settings = loadSettings();
const progress = new Progress();
const history = new HistoryStore();
const ctrl = new GameController({ settings, progress, history });

const tableEl = $('#table');
const sidebar = new Sidebar($('#sidebar'), ctrl);
const actionBar = new ActionBar($('#action-bar'), ctrl, { onHint: () => sidebar.showHint() });
actionBar.onSettings = () => saveSettings(settings);
const review = new ReviewView($('#view-review'), { history, settings });
const statsView = new StatsView($('#view-stats'), { ctrl, progress });
const drills = new DrillsView($('#view-drills'), { progress });
const ranges = new RangesView($('#view-ranges'));
const settingsView = new SettingsView($('#view-settings'), {
  settings,
  onApply: () => {
    saveSettings(settings);
    applyDisplay();
    switchView('play');
    ctrl.newGame();
    toast('New game started');
  },
  onLive: (name) => {
    saveSettings(settings);
    applyDisplay();
    if (name === 'autoDeal' && settings.autoDeal && ctrl.state.handOver) ctrl.deal();
    renderPlay();
  },
  onResetAll: () => {
    clearAll();
    location.reload();
  },
});

let currentView = 'play';
function switchView(v) {
  currentView = v;
  $$('.view').forEach((el) => el.classList.toggle('active', el.id === `view-${v}`));
  $$('#tabs button').forEach((b) => b.setAttribute('aria-selected', String(b.dataset.view === v)));
  if (v === 'review') review.render();
  if (v === 'stats') statsView.render();
  if (v === 'drills') drills.render();
  if (v === 'ranges') ranges.render();
  if (v === 'settings') {
    settingsView.draft = null;
    settingsView.render();
  }
  if (v === 'play') renderPlay();
}

$('#tabs').addEventListener('click', (e) => {
  const b = e.target.closest('button[data-view]');
  if (b) switchView(b.dataset.view);
});

function applyDisplay() {
  document.body.classList.toggle('four-color', !!settings.fourColor);
}

let raf = 0;
function renderPlay() {
  if (raf) return;
  raf = requestAnimationFrame(() => {
    raf = 0;
    renderTable(tableEl, ctrl);
    actionBar.render();
    sidebar.render();
    const h = ctrl.table?.hand;
    $('#hand-meta').textContent = h
      ? `Hand #${h.id} · Blinds ${h.sb}/${h.bb}${h.ante ? ` · Ante ${h.ante}` : ''}${settings.mode === 'tournament' ? ` · Level ${ctrl.state.level + 1}` : ''}`
      : '';
  });
}

ctrl.onChange((reason) => {
  if (reason === 'handOver' && currentView === 'stats') statsView.render();
  renderPlay();
});

tableEl.addEventListener('click', (e) => {
  if (e.target.closest('[data-act="newgame"]')) ctrl.newGame();
});

document.addEventListener('keydown', (e) => {
  if (e.target.matches('input, select, textarea') && e.target.type !== 'range') return;
  if (e.metaKey || e.ctrlKey || e.altKey) return;
  if (currentView === 'review' && review.key(e)) return e.preventDefault();
  if (currentView === 'drills' && drills.key(e)) return e.preventDefault();
  if (currentView !== 'play') return;
  const k = e.key.toLowerCase();
  const map = { f: 'fold', c: 'call', k: 'call', r: 'raise', b: 'raise', h: 'hint', p: 'pause' };
  if (k === ' ' || k === 'enter') {
    if (ctrl.state.handOver) {
      e.preventDefault();
      ctrl.deal();
    }
    return;
  }
  if (map[k]) {
    e.preventDefault();
    actionBar.perform(map[k]);
  }
});

applyDisplay();
ctrl.newGame();
switchView('play');

// exposed for debugging from the browser console
globalThis.holdemLab = { ctrl, settings, progress, history };
