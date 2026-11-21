// 06 / Outputs: LED count, RPM zones and colours for the shift light strip.

import { html, on, panel, fields, button, range, dirtyDot } from '../ui.js';
import { S, val, setVal, paramField, selectField, requestRender, isDirty, intOr } from '../state.js';
import {
  tachValues, tachSteps, tachLedStep, tachStepRpm, tachDisplayColor, tachColorHsv, tachHsvColor, pyInt,
} from '../units.js';
import { toggleTachTest } from '../actions.js';

const ZONES = [['normal', 'Normal'], ['warning', 'Warning'], ['shift', 'Shift']];
const colorParam = (zone) => `tach_color_${zone}`;

/** Remembered hue per colour, so brightness 0 (black) keeps its hue (GaugeConfigApp._tach_color_hues). */
function hueOf(param) {
  if (!(param in S.tach.hues)) {
    try { S.tach.hues[param] = tachColorHsv(S.st.tachColors[param])[0] * 360; } catch { S.tach.hues[param] = 0; }
  }
  try {
    const [h, s, v] = tachColorHsv(val(param));
    if (v > 0 && s > 0) S.tach.hues[param] = h * 360;
  } catch { /* invalid colour: keep the remembered hue */ }
  return S.tach.hues[param];
}

function displayColor(param) {
  try { return tachDisplayColor(val(param), hueOf(param)); } catch { return '#888888'; }
}

/** Clamp the zones to the strip (GaugeConfigApp._normalize_tach_zones). */
function fitZones() {
  const count = intOr('num_leds');
  const sweep = intOr('tach_sweep');
  const warning = intOr('warn_leds');
  const shift = intOr('shift_leds');
  if ([count, sweep, warning, shift].includes(null)) return;
  if (!(count >= 1 && count <= 64) || !(sweep >= 0 && sweep <= 3)) return;
  const w = Math.max(0, Math.min(warning, tachSteps(count, sweep) - 1));
  setVal('warn_leds', String(w));
  setVal('shift_leds', String(Math.max(0, Math.min(shift, w))));
}

function settingsPanel() {
  return panel({
    title: 'LED Tachometer',
    wide: true,
    body: fields([
      paramField('LED count', 'num_leds', { note: '', onCommit: fitZones }),
      paramField('Min display RPM', 'tach_min', { note: '' }),
      paramField('Max RPM', 'tach_max', { note: '' }),
      paramField('Flash RPM', 'tach_flash', { note: '' }),
      selectField('Sweep style', 'tach_sweep', S.st.tachSweeps, { note: '', onPick: (v) => { setVal('tach_sweep', v); fitZones(); } }),
    ]),
  });
}

function stepper(label, param, delta, t) {
  const steps = tachSteps(t.count, t.sweep);
  const move = (d) => on({ click: () => {
    const value = intOr(param, 0) + d;
    if (param === 'warn_leds') setVal(param, String(Math.max(t.shift, Math.min(steps - 1, value))));
    else setVal(param, String(Math.max(0, Math.min(intOr('warn_leds', 0), value))));
    requestRender();
  } });
  return html`<div class="stepper">
    <span class="label">${dirtyDot(isDirty(param))}${label}</span>
    <div class="row">
      ${button('−', { cls: 'btn-icon', h: move(-delta), title: `Fewer ${label.toLowerCase()} steps` })}
      <span class="value">${val(param)}</span>
      ${button('+', { cls: 'btn-icon', h: move(delta), title: `More ${label.toLowerCase()} steps` })}
      <span class="unit">steps before final</span>
    </div>
  </div>`;
}

function colorEditor(zone, label) {
  const param = colorParam(zone);
  let hue = 0;
  let bright = 0;
  try {
    const [h, , v] = tachColorHsv(val(param));
    bright = Math.round(v * 100);
    hue = Math.round(v === 0 ? hueOf(param) : h * 360);
  } catch { hue = Math.round(hueOf(param)); }
  const setHue = on({ input: (_e, el) => {
    const hh = Number(el.value);
    S.tach.hues[param] = hh;
    setVal(param, tachHsvColor(hh, bright));
    requestRender();
  } });
  const setBright = on({ input: (_e, el) => {
    setVal(param, tachHsvColor(hueOf(param), Number(el.value)));
    requestRender();
  } });
  return html`<div class="color-editor">
    <div class="head">
      <span class="swatch" style="background:${displayColor(param)}"></span>
      <span class="name">${label} color</span>${dirtyDot(isDirty(param))}
      <span class="hex">#${val(param)}</span>
    </div>
    <div class="sliders">
      <span>Hue</span>${range({ value: hue, min: 0, max: 359, h: setHue, label: `${label} hue` })}<span class="readout">${hue}°</span>
      <span>Brightness</span>${range({ value: bright, min: 0, max: 100, h: setBright, label: `${label} brightness` })}<span class="readout">${bright}%</span>
    </div>
  </div>`;
}

