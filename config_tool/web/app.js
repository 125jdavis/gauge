// Gauge V4 Configuration Tool: app shell, rendering and Python events.

import { html, on, attr, button, icon, morph, resetHandlers, dispatch, progress } from './ui.js';
import { S, setRenderer, requestRender, visibleDirtyParams, tabOf } from './state.js';
import { api, pywebviewReady, closeModal } from './bridge.js';
import {
  TAB_TITLES, applyTheme, setTheme, setUnits, goTab, refreshPorts,
  readDevice, writeDevice, loadFile, saveFile,
} from './actions.js';
import * as connection from './tabs/connection.js';
import * as source from './tabs/source.js';
import * as speed from './tabs/speed.js';
import * as sensors from './tabs/sensors.js';
import * as motors from './tabs/motors.js';
import * as tach from './tabs/tach.js';
import * as outputs from './tabs/outputs.js';
import * as display from './tabs/display.js';
import * as faults from './tabs/faults.js';
import * as splash from './tabs/splash.js';
import * as test from './tabs/test.js';

const NAV = [
  ['Device', ['connection']],
  ['Inputs', ['source', 'speed', 'sensors']],
  ['Outputs', ['motors', 'tach', 'outputs', 'display', 'faults', 'splash']],
  ['Bench', ['test']],
];

const DESC = {
  connection: 'Serial link to the gauge, and one-click firmware updates from a .hex file.',
  source: 'Choose where each signal comes from. Settings that only apply to one source stay disabled until that source is selected.',
  speed: 'Vehicle speed sensor, engine RPM pickup and the mechanical odometer.',
  sensors: 'Thermistor and fuel sender curves, analog input types and filtering.',
  motors: 'Function, sweep and dial range for each gauge motor, including the Motor S speedometer.',
  tach: 'LED count, RPM zones and colors for the shift light strip.',
  outputs: 'Low side outputs on D12 and D13: on/off with hysteresis, or a PWM curve.',
  display: 'Gauge units, the screen order for both OLEDs, the clock and the fuel tank.',
  faults: 'Thresholds and delays for the warning flashes. A reading must stay past its threshold for the delay before the warning activates.',
  splash: 'Two 128×32 images for the OLED displays. Images are converted to 1-bit before upload.',
  test: "Inject signal values into the gauge. Sending a value temporarily switches that signal's source to Serial in the gauge's RAM. The original source is restored automatically when you leave this tab, disconnect, read, write or close the tool, and is never saved to EEPROM from here.",
};

const TABS = { connection, source, speed, sensors, motors, tach, outputs, display, faults, splash, test };
const FLAT = NAV.flatMap(([group, ids]) => ids.map((id) => ({ id, group })));

// ---- shell ------------------------------------------------------------------------

function seg(name, current, options, pick) {
  return html`<div class="seg" role="radiogroup">${options.map(([value, label]) => html`<label class="seg-opt"><input type="radio" name="${name}" value="${value}"${attr('checked', current === value)}${on({ change: () => pick(value) })}>${label}</label>`)}</div>`;
}

function header(dirtyCount) {
  const { conn } = S;
  const offline = !conn.connected;
  return html`
    <div class="app-title"><b>Gauge V4</b><span>Configuration Tool</span></div>
    <div class="conn${conn.connected ? ' is-on' : ''}">${conn.connected ? `Connected · ${conn.port} · ${conn.baud}` : 'Disconnected'}</div>
    <div class="ctl"><span class="label">Theme</span>${seg('tool-theme', S.prefs.theme, [['dark', 'Dark'], ['light', 'Light']], setTheme)}</div>
    <div class="ctl"><span class="label">Tool units</span>${seg('tool-units', S.prefs.units, [['metric', 'Metric'], ['imperial', "'Merican"]], setUnits)}</div>
    <div class="actions">
      ${button('Load from file…', { kind: 'ghost', iconName: 'folder-open', h: on({ click: () => loadFile() }) })}
      ${button('Save to file…', { kind: 'ghost', iconName: 'save', h: on({ click: () => saveFile() }) })}
      ${button('Read from device', { kind: 'secondary', iconName: 'download', h: on({ click: () => readDevice() }), disabled: offline })}
      <button type="button" class="btn btn-primary blueprint"${on({ click: () => writeDevice() })}${attr('disabled', offline)}>
        <i class="corner tl"></i><i class="corner tr"></i><i class="corner bl"></i><i class="corner br"></i>
        ${icon('upload')}Write to device${dirtyCount ? html`<span class="count">${dirtyCount}</span>` : ''}
      </button>
    </div>`;
}

function sidebar(counts) {
  return NAV.map(([group, ids]) => html`<div class="nav-group">
    <h6>${group}</h6>
    ${ids.map((id) => html`<button type="button" class="nav-item"${attr('aria-current="page"', S.tab === id)}${on({ click: () => goTab(id) })}>
      <span>${TAB_TITLES[id]}</span>${counts[id] ? html`<span class="badge" title="Unsaved changes">${counts[id]}</span>` : ''}
    </button>`)}
  </div>`);
}

