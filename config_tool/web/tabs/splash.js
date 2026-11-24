// 10 / Outputs: splash image for each OLED. Each display picks a built-in image or its own
// custom image, uploaded to the matching slot (display 1 = slot 1, display 2 = slot 2).

import { html, on, panel, button, actions, progress, field, fields, select } from '../ui.js';
import { S, requestRender, val, setVal, isDirty } from '../state.js';
import { api, failed, confirmBox, requireConnection } from '../bridge.js';

const imageParam = (display) => `splash${display}_img`;
const customId = (slot) => S.st.splashCustomIds[slot - 1];
const slotOf = (id) => S.st.splashCustomIds.indexOf(Number(id)) + 1;  // 0 = built-in

function imageChoices(display, id) {
  const choices = [
    ...S.st.splashImages.map((name, i) => [String(i), name]),
    [String(customId(display)), 'Custom image'],
  ];
  // A config may point this display at the other slot; keep it selectable instead of "Unsupported".
  const other = slotOf(id);
  if (other && other !== display) choices.push([String(id), `Custom image (slot ${other})`]);
  return choices;
}

function oled(src, alt, emptyText) {
  return html`<div class="oled">${src ? html`<img src="${src}" alt="${alt}">` : html`<span class="empty">${emptyText}</span>`}</div>`;
}

function preview(id) {
  if (id === '') return oled('', '', 'Not read');
  const slot = slotOf(id);
  if (!slot) return oled(`splash/${Number(id)}.png`, S.st.splashImages[Number(id)] ?? '', 'Unknown image');
  return oled(S.splash[slot].png, `Custom image, slot ${slot}`, 'Custom image');
}

// ---- actions ---------------------------------------------------------------------------

async function showOnDevice(display) {
  if (!requireConnection()) return;
  const id = val(imageParam(display));
  if (id === '') return;
  if (failed(await api.splash_show(display, Number(id)))) return;
  S.status = `Display ${display}: showing splash preview.`;
  requestRender();
}

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

async function clear(slot) {
  if (!requireConnection()) return;
  const msg = `Remove the custom image stored in slot ${slot}? A display set to it goes back to its default image.`;
  if (!(await confirmBox('Confirm', msg, 'Remove image'))) return;
  if (failed(await api.splash_clear(slot))) return;
  S.splash[slot] = { png: '', name: '', pct: 0 };
  S.status = `Slot ${slot}: custom image removed.`;
  requestRender();
}

// ---- panel -----------------------------------------------------------------------------

function customControls(slot) {
  const sp = S.splash[slot];
  const offline = !S.conn.connected;
  const caption = sp.png
    ? `${sp.name} · converted to 1-bit, 128×32`
    : 'PNG, BMP, JPG or GIF. Scaled to fit and converted to 1-bit. Upload saves it on the gauge right away.';
  return [
    html`<span class="splash-caption">${caption}</span>`,
    progress(sp.pct),
    actions([
      button('Load image…', { h: on({ click: () => load(slot) }) }),
      button('Upload to device', { kind: 'primary', h: on({ click: () => upload(slot) }), disabled: !sp.png || offline }),
      button('Remove from device', { kind: 'ghost', h: on({ click: () => clear(slot) }), disabled: offline }),
    ]),
  ];
}

function displayPanel(display) {
  const p = imageParam(display);
  const id = val(p);
  const slot = slotOf(id);
  const offline = !S.conn.connected;
  const h = on({ change: (_e, el) => { setVal(p, el.value); requestRender(); } });
  const note = slot
    ? `If slot ${slot} holds no uploaded image, the gauge shows its default image.`
    : '';
  return panel({
    title: `Display ${display}`,
    tag: 'Power-up & shutdown · 128×32',
    cls: 'spaced',
    dirty: isDirty(p),
    body: [
      preview(id),
      fields([field({
        label: 'Splash image',
        note,
        dirty: isDirty(p),
        tip: S.st.tooltips[p] || '',
        control: select({ value: id, options: imageChoices(display, id), h, dirty: isDirty(p) }),
      })]),
      slot ? customControls(slot) : '',
      actions([
        button('Show on device', {
          h: on({ click: () => showOnDevice(display) }),
          disabled: offline || id === '',
          title: 'Show the selected image on the gauge for a few seconds',
        }),
      ]),
    ],
  });
}

export function render() {
  return html`<div class="panel-grid roomy">${displayPanel(1)}${displayPanel(2)}</div>`;
}
