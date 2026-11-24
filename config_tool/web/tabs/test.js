// 11 / Tests: bench testing — signal injection and other device tests.

import { html, on, panel, fields, field, textInput, button, actions, range, logView } from '../ui.js';
import { S, imperial, requestRender, sourceLabel } from '../state.js';
import { api, failed, alertBox, requireConnection } from '../bridge.js';
import { testLog, startOdoUi, stopOdoUi } from '../actions.js';
import { pyFloat, pyInt } from '../units.js';

// Imperial -> metric, as gui.py's _TEST_SIGNALS
const TO_METRIC = {
  speed: (v) => v * 1.609344,
  oil: (v) => v * 6.894757,
  coolant: (v) => (v - 32) / 1.8,
};
const toMetric = (key, v) => (TO_METRIC[key] ? TO_METRIC[key](v) : v);
const fromMetric = (key, v) => {
  if (!TO_METRIC[key]) return v;
  const offset = TO_METRIC[key](0);
  return (v - offset) / (TO_METRIC[key](1) - offset);
};

export function initTest() {
  S.st.testSignals.forEach(([key, , , , , , dflt]) => { S.test.values[key] = dflt; });
}

/** Displayed value in the selected units (gui.py rounds to whole units). */
function shown(key) {
  if (key in S.test.texts) return S.test.texts[key];
  return String(Math.round(imperial() ? fromMetric(key, S.test.values[key]) : S.test.values[key]));
}

async function send(spec) {
  const [key, name, , , mRange, iRange] = spec;
  if (!requireConnection()) return;
  const text = shown(key);
  let value;
  try { value = pyFloat(text); } catch { alertBox('Invalid value', `'${text}' is not a number.`); return; }
  const [lo, hi] = imperial() ? iRange : mRange;
  if (!(value >= lo && value <= hi)) { alertBox('Out of range', `${name} must be between ${lo} and ${hi}.`); return; }
  const metric = imperial() ? toMetric(key, value) : value;
  const res = await api.test_signal(key, metric);
  if (failed(res, 'Test failed')) return;
  testLog(res.log);
  if (!res.sent) S.status = 'Test failed: see the log.';
  requestRender();
}

async function logResult(promise) {
  const res = await promise;
  if (failed(res, 'Test failed')) return null;
  testLog(res.log);
  requestRender();
  return res;
}

async function toggleOdo() {
  if (!requireConnection()) return;
  if (S.test.odoMoving) {
    await logResult(api.test_odo_stop());
    stopOdoUi();
    requestRender();
    return;
  }
  let revs;
  try { revs = pyInt(S.test.odoRevs); } catch { alertBox('Invalid value', 'Revolutions must be an integer.'); return; }
  if (!(revs >= -100 && revs <= 100)) { alertBox('Out of range', 'Revolutions must be between -100 and 100.'); return; }
  if (revs === 0) return;
  const res = await logResult(api.test_odo(revs));
  if (res && res.moving) { startOdoUi(res.ms); requestRender(); }
}

const ODO_PROFILE_TIP = 'Runs the gauge firmware 1-mile odometer accuracy test: 5 s after you click, '
  + 'speed ramps to 60 mph, holds, and ramps back to 0 (about 70 s total), driving the odometer motor and '
  + 'trip odometer exactly 1 mile. Compare both against 1.0 mi to check calibration. Speed source is '
  + 'switched in RAM only; Restore sources (or leaving this tab) puts it back.';

async function restore() {
  await logResult(api.restore_sources());
}

function signalRow(spec) {
  const [key, name, mUnit, iUnit, mRange, iRange, , , source] = spec;
  const imp = imperial();
  const [lo, hi] = imp ? iRange : mRange;
  const unit = imp ? iUnit : mUnit;
  const offline = !S.conn.connected;
  let sliderValue = lo;
  try { sliderValue = Math.min(hi, Math.max(lo, pyFloat(shown(key)))); } catch { /* invalid text */ }
  const setFromDisplay = (text) => {
    try {
      const v = pyFloat(text);
      if (!Number.isFinite(v)) throw new Error('not finite');
      S.test.values[key] = imp ? toMetric(key, v) : v;
      delete S.test.texts[key];
      if (String(Math.round(v)) !== text.trim()) S.test.texts[key] = text;
    } catch {
      S.test.texts[key] = text;
    }
    requestRender();
  };
  const slider = on({ input: (_e, el) => setFromDisplay(String(Math.round(Number(el.value)))) });
  const entry = on({
    input: (_e, el) => setFromDisplay(el.value),
    keydown: (e) => { if (e.key === 'Enter') send(spec); },
  });
  return html`<div class="signal-row">
    <span class="name">${name} <small>source: ${sourceLabel(source)}</small></span>
    ${range({ value: sliderValue, min: lo, max: hi, h: slider, label: name })}
    ${textInput({ value: shown(key), h: entry, label: `${name} value` })}
    <span class="unit">${unit}</span>
    ${button('Send', { h: on({ click: () => send(spec) }), disabled: offline })}
  </div>`;
}

export function render() {
  const offline = !S.conn.connected;
  const injection = panel({
    title: 'Signal Injection',
    wide: true,
    head: button('Restore sources now', { h: on({ click: () => restore() }), disabled: offline }),
    body: html`<div class="signal-list">${S.st.testSignals.map(signalRow)}</div>`,
  });
  const other = panel({
    title: 'Other Tests',
    body: [
      fields([field({
        label: 'Odometer motor revs',
        unit: '-100 to 100',
        control: textInput({ value: S.test.odoRevs, label: 'Odometer motor revs', h: on({ input: (_e, el) => { S.test.odoRevs = el.value; } }) }),
      })]),
      actions([
        button('Ping', { h: on({ click: () => logResult(api.ping()) }), disabled: offline }),
        button('Version', { h: on({ click: () => logResult(api.version()) }), disabled: offline }),
        button(S.test.odoMoving ? 'Stop odometer' : 'Move odometer', { h: on({ click: () => toggleOdo() }), disabled: offline }),
        button('1-mile odometer test', { h: on({ click: () => requireConnection() && logResult(api.test_odo_profile()) }), disabled: offline, title: ODO_PROFILE_TIP }),
        button('Splash slot 1', { h: on({ click: () => logResult(api.test_splash(1)) }), disabled: offline }),
        button('Splash slot 2', { h: on({ click: () => logResult(api.test_splash(2)) }), disabled: offline }),
      ]),
    ],
  });
  const log = panel({ title: 'Log', body: logView(S.test.log) });
  return html`<div class="panel-grid">${injection}${other}${log}</div>`;
}
