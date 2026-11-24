// Application state and the helpers tabs use to bind controls to parameters.
//
// raw:   last values read from the device (stored firmware units, always metric).
// edits: pending changes, also in stored units. A parameter is dirty while it is in
//        edits, and setVal() drops an edit that returns to the raw value.
// bad:   text the user typed that does not convert to a stored value yet
//        ({param: {text, error}}); it blocks Write/Save until fixed.
// Display conversion happens only when rendering and parsing, so switching units
// never changes stored values.

import { html, on, field, textInput, select, range } from './ui.js';
import { filterPercentToRaw, filterRawToPercent, pyInt } from './units.js';

export const S = {
  st: null,                       // get_static() from Python
  prefs: { theme: 'dark', units: 'imperial' },
  raw: {},
  edits: {},
  bad: {},
  tab: 'connection',
  status: 'Disconnected',
  conn: { connected: false, port: '', baud: '115200', version: '', state: 'Disconnected' },
  ports: [],
  fw: { path: '', log: [], pct: 0, busy: false, available: true },
  splash: { 1: { png: '', name: '', pct: 0 }, 2: { png: '', name: '', pct: 0 } },
  tach: { previewRpm: null, blink: true, testOn: false, hues: {} },
  test: { values: {}, texts: {}, odoRevs: '1', odoMoving: false, log: [] },
  // Logging tab: samples are {t: gauge millis, v: metric values in st.logSignals order}.
  // hover holds each trace's crosshair position (0-1 across the plot) or null.
  log: {
    live: false, recording: false, name: '', rows: 0, lastAt: 0, samples: [],
    traces: ['speed', 'rpm', 'lambda', 'map'], hover: [null, null, null, null],
    size: { w: 800, h: 120 },  // measured plot size in px (charts draw at 1:1)
  },
  screens: {},                    // display list UI order: {param: {order, forValue}}
  dirHint: {},                    // motor direction when low == high: {n: ccw}
  modal: null,
};

let renderer = () => {};
export const setRenderer = (fn) => { renderer = fn; };
export const requestRender = () => renderer();

export const imperial = () => S.prefs.units === 'imperial';
export const val = (p) => (p in S.edits ? S.edits[p] : (S.raw[p] ?? ''));
export const merged = () => ({ ...S.raw, ...S.edits });
export const isDirty = (p) => p in S.edits || p in S.bad;
export const intOr = (p, fallback = null) => { try { return pyInt(val(p)); } catch { return fallback; } };

export function setVal(p, value) {
  const text = String(value);
  if (text === (S.raw[p] ?? '')) delete S.edits[p];
  else S.edits[p] = text;
  delete S.bad[p];
}

export function setMany(values) {
  Object.entries(values).forEach(([p, v]) => setVal(p, v));
}

// ---- which tab owns a parameter (sidebar badges) ------------------------------------

const TAB_RULES = [
  ['source', /^(speed_source|rpm_source|oil_prs_source|fuel_prs_source|coolant_src|oil_temp_src|map_source|lambda_source|fuel_lvl_src|can_protocol)$/],
  ['speed', /^(revs_per_km|teeth_per_rev|hall_speed_min|filter_hall|filter_gps|cyl_count|filter_rpm|rpm_debounce_us|engine_rpm_min|odo_.*)$/],
  ['sensors', /^(therm_.*|fuel_(type|bias|r_.*|v_.*|mid_pct)|filter_(therm|fuel|av\d|vbatt)|vbatt_scaler|av\d_.*)$/],
  ['motors', /^(m\d_.*|ms_.*|motor_sweep_ms|speedo_max)$/],
  ['tach', /^(num_leds|warn_leds|shift_leds|tach_.*)$/],
  ['outputs', /^out\d_/],
  ['display', /^(units|disp\d_order|clock_offset|fuel_capacity)$/],
  ['faults', /^(oil_warn_kpa|coolant_warn_c|batt_warn_v|engine_run_rpm|fuel_warn_pct|.*_warn_ms)$/],
  ['splash', /^splash\d_img$/],
];

export function tabOf(param) {
  const rule = TAB_RULES.find(([, re]) => re.test(param));
  return rule ? rule[0] : 'connection';
}

export function dirtyParams() {
  return [...new Set([...Object.keys(S.edits), ...Object.keys(S.bad)])];
}

/** gui_model.output_param_active: false for output settings the selected mode does not use. */
function outputParamActive(param) {
  const m = /^out(\d)_(.+)$/.exec(param);
  if (!m) return true;
  const mode = val(`out${m[1]}_mode`);
  const fieldName = m[2];
  if (['on_op', 'on_val', 'off_val'].includes(fieldName)) return mode === '1';
  if (fieldName === 'freq' || /^[xd]\d+$/.test(fieldName)) return mode === '2';
  return true;
}

/** Edits the user can see: hidden output settings stay pending but are not counted. */
export function visibleDirtyParams() {
  return dirtyParams().filter(outputParamActive);
}

// ---- source-dependent fields -----------------------------------------------------------

