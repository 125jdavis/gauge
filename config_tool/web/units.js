// JS ports of the gui_model.py helpers the page needs to show and enter values.
// Python stays the authority: write/save re-validate everything there. These ports
// keep Python's rounding (round half to even) and messages so the UI matches gui.py.

// ---- Python number semantics -------------------------------------------------------

/** Python round(): halves go to the even neighbour. */
export function pyRound(x) {
  return Math.abs(x % 1) === 0.5 ? 2 * Math.round(x / 2) : Math.round(x);
}

/** Python int(text) for base-10 text; throws Error on anything else. */
export function pyInt(text) {
  const s = String(text).trim();
  if (!/^[+-]?\d+$/.test(s)) throw new Error(`invalid literal for int(): '${text}'`);
  return parseInt(s, 10);
}

/** Python float(text); throws Error when Python would raise ValueError. */
export function pyFloat(text) {
  const s = String(text).trim().toLowerCase();
  if (/^[+-]?(inf|infinity)$/.test(s)) return s.startsWith('-') ? -Infinity : Infinity;
  if (/^[+-]?nan$/.test(s)) return NaN;
  if (!/^[+-]?(\d+\.?\d*|\.\d+)(e[+-]?\d+)?$/.test(s)) throw new Error(`could not convert string to float: '${text}'`);
  return Number(s);
}

const rstripZeros = (s) => s.replace(/0+$/, '').replace(/\.$/, '');

/** gui_model._trim_number */
export const trimNumber = (value, decimals = 3) => rstripZeros(value.toFixed(decimals));
/** gui_model._display_number */
export const displayNumber = (value) => trimNumber(value, 2);
/** Stored text for float params (oil/coolant warnings, fuel capacity) */
export const floatText = (value) => rstripZeros(value.toFixed(2));

function fail(message) { throw new Error(message); }

function num(text, message) {
  try { return pyFloat(text); } catch { return fail(message); }
}

// ---- tool-wide units (firmware always stores metric) -----------------------------

export const KMH_PER_MPH = 1.609344;
export const PSI_PER_KPA = 0.1450377377;
export const LITERS_PER_GALLON = 3.785411784;
const IMPERIAL_UNIT_LABELS = { temp: '°F', speed: 'mph', pressure: 'PSI', pressure_abs: 'PSI (abs)' };
const METRIC_UNIT_LABELS = { temp: '°C', speed: 'km/h', pressure: 'kPa', pressure_abs: 'kPa' };

// Scaled kinds: the firmware stores lambda x1000 and volts x10; the tool shows real values in both systems.
const SCALED = { lambda: { div: 1000, dp: 3, label: 'λ' }, volts: { div: 10, dp: 1, label: 'V' } };

/** Relabel the x1000 / x10 parameters from get_static() and give them scaled kinds. Indices are unchanged. */
export function normalizeScaledParams(st) {
  const scaledKind = (label) => (/lambda/i.test(label) ? 'lambda' : /volt/i.test(label) ? 'volts' : null);
  const clean = (label) => label.replace(/\s*\((x|×)\s*\d+\)\s*$/i, '');
  st.outputParams.forEach((label, i) => {
    const kind = scaledKind(label);
    if (!kind) return;
    st.outputKinds[i] = kind;
    st.outputUnits[i] = SCALED[kind].label;
    st.outputParams[i] = clean(label);
  });
  st.motorFunctions = st.motorFunctions.map(clean);
}

export function unitLabel(kind, imperial, metricLabel = '') {
  if (kind == null) return metricLabel;
  if (SCALED[kind]) return SCALED[kind].label;
  return (imperial ? IMPERIAL_UNIT_LABELS : METRIC_UNIT_LABELS)[kind];
}

/** Stored metric integer -> text in the selected system (non-numbers pass through). */
export function rawToDisplay(text, kind, imperial) {
  if (SCALED[kind]) {
    try { return trimNumber(pyInt(text) / SCALED[kind].div, SCALED[kind].dp); } catch { return text; }
  }
  if (!imperial || kind == null) return text;
  let raw;
  try { raw = pyInt(text); } catch { return text; }
  let value;
  if (kind === 'temp') value = raw * 9 / 5 + 32;
  else if (kind === 'speed') value = raw / KMH_PER_MPH;
  else value = raw * PSI_PER_KPA;
  return trimNumber(value, 1);
}