function pageHead() {
  const index = FLAT.findIndex(({ id }) => id === S.tab);
  return html`<div class="page-head">
    <h6>${String(index + 1).padStart(2, '0')} / ${FLAT[index].group}</h6>
    <h2>${TAB_TITLES[S.tab]}</h2>
    <p>${DESC[S.tab]}</p>
  </div>`;
}

function footer(dirtyCount) {
  return html`<span>${S.status}</span>
    <span class="muted">${dirtyCount ? `${dirtyCount} unsaved change${dirtyCount > 1 ? 's' : ''}` : 'No unsaved changes'}</span>
    <span class="muted">gauge-config-tool v${S.st ? S.st.version : ''}</span>`;
}

function modal() {
  const m = S.modal;
  if (!m) return '';
  return html`<div class="modal-backdrop">
    <div class="modal panel blueprint" role="dialog" aria-modal="true" aria-labelledby="modal-title">
      <i class="corner tl"></i><i class="corner tr"></i><i class="corner bl"></i><i class="corner br"></i>
      <div class="panel-head"><h4 id="modal-title">${m.title}</h4></div>
      ${m.message ? html`<p class="message">${m.message}</p>` : ''}
      ${m.busy ? progress(0, true) : ''}
      <div class="actions">${m.buttons.map((b) => button(b.label, {
        kind: b.kind || 'secondary',
        disabled: b.value === 'cancel' && m.cancelling,
        cls: 'modal-btn',
        h: on({ click: () => (b.onClick ? b.onClick() : closeModal(b.value)) }),
      }))}</div>
    </div>
  </div>`;
}

// ---- rendering -------------------------------------------------------------------------

const logLengths = new WeakMap();
let queued = false;

function renderNow() {
  queued = false;
  if (!S.st) return;
  resetHandlers();
  const dirty = visibleDirtyParams();
  const counts = {};
  dirty.forEach((p) => { const t = tabOf(p); counts[t] = (counts[t] || 0) + 1; });
  morph(document.getElementById('header'), header(dirty.length));
  morph(document.getElementById('sidebar'), html`${sidebar(counts)}`);
  morph(document.getElementById('page'), html`${pageHead()}${TABS[S.tab].render()}`);
  morph(document.getElementById('footer'), footer(dirty.length));
  const modalRoot = document.getElementById('modal');
  const hadModal = !!modalRoot.firstElementChild;
  morph(modalRoot, modal());
  if (S.modal && S.modal.focus) {
    S.modal.focus = false;
    const buttons = modalRoot.querySelectorAll('.modal-btn');
    const target = [...buttons].reverse().find((b) => b.classList.contains('btn-primary')) || buttons[buttons.length - 1];
    if (target) target.focus();
  }
  document.getElementById('page-scroll').inert = !!S.modal;
  if (hadModal && !S.modal) document.body.focus();
  // Keep logs scrolled to the newest line.
  document.querySelectorAll('.log').forEach((el) => {
    if (logLengths.get(el) !== el.childElementCount) {
      logLengths.set(el, el.childElementCount);
      el.scrollTop = el.scrollHeight;
    }
  });
}

function render() {
  if (queued) return;
  queued = true;
  requestAnimationFrame(renderNow);
}

setRenderer(render);

// ---- events ---------------------------------------------------------------------------

['input', 'change', 'click', 'keydown'].forEach((type) => document.addEventListener(type, dispatch));

document.addEventListener('keydown', (e) => {
  if (e.key !== 'Escape' || !S.modal) return;
  const cancel = S.modal.buttons.find((b) => b.value === false || b.value === 'cancel') || S.modal.buttons[0];
  if (cancel.onClick) cancel.onClick();
  else closeModal(cancel.value);
});

/** Events pushed from Python (api.Api._emit). */
window.onGaugeEvent = (evt) => {
  switch (evt.type) {
    case 'progress':
      if (evt.target === 'firmware') S.fw.pct = evt.pct;
      else if (evt.target === 'splash1' || evt.target === 'splash2') S.splash[evt.target.slice(-1)].pct = evt.pct;
      break;
    case 'log':
      if (evt.target === 'firmware') S.fw.log = [...S.fw.log, evt.text].slice(-300);
      break;
    case 'status':
      S.status = evt.text;
      break;
    case 'connect':
      if (S.modal && S.modal.busy && !S.modal.cancelling) S.modal.message = evt.text;
      break;
    case 'disconnected':
      connection.onFirmwareDisconnect();
      break;
    default:
      return;
  }
  requestRender();
};

// The shift zone blinks at 300 ms while the preview RPM is past Flash RPM.
setInterval(() => {
  S.tach.blink = !S.tach.blink;
  if (S.tab === 'tach' && tach.isBlinking()) requestRender();
}, 300);

// ---- start ----------------------------------------------------------------------------

async function start() {
  await pywebviewReady();
  const [st, prefs] = await Promise.all([api.get_static(), api.get_prefs()]);
  S.st = st;
  S.prefs = { ...S.prefs, ...prefs };
  S.raw = { ...st.tachColors };
  applyTheme(S.prefs.theme);
  test.initTest();
  renderNow();
  document.documentElement.classList.remove('booting');
  await Promise.all([refreshPorts(), connection.initFirmware()]);
  requestRender();
}

start();
