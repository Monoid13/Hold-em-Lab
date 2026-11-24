// localStorage wrapper that never throws (private mode, quota, disabled storage).
const PREFIX = 'holdemlab.v1.';

export function load(key, fallback) {
  try {
    const raw = globalThis.localStorage?.getItem(PREFIX + key);
    return raw ? JSON.parse(raw) : fallback;
  } catch {
    return fallback;
  }
}

export function save(key, value) {
  try {
    globalThis.localStorage?.setItem(PREFIX + key, JSON.stringify(value));
    return true;
  } catch {
    return false;
  }
}

export function remove(key) {
  try {
    globalThis.localStorage?.removeItem(PREFIX + key);
  } catch {
    /* ignore */
  }
}

export function clearAll() {
  try {
    const ls = globalThis.localStorage;
    if (!ls) return;
    const keys = [];
    for (let i = 0; i < ls.length; i++) if (ls.key(i).startsWith(PREFIX)) keys.push(ls.key(i));
    keys.forEach((k) => ls.removeItem(k));
  } catch {
    /* ignore */
  }
}
