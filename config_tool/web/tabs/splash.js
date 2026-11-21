// 10 / Outputs: two 128x32 images for the OLED displays.

import { html, on, panel, button, actions, progress } from '../ui.js';
import { S, requestRender } from '../state.js';
import { api, failed, confirmBox, requireConnection } from '../bridge.js';

async function load(slot) {
  const res = await api.splash_load(slot);
  if (failed(res, 'Load error') || res.cancelled) return;
  S.splash[slot] = { png: res.png, name: res.name, pct: 0 };
  S.status = `Slot ${slot}: loaded ${res.name}`;
  requestRender();
}

async function upload(slot) {
  if (!requireConnection()) return;
  S.splash[slot].pct = 0;
  S.status = `Slot ${slot}: uploading…`;
  requestRender();
  const res = await api.splash_upload(slot);
  if (failed(res, 'Upload error')) { requestRender(); return; }
  S.splash[slot].pct = 100;
  S.status = `Slot ${slot}: upload complete.`;
  requestRender();
}

async function test(slot) {
  if (!requireConnection()) return;
  if (failed(await api.splash_test(slot))) return;
  S.status = `Slot ${slot}: test command sent.`;
  requestRender();
}

async function clear(slot) {
  if (!requireConnection()) return;
  if (!(await confirmBox('Confirm', `Clear Slot ${slot}? This cannot be undone.`, 'Clear slot'))) return;
  if (failed(await api.splash_clear(slot))) return;
  S.splash[slot] = { png: '', name: '', pct: 0 };
  S.status = `Slot ${slot}: cleared.`;
  requestRender();
}

function slotPanel(slot) {
  const sp = S.splash[slot];
  const offline = !S.conn.connected;
  return panel({
    title: `Splash Slot ${slot}`,
    tag: 'SSD1306 · 128×32',
    cls: 'spaced',
    body: [
      html`<div class="oled">${sp.png ? html`<img src="${sp.png}" alt="Slot ${slot} preview">` : html`<span class="empty">Empty slot</span>`}</div>`,
      html`<span class="splash-caption">${sp.png ? `${sp.name} · converted to 1-bit, 128×32` : 'PNG, BMP, JPG or GIF. Scaled to fit and converted to 1-bit.'}</span>`,
      progress(sp.pct),
      actions([
        button('Load image…', { h: on({ click: () => load(slot) }) }),
        button('Upload to device', { kind: 'primary', h: on({ click: () => upload(slot) }), disabled: !sp.png || offline }),
        button('Test on device', { h: on({ click: () => test(slot) }), disabled: offline }),
        button('Clear slot', { kind: 'ghost', h: on({ click: () => clear(slot) }), disabled: offline }),
      ]),
    ],
  });
}

export function render() {
  return html`<div class="panel-grid roomy">${slotPanel(1)}${slotPanel(2)}</div>`;
}