/** Text in the selected system -> stored metric integer. Throws Error. */
export function displayToRaw(text, kind, imperial) {
  const s = String(text).trim();
  if (SCALED[kind]) {
    const value = pyFloat(s);
    if (!Number.isFinite(value)) fail('not a number');
    return pyRound(value * SCALED[kind].div);
  }
  if (!imperial || kind == null) return pyInt(s);
  const value = pyFloat(s);
  if (!Number.isFinite(value)) fail('not a number');
  if (kind === 'temp') return pyRound((value - 32) * 5 / 9);
  if (kind === 'speed') return pyRound(value * KMH_PER_MPH);
  return pyRound(value / PSI_PER_KPA);
}

// ---- filters ----------------------------------------------------------------------

export function filterPercentToRaw(percent) {
  if (!(percent >= 0 && percent <= 99)) fail('Filter percentage must be between 0 and 99.');
  return Math.min(127, Math.floor((percent * 128 + 50) / 100));
}

export function filterRawToPercent(raw) {
  if (!(raw >= 0 && raw <= 127)) fail('Filter coefficient must be between 0 and 127.');
  return Math.min(99, Math.floor((raw * 100 + 64) / 128));
}

// ---- motors -------------------------------------------------------------------------

export const MOTOR_STEPS_PER_DEGREE = 12;
export const MOTOR_MAX_ANGLE = 315;
const SWEEP_STEPS = [100, MOTOR_MAX_ANGLE * MOTOR_STEPS_PER_DEGREE];

export function angleToSteps(text) {
  const angle = num(text, 'Sweep angle must be a number.');
  if (!Number.isFinite(angle)) fail('Sweep angle must be a finite number.');
  const steps = pyRound(angle * MOTOR_STEPS_PER_DEGREE);
  if (!(steps >= SWEEP_STEPS[0] && steps <= SWEEP_STEPS[1])) {
    fail(`Sweep angle must be between ${(SWEEP_STEPS[0] / MOTOR_STEPS_PER_DEGREE).toFixed(1)} and ${MOTOR_MAX_ANGLE} degrees.`);
  }
  return steps;
}

export const stepsToAngle = (steps) => displayNumber(steps / MOTOR_STEPS_PER_DEGREE);

/** (min, max) stored for the user's low/high; CCW stores them swapped. */
export const gaugeRangeToDevice = (low, high, ccw) => (ccw ? [high, low] : [low, high]);

/** [low, high, ccw] for stored (min, max). */
export function gaugeRangeFromDevice(min, max) {
  try {
    if (pyInt(min) > pyInt(max)) return [max, min, true];
  } catch { /* not numbers yet */ }
  return [min, max, false];
}

// ---- analog inputs ------------------------------------------------------------------

export const AV_TYPE_100PSI_G = 2;
export const AV_TYPE_LAMBDA = 5;
/** Sensor types whose calibration is shown and entered as gauge pressure (gui_model.AV_GAUGE_TYPES) */
export const AV_GAUGE_TYPES = [AV_TYPE_100PSI_G];
/** Types that keep their type when a calibration point is edited (gui_model.AV_UNIT_TYPES) */
export const AV_UNIT_TYPES = [AV_TYPE_LAMBDA, ...AV_GAUGE_TYPES];
const PHYS_UNIT_SCALE = {
  kPa: 10, Lambda: 1000, 'PSI (abs)': 10 / PSI_PER_KPA,
  'kPa (gauge)': 10, 'PSI (gauge)': 10 / PSI_PER_KPA,
};
// Gauge units are entered relative to atmosphere (101.3 kPa); stored raw = phys * scale + offset
const PHYS_UNIT_OFFSET = { 'kPa (gauge)': 1013, 'PSI (gauge)': 1013 };

export function avPhysicalUnit(avType, imperial) {
  if (avType === AV_TYPE_LAMBDA) return 'Lambda';
  if (AV_GAUGE_TYPES.includes(avType)) return imperial ? 'PSI (gauge)' : 'kPa (gauge)';
  return imperial ? 'PSI (abs)' : 'kPa';
}