const SOURCE_NAMES = {
  speed_source: 'Speed source', rpm_source: 'RPM source', oil_prs_source: 'Oil pressure source',
  fuel_prs_source: 'Fuel pressure source', coolant_src: 'Coolant temp source',
  oil_temp_src: 'Oil temp source', map_source: 'MAP source', lambda_source: 'Lambda source',
  fuel_lvl_src: 'Fuel level source',
};

export function sourceLabel(param) {
  const value = val(param);
  if (value === '') return 'not set';
  const options = S.st.sources[param];
  const i = Number(value);
  return Number.isInteger(i) && i >= 0 && i < options.length ? options[i] : 'Unsupported source';
}

/** Why *param* is unused with the selected sources ('' when it is in use). */
export function dependencyNote(param) {
  if (param === 'speedo_max') {
    const value = val('speed_source');
    if (value === '') return 'Used when a speed source is selected. Currently not set.';
    return value === '0' ? 'Speed source is Off.' : '';
  }
  const reqs = S.st.sourceDependent[param];
  if (!reqs) return '';
  if (reqs.some(([source, value]) => val(source) === String(value))) return '';
  const [[source, value]] = reqs;
  const option = S.st.sources[source][value];
  if (reqs.length === 1) return `Used when ${SOURCE_NAMES[source]} is ${option}. Currently ${sourceLabel(source)}.`;
  if (reqs.length === 2) {
    const names = reqs.map(([s]) => SOURCE_NAMES[s].replace(/ source$/, '')).join(' or ');
    return `Used when ${names} source is ${option}.`;
  }
  return `Used when a signal source is ${option}. None is currently.`;
}

// ---- binding controls to parameters -------------------------------------------------

/**
 * Input bound to *p*. fmt(stored) -> display text; parse(text) -> stored value (throws
 * Error to mark the text invalid). Without parse the trimmed text is stored as typed,
 * as gui.py did, and Python validation reports problems on write.
 */
export function bind(p, { fmt = (v) => v, parse = null, after = null, onCommit = null } = {}) {
  const bad = S.bad[p];
  const value = bad ? bad.text : fmt(val(p));
  const h = on({
    input: (_e, el) => {
      const text = el.value;
      if (text.trim() === '' || !parse) {
        setVal(p, text.trim());
      } else {
        try {
          setVal(p, parse(text));
        } catch (exc) {
          S.bad[p] = { text, error: exc.message };
          requestRender();
          return;
        }
      }
      if (after) after(p);
      requestRender();
    },
    change: (_e, el) => {
      // Leaving the field shows the value that will actually be stored.
      if (onCommit) onCommit(p);
      if (!S.bad[p]) el.value = fmt(val(p));
      requestRender();
    },
  });
  return { value, h, dirty: isDirty(p), error: bad ? bad.error : '' };
}

const tipOf = (p) => (S.st.tooltips[p] || '');

/** Standard numeric/text field bound to a parameter. */
export function paramField(label, p, opts = {}) {
  const note = opts.note ?? dependencyNote(p);
  const disabled = opts.disabled ?? !!note;
  const b = bind(p, opts);
  return field({
    label, unit: opts.unit, note, error: b.error, disabled, dirty: b.dirty, tip: opts.tip ?? tipOf(p),
    control: textInput({ value: b.value, h: b.h, disabled, dirty: b.dirty, num: opts.num ?? true, cls: opts.cls, label }),
  });
}

/** Index-valued select bound to a parameter; onPick(value) overrides the default store. */
export function selectField(label, p, options, opts = {}) {
  const note = opts.note ?? dependencyNote(p);
  const disabled = opts.disabled ?? !!note;
  const offset = opts.offset ?? 0;
  const hide = opts.hide ?? [];
  // Hidden options keep their index; one already stored in a loaded config still shows.
  const pairs = options.map((text, i) => [String(i + offset), text])
    .filter(([v, text]) => !hide.includes(text) || v === val(p));
  const h = on({
    change: (_e, el) => {
      if (opts.onPick) opts.onPick(el.value);
      else setVal(p, el.value);
      requestRender();
    },
  });
  return field({
    label, unit: opts.unit, note, disabled, dirty: isDirty(p), tip: opts.tip ?? tipOf(p),
    control: select({ value: val(p), options: pairs, h, disabled, dirty: isDirty(p), fallback: opts.fallback }),
  });
}

/** Filter slider: 0-99 % on screen, 0-127 stored. */
export function filterField(label, p, opts = {}) {
  const note = opts.note ?? dependencyNote(p);
  const disabled = !!note;
  let percent = null;
  try { percent = filterRawToPercent(pyInt(val(p))); } catch { /* unset or invalid */ }
  const h = on({
    input: (_e, el) => {
      const pct = Math.max(0, Math.min(99, Math.round(Number(el.value))));
      if (percent !== pct) setVal(p, String(filterPercentToRaw(pct)));
      requestRender();
    },
  });
  const readout = val(p) === '' ? '--' : percent === null ? 'Invalid' : `${percent}%`;
  return field({
    label, note, disabled, dirty: isDirty(p), tip: tipOf(p),
    control: [range({ value: percent ?? 0, min: 0, max: 99, h, disabled, label }),
      html`<span class="readout">${readout}</span>`],
  });
}