function previewPanel() {
  let t;
  try {
    t = tachValues(val);
  } catch (exc) {
    return panel({ title: 'Strip preview', wide: true, body: html`<div class="strip"><span class="empty">${exc.message}</span></div>` });
  }
  const { count, minimum, maximum, sweep, warning, shift } = t;
  const steps = tachSteps(count, sweep);
  let flash = Infinity;
  try { flash = pyInt(val('tach_flash')); } catch { /* unset */ }
  const maxPreview = Math.max(maximum, Number.isFinite(flash) ? flash : 0) + 1000;
  if (S.tach.previewRpm == null) S.tach.previewRpm = maximum;
  const rpm = Math.min(S.tach.previewRpm, maxPreview);
  const zoneOf = (step) => (step >= steps - 1 - shift ? 'shift' : step >= steps - 1 - warning ? 'warning' : 'normal');
  const colors = Object.fromEntries(ZONES.map(([z]) => [z, displayColor(colorParam(z))]));

  const leds = [...Array(count).keys()].map((i) => {
    const step = tachLedStep(i, count, sweep);
    const zone = zoneOf(step);
    const lit = rpm >= tachStepRpm(step, steps, minimum, maximum)
      && !(zone === 'shift' && rpm >= flash && !S.tach.blink);
    return { color: lit ? colors[zone] : '', zone };
  });
  const span = (zone) => {
    const list = [...Array(steps).keys()].filter((s) => zoneOf(s) === zone);
    if (!list.length) return 'none';
    return `${tachStepRpm(list[0], steps, minimum, maximum)}–${tachStepRpm(list[list.length - 1], steps, minimum, maximum)} rpm`;
  };
  const legend = [...ZONES.map(([z, name]) => [name, colors[z], span(z)]), ['Flash', colors.shift, `≥ ${val('tach_flash')} rpm`]];
  const setPreview = on({ input: (_e, el) => { S.tach.previewRpm = Number(el.value); requestRender(); } });

  return panel({
    title: 'Strip preview',
    tag: `${count} LEDs · ${S.st.tachSweeps[sweep]} · ${steps} steps`,
    wide: true,
    cls: 'spaced',
    body: [
      html`<div class="strip">
        <div class="row">${leds.map((l) => html`<div class="led"${l.color ? html` style="background:${l.color}"` : ''}></div>`)}</div>
        <div class="row">${leds.map((l) => html`<div class="zone" style="background:${colors[l.zone]}"></div>`)}</div>
      </div>`,
      html`<div class="legend">${legend.map(([name, color, text]) => html`<div><span class="swatch" style="background:${color}"></span><b>${name}</b><span class="range">${text}</span></div>`)}</div>`,
      html`<div class="rpm-row"><span>Preview RPM</span>${range({ value: rpm, min: 0, max: maxPreview, step: 50, h: setPreview, label: 'Preview RPM' })}<span class="big-readout">${rpm}</span></div>`,
      html`<div class="section steppers">
        ${stepper('Warning zone', 'warn_leds', 1, t)}
        ${stepper('Shift zone', 'shift_leds', 1, t)}
        <div class="stepper end">${button('Fit zones to strip', { h: on({ click: () => { fitZones(); requestRender(); } }) })}</div>
      </div>`,
      html`<div class="section colors">${ZONES.map(([z, name]) => colorEditor(z, name))}</div>`,
      html`<div class="section inline">
        ${button(S.tach.testOn ? 'Test strip: On' : 'Test strip: Off', { h: on({ click: () => toggleTachTest() }), disabled: !S.conn.connected })}
        <span>Test uses unsaved settings; leaving this tab restores normal output.</span>
      </div>`,
    ],
  });
}

/** True when the shift zone is flashing, so the blink timer needs to re-render. */
export function isBlinking() {
  try {
    const flash = pyInt(val('tach_flash'));
    return S.tach.previewRpm != null && S.tach.previewRpm >= flash;
  } catch { return false; }
}

export function render() {
  return html`<div class="panel-grid">${settingsPanel()}${previewPanel()}</div>`;
}