/** gui_model.av_physical_text: PSI shown to 0.01, the stored resolution */
export const avPhysicalText = (raw, unit) =>
  trimNumber(rawToPhysical(raw, unit), unit.startsWith('PSI') ? 2 : 3);

export function voltsToAdc(text) {
  const volts = num(text, 'Voltage must be a number from 0 to 5 V.');
  if (!Number.isFinite(volts) || !(volts >= 0 && volts <= 5)) fail('Voltage must be from 0 to 5 V.');
  return pyRound(volts * 1023 / 5);
}

export const adcToVolts = (adc) => adc * 5 / 1023;

export function physicalToRaw(text, unit) {
  const phys = num(text, `Physical value must be a number in ${unit}.`);
  const offset = PHYS_UNIT_OFFSET[unit] ?? 0;
  const raw = Number.isFinite(phys) ? pyRound(phys * PHYS_UNIT_SCALE[unit] + offset) : 32768;
  if (!(raw >= -32768 && raw <= 32767)) fail(`Physical value is out of range for ${unit}.`);
  return raw;
}

export const rawToPhysical = (raw, unit) => (raw - (PHYS_UNIT_OFFSET[unit] ?? 0)) / PHYS_UNIT_SCALE[unit];

// ---- sensor curves -------------------------------------------------------------------

export function dividerMillivolts(resistance, bias) {
  if (bias <= 0 || resistance < 0) fail('Bias must be above 0 ohms and resistance cannot be negative.');
  return pyRound(5000 * resistance / (resistance + bias));
}

/** Curve voltage preview text, or '—' when the inputs are incomplete. */
export function previewMillivolts(resText, biasText) {
  try { return String(dividerMillivolts(pyInt(resText), pyInt(biasText))); } catch { return '—'; }
}

// ---- speed, warnings, fuel ---------------------------------------------------------

export function hallSpeedMinToRaw(text) {
  const speed = num(text, 'Minimum Speed Cutoff must be a number from 0 to 1 km/h.');
  if (!Number.isFinite(speed) || !(speed >= 0 && speed <= 1)) fail('Minimum Speed Cutoff must be from 0 to 1 km/h.');
  return pyRound(speed * 100);
}

export function speedoMaxToRaw(text, units) {
  let speed = num(text, 'Speedometer Max must be a number.');
  if (!Number.isFinite(speed) || speed <= 0) fail('Speedometer Max must be a positive number.');
  if (units === 'km/h') speed /= KMH_PER_MPH;
  const raw = pyRound(speed * 100);
  if (!(raw >= 1000 && raw <= 30000)) fail('Speedometer Max must be between 10 and 300 mph (or the equivalent in km/h).');
  return raw;
}

export function speedoMaxDisplay(rawText, units) {
  let raw;
  try { raw = pyInt(rawText); } catch { return rawText; }
  let value = raw / 100;
  if (units === 'km/h') value *= KMH_PER_MPH;
  return displayNumber(value);
}

export function oilWarningToRaw(text, units) {
  let pressure = num(text, 'Oil pressure warning must be a number.');
  if (!Number.isFinite(pressure)) fail('Oil pressure warning must be a finite number.');
  if (units === 'PSI') pressure /= PSI_PER_KPA;
  if (!(pressure >= 0 && pressure <= 1000)) fail('Oil pressure warning must be between 0 and 1000 kPa.');
  return pressure;
}

export function temperatureWarningToRaw(text, units) {
  let temperature = num(text, 'Engine temperature warning must be a number.');
  if (!Number.isFinite(temperature)) fail('Engine temperature warning must be a finite number.');
  if (units === 'F') temperature = (temperature - 32) * 5 / 9;
  if (!(temperature >= 50 && temperature <= 200)) fail('Engine temperature warning must be between 50 and 200 C.');
  return temperature;
}

/** gui_model._warning_display_value; non-numbers pass through. */
export function warningDisplayValue(rawText, param, units) {
  let value;
  try { value = pyFloat(rawText); } catch { return rawText; }
  if (param === 'oil_warn_kpa' && units === 'PSI') value *= PSI_PER_KPA;
  else if (param === 'coolant_warn_c' && units === 'F') value = value * 9 / 5 + 32;
  return displayNumber(value);
}

