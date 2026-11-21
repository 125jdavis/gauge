// Python bridge and modal dialogs.

import { S, requestRender } from './state.js';

/** window.pywebview.api, but a failed call resolves to {ok: false, ...} instead of rejecting. */
export const api = new Proxy({}, {
  get: (_target, name) => async (...args) => {
    try {
      return await window.pywebview.api[name](...args);
    } catch (exc) {
      return { ok: false, title: 'Error', error: (exc && exc.message) || String(exc) };
    }
  },
});

export function pywebviewReady() {
  return new Promise((resolve) => {
    if (window.pywebview && window.pywebview.api) resolve();
    else window.addEventListener('pywebviewready', () => resolve(), { once: true });
  });
}

// ---- modal dialogs ----------------------------------------------------------------------

/**
 * Show a modal and resolve with the value of the button pressed.
 * buttons: [{label, value, kind}] (first primary button gets focus).
 */
export function openModal({ title, message = '', buttons = [{ label: 'OK', value: true, kind: 'primary' }], busy = false }) {
  if (S.modal) S.modal.resolve(undefined);
  return new Promise((resolve) => {
    S.modal = { title, message, buttons, busy, resolve, focus: true };
    requestRender();
  });
}

export function closeModal(value) {
  const modal = S.modal;
  if (!modal) return;
  S.modal = null;
  requestRender();
  modal.resolve(value);
}

export const alertBox = (title, message) => openModal({ title, message });

export const confirmBox = (title, message, okLabel = 'OK') => openModal({
  title, message,
  buttons: [{ label: 'Cancel', value: false, kind: 'secondary' }, { label: okLabel, value: true, kind: 'primary' }],
});

/** Show a failed bridge result the way gui.py showed messagebox.showerror. Returns true if it failed. */
export function failed(result, fallbackTitle = 'Error') {
  if (result && result.ok !== false) return false;
  const title = (result && result.title) || fallbackTitle;
  const message = (result && result.error) || 'Unknown error';
  S.status = message.split('\n')[0];
  alertBox(title, message);
  return true;
}

export function requireConnection() {
  if (S.conn.connected) return true;
  alertBox('Not connected', 'Connect to the Arduino first.');
  return false;
}