export function fuelCapacityToRaw(text, units) {
  let capacity = num(text, 'Fuel tank capacity must be a number.');
  if (!Number.isFinite(capacity)) fail('Fuel tank capacity must be a finite number.');
  if (units === 'L') capacity /= LITERS_PER_GALLON;
  if (!(capacity >= 1 && capacity <= 200)) fail('Fuel tank capacity must be between 1 and 200 gallons (or the equivalent in liters).');
  return capacity;
}

export function fuelCapacityDisplay(rawText, units) {
  let capacity;
  try { capacity = pyFloat(rawText); } catch { return rawText; }
  if (units === 'L') capacity *= LITERS_PER_GALLON;
  return displayNumber(capacity);
}

export const WARN_DEBOUNCE_MAX_S = 60;

export function debounceSecondsToMs(text) {
  const seconds = num(text, `Delay must be a number of seconds from 0 to ${WARN_DEBOUNCE_MAX_S}.`);
  if (!Number.isFinite(seconds) || !(seconds >= 0 && seconds <= WARN_DEBOUNCE_MAX_S)) {
    fail(`Delay must be from 0 to ${WARN_DEBOUNCE_MAX_S} seconds.`);
  }
  return pyRound(seconds * 1000);
}

export function debounceMsToSeconds(text) {
  try { return trimNumber(pyInt(text) / 1000, 3); } catch { return text; }
}

// ---- LED tach -----------------------------------------------------------------------

export function tachColor(value) {
  if (!/^[0-9a-fA-F]{6}$/.test(value || '')) fail('Tach colors must be six hexadecimal RGB digits.');
  return value.toUpperCase();
}

/** colorsys.rgb_to_hsv on a tach colour: [h 0-1, s, v]. */
export function tachColorHsv(value) {
  const c = tachColor(value);
  const [r, g, b] = [0, 2, 4].map((i) => parseInt(c.slice(i, i + 2), 16) / 255);
  const maxc = Math.max(r, g, b);
  const minc = Math.min(r, g, b);
  if (minc === maxc) return [0, 0, maxc];
  const s = (maxc - minc) / maxc;
  const rc = (maxc - r) / (maxc - minc);
  const gc = (maxc - g) / (maxc - minc);
  const bc = (maxc - b) / (maxc - minc);
  let h;
  if (r === maxc) h = bc - gc;
  else if (g === maxc) h = 2 + rc - bc;
  else h = 4 + gc - rc;
  return [(((h / 6) % 1) + 1) % 1, s, maxc];
}

/** colorsys.hsv_to_rgb */
function hsvToRgb(h, s, v) {
  if (s === 0) return [v, v, v];
  let i = Math.trunc(h * 6);
  const f = h * 6 - i;
  const p = v * (1 - s);
  const q = v * (1 - s * f);
  const t = v * (1 - s * (1 - f));
  i %= 6;
  return [[v, t, p], [q, v, p], [p, v, t], [p, q, v], [t, p, v], [v, p, q]][i];
}

const hex2 = (channel) => pyRound(channel * 255).toString(16).toUpperCase().padStart(2, '0');

/** gui_model.tach_hsv_color: hue 0-359, brightness 0-100 -> LED RGB hex. */
export function tachHsvColor(hue, brightness) {
  return hsvToRgb((((hue % 360) + 360) % 360) / 360, 1, brightness / 100).map(hex2).join('');
}

/** LED brightness 0-100 % shown as screen brightness 60-100 %. */
export function tachDisplayColor(value, blackHue = 0) {
  let [h, s, v] = tachColorHsv(value);
  if (v === 0) {
    h = (((blackHue % 360) + 360) % 360) / 360;
    s = 1;
  }
  return `#${hsvToRgb(h, s, 0.6 + 0.4 * v).map(hex2).join('')}`;
}

export const tachSteps = (count, sweep) => (sweep < 2 ? count : Math.floor((count + 1) / 2));

export function tachLedStep(index, count, sweep) {
  if (sweep === 0) return index;
  if (sweep === 1) return count - 1 - index;
  const edge = Math.min(index, count - 1 - index);
  return sweep === 2 ? edge : tachSteps(count, sweep) - 1 - edge;
}

export function tachStepRpm(step, steps, minimum, maximum) {
  if (steps <= 1) return minimum;
  return minimum + Math.floor((step * (maximum - minimum) + steps - 2) / (steps - 1));
}

/** gui_model.tach_values; throws Error with the Tk UI's message. */
export function tachValues(get) {
  let count, minimum, maximum, sweep, warning, shift;
  try {
    [count, minimum, maximum, sweep, warning, shift] = ['num_leds', 'tach_min', 'tach_max', 'tach_sweep', 'warn_leds', 'shift_leds']
      .map((p) => pyInt(get(p)));
  } catch {
    fail('Read or enter all tach settings before adjusting the sliders.');
  }
  if (!(count >= 1 && count <= 64 && minimum >= 0 && minimum <= 5000
        && maximum >= 1000 && maximum <= 15000 && minimum <= maximum && sweep >= 0 && sweep <= 3)) {
    fail('Check LED count, RPM limits, and sweep style.');
  }
  if (!(shift >= 0 && shift <= warning && warning < tachSteps(count, sweep))) {
    fail('Zone settings exceed the strip or warning starts after shift.');
  }
  return { count, minimum, maximum, sweep, warning, shift };
}

// ---- low side outputs -----------------------------------------------------------------

function outputInt(text, name, low, high) {
  let value;
  try { value = pyInt(String(text)); } catch { fail(`${name} must be a whole number.`); }
  if (!(value >= low && value <= high)) fail(`${name} must be from ${low} to ${high}.`);
  return value;
}

export function validateOnOff(onOp, onVal, offVal, fmt = String) {
  const op = outputInt(onOp, 'On comparison', 0, 1);
  const on = outputInt(onVal, 'On value', -32768, 32767);
  const off = outputInt(offVal, 'Off value', -32768, 32767);
  if (op === 0 && !(off < on)) fail(`Turns on above ${fmt(on)}, so the off value must be below ${fmt(on)}.`);
  if (op === 1 && !(off > on)) fail(`Turns on below ${fmt(on)}, so the off value must be above ${fmt(on)}.`);
}

export function validatePwmCurve(freq, xs, duties, fmt = String) {
  outputInt(freq, 'PWM frequency (Hz)', 20, 500);
  const values = xs.map((x, i) => outputInt(x, `Point ${i + 1} value`, -32768, 32767));
  duties.forEach((d, i) => outputInt(d, `Point ${i + 1} duty (%)`, 0, 100));
  for (let i = 1; i < values.length; i++) {
    if (values[i] <= values[i - 1]) {
      fail(`Point ${i + 1} value (${fmt(values[i])}) must be greater than point ${i} (${fmt(values[i - 1])}).`);
    }
  }
}

/** Error message for output n (as gui_model.validate_outputs words it), or ''. */
export function outputError(n, get, imperial, st) {
  const p = `out${n}_`;
  const modeText = get(`${p}mode`);
  if (modeText === '') return '';
  try {
    const mode = outputInt(modeText, 'Mode', 0, st.outputModes.length - 1);
    if (mode === 0) return '';
    const param = outputInt(get(`${p}param`), 'Parameter', 1, st.outputParams.length);
    const kind = st.outputKinds[param - 1];
    const unit = unitLabel(kind, imperial, st.outputUnits[param - 1]);
    const fmt = (raw) => `${rawToDisplay(String(raw), kind, imperial)} ${unit}`.trim();
    if (mode === 1) {
      validateOnOff(get(`${p}on_op`), get(`${p}on_val`), get(`${p}off_val`), fmt);
    } else {
      const points = [...Array(st.outputPwmPoints).keys()];
      validatePwmCurve(get(`${p}freq`), points.map((i) => get(`${p}x${i}`)), points.map((i) => get(`${p}d${i}`)), fmt);
    }
  } catch (exc) {
    return `Output ${n}: ${exc.message}`;
  }
  return '';
}

// ---- display screens -------------------------------------------------------------------

/** gui_model.parse_screen_order: unique valid IDs in order. */
export function parseScreenOrder(text, validIds) {
  const result = [];
  String(text).split(',').forEach((part) => {
    const s = part.trim();
    if (/^\d+$/.test(s)) {
      const id = parseInt(s, 10);
      if (validIds.includes(id) && !result.includes(id)) result.push(id);
    }
  });
  return result;
}
