// 05 / Outputs: function, sweep and dial range for each gauge motor, including Motor S.

import { html, on, attr, panel, fields, select, textInput, errorBox, dirtyDot } from '../ui.js';
import { S, val, setVal, bind, paramField, imperial, requestRender, isDirty, intOr, dependencyNote } from '../state.js';
import {
  angleToSteps, stepsToAngle, gaugeRangeToDevice, gaugeRangeFromDevice,
  rawToDisplay, displayToRaw, unitLabel, pyInt, MOTOR_STEPS_PER_DEGREE, MOTOR_MAX_ANGLE,
  speedoMaxToRaw, speedoMaxDisplay,
} from '../units.js';
import { api } from '../bridge.js';

// Motor function -> unit conversion kind (gui_model.OUTPUT_PARAM_KINDS shifted by one for "Off")
const kindOf = (fn) => (fn >= 1 ? S.st.outputKinds[fn - 1] : null);

/** Dial preview: viewBox 0 0 120 108, centre (60, 60), r 44, 0° at 12 o'clock. */
function dial(angle, ccw) {
  const point = (a) => {
    const r = a * Math.PI / 180;
    return [60 + 44 * Math.sin(r), 60 - 44 * Math.cos(r)].map((n) => n.toFixed(2));
  };
  const arc = (a) => {
    const [x1, y1] = point(-a / 2);
    const [x2, y2] = point(a / 2);
    return `M ${x1} ${y1} A 44 44 0 ${a > 180 ? 1 : 0} 1 ${x2} ${y2}`;
  };
  const a = Math.max(8, Math.min(MOTOR_MAX_ANGLE, angle || 0));
  const [nx, ny] = point(ccw ? a / 2 : -a / 2);
  return html`<svg class="dial" viewBox="0 0 120 108" aria-hidden="true">
    <path class="track" d="${arc(MOTOR_MAX_ANGLE)}"></path>
    <path class="sweep" d="${arc(a)}"></path>
    <line class="needle" x1="60" y1="60" x2="${nx}" y2="${ny}"></line>
    <circle class="hub" cx="60" cy="60" r="3.5"></circle>
  </svg>`;
}

/** Current [low, high, ccw] for motor n (low == high keeps the direction last picked). */
function range(n) {
  const [low, high, ccw] = gaugeRangeFromDevice(val(`m${n}_min`), val(`m${n}_max`));
  const sameValue = low !== '' && low === high;
  return [low, high, sameValue && n in S.dirHint ? S.dirHint[n] : ccw];
}

function storeRange(n, low, high, ccw) {
  const [min, max] = gaugeRangeToDevice(String(low), String(high), ccw);
  setVal(`m${n}_min`, min);
  setVal(`m${n}_max`, max);
  S.dirHint[n] = ccw;
}

/** Picking a function seeds typical gauge min/max (GaugeConfigApp._apply_function_defaults). */
function pickFunction(n, value) {
  setVal(`m${n}_func`, value);
  const [lo, hi] = S.st.motorRanges[Number(value)] || [0, 0];
  if (lo !== 0 || hi !== 0) storeRange(n, lo, hi, range(n)[2]);
}

/** Low or High input; edits re-store min/max with the direction applied. Returns {control, error}. */
function endInput(n, which, kind, off) {
  const imp = imperial();
  const [low, high, ccw] = range(n);
  const key = `m${n}_${which}`;            // UI-only key for invalid text
  const current = which === 'lo' ? low : high;
  const badEntry = S.bad[key];
  const param = which === 'lo' ? (ccw ? `m${n}_max` : `m${n}_min`) : (ccw ? `m${n}_min` : `m${n}_max`);
  const h = on({
    input: (_e, el) => {
      const text = el.value.trim();
      let raw;
      try {
        raw = text === '' ? '' : String(displayToRaw(text, kind, imp));
      } catch {
        S.bad[key] = { text: el.value, error: 'Gauge min and max must be whole numbers.' };
        requestRender();
        return;
      }
      delete S.bad[key];
      const [lo, hi, dir] = range(n);
      if (which === 'lo') storeRange(n, raw, hi, dir);
      else storeRange(n, lo, raw, dir);
      requestRender();
    },
    change: (_e, el) => {
      if (!S.bad[key]) el.value = rawToDisplay(which === 'lo' ? range(n)[0] : range(n)[1], kind, imp);
      requestRender();
    },
  });
  const dirty = isDirty(param) || !!badEntry;
  return {
    control: textInput({ value: badEntry ? badEntry.text : rawToDisplay(current, kind, imp), h, dirty, disabled: off, cls: 'compact', label: `Motor ${n} ${which === 'lo' ? 'low' : 'high'}` }),
    error: badEntry ? `Motor ${n}: ${badEntry.error}` : '',
  };
}

/** A table cell. tip becomes the cell's tooltip; off dims it. */
function td(control, { unit = '', tip = '', off = false, dirty = false } = {}) {
  return html`<td${tip ? html` title="${tip}"` : ''}${attr('aria-disabled="true"', off)}><div class="cell">${dirtyDot(dirty)}${control}${unit ? html`<span class="unit">${unit}</span>` : ''}</div></td>`;
}

function motorRow(n, errors) {
  const fn = intOr(`m${n}_func`, 0);
  const kind = kindOf(fn);
  const unit = unitLabel(kind, imperial(), S.st.motorUnits[fn] || '');
  const off = val(`m${n}_func`) === '0';
  const [, , ccw] = range(n);
  let angle = 0;
  try { angle = pyInt(val(`m${n}_sweep`)) / MOTOR_STEPS_PER_DEGREE; } catch { /* unset */ }
  const sweepTip = `Full needle sweep in degrees (Switec Z27.168: ${MOTOR_STEPS_PER_DEGREE} steps per degree). Stored in the config as steps.`;
  const offTip = 'Function is Off: the needle parks at zero.';
  const sweep = bind(`m${n}_sweep`, {
    fmt: (raw) => { try { return stepsToAngle(pyInt(raw)); } catch { return raw; } },
    parse: (t) => angleToSteps(t),
  });
  if (sweep.error) errors.push(`Motor ${n}: ${sweep.error}`);
  const lo = endInput(n, 'lo', kind, off);
  const hi = endInput(n, 'hi', kind, off);
  errors.push(lo.error, hi.error);
  const funcSelect = select({
    value: val(`m${n}_func`),
    options: S.st.motorFunctions.map((t, i) => [String(i), t]),
    dirty: isDirty(`m${n}_func`),
    cls: 'compact',
    h: on({ change: (_e, el) => { pickFunction(n, el.value); requestRender(); } }),
  });
  const dirSelect = select({
    value: ccw ? '1' : '0',
    options: [['0', 'CW'], ['1', 'CCW']],
    disabled: off,
    dirty: isDirty(`m${n}_min`) || isDirty(`m${n}_max`),
    cls: 'compact dir',
    h: on({ change: (_e, el) => { const [l, h] = range(n); storeRange(n, l, h, el.value === '1'); requestRender(); } }),
  });
  return html`<tr>
    <td class="dial-cell">${dial(angle, ccw)}</td>
    <td class="row-head">Motor ${n}</td>
    ${td(funcSelect, { tip: S.st.tooltips[`m${n}_func`] })}
    ${td(textInput({ value: sweep.value, h: sweep.h, dirty: sweep.dirty, cls: 'compact', label: `Motor ${n} sweep angle` }), { unit: '°', tip: sweepTip })}
    ${td(textInput({ value: String(MOTOR_STEPS_PER_DEGREE), disabled: true, cls: 'compact', label: `Motor ${n} steps per degree` }), { off: true, tip: 'Fixed for the Switec Z27.168.' })}
    ${td(lo.control, { off, tip: off ? offTip : S.st.tooltips[`m${n}_min`] })}
    ${td(hi.control, { unit, off, tip: off ? offTip : S.st.tooltips[`m${n}_max`] })}
    ${td(dirSelect, { off, tip: off ? offTip : S.st.tooltips.motor_direction })}
  </tr>`;
}

/**
 * Motor S (NEMA14) on the same row layout. Function is ms_func (0 Speed, 1 RPM); the firmware
 * fixes its low end to 0 and its direction, so those show locked. High is speedo_max for
 * speed and ms_rpm_max for RPM.
 * The config stores the sweep in steps. NEMA14 steps per degree depends on the driver
 * microstepping, so it is a tool preference; the sweep is entered in degrees and converted.
 */
const MS_STEPS = [100, 16000];
const fmt2 = (x) => String(Math.round(x * 100) / 100);
const msStepsPerDeg = () => Number(S.prefs.ms_steps_per_deg) || MS_STEPS_PER_DEGREE_DEFAULT;
const MS_STEPS_PER_DEGREE_DEFAULT = (360 / 0.9) * 64 / 360;   // 0.9° NEMA14 at 1/64 microstepping = 71.11

function msDegreesToSteps(text, spd) {
  const deg = Number(String(text).trim());
  if (String(text).trim() === '' || !Number.isFinite(deg)) throw new Error('Sweep angle must be a number.');
  const steps = Math.round(deg * spd);
  if (steps < MS_STEPS[0] || steps > MS_STEPS[1]) {
    throw new Error(`Sweep angle must be between ${fmt2(MS_STEPS[0] / spd)} and ${fmt2(MS_STEPS[1] / spd)} degrees at ${fmt2(spd)} steps per degree.`);
  }
  return String(steps);
}

/** Changing steps per degree keeps the entered angle and recomputes the stored steps. */
function setMsStepsPerDeg(text) {
  const spd = Number(String(text).trim());
  if (!(spd > 0) || !Number.isFinite(spd)) {
    S.bad.ms_spd = { text, error: 'Steps per degree must be a number above 0.' };
    requestRender();
    return;
  }
  delete S.bad.ms_spd;
  const old = msStepsPerDeg();
  let steps = null;
  try { steps = pyInt(val('ms_sweep')); } catch { /* unset */ }
  S.prefs.ms_steps_per_deg = spd;
  api.set_pref('ms_steps_per_deg', spd);
  if (steps != null && !S.bad.ms_sweep) {
    try { setVal('ms_sweep', msDegreesToSteps(fmt2(steps / old), spd)); } catch (exc) { S.bad.ms_sweep = { text: fmt2(steps / old), error: exc.message }; }
  }
  requestRender();
}

function motorSRow(errors, notes) {
  const speedUnit = imperial() ? 'mph' : 'km/h';
  const locked = (value, text) => select({ value, options: [[value, text]], disabled: true, cls: 'compact' });
  const spd = msStepsPerDeg();
  const sweep = bind('ms_sweep', {
    fmt: (raw) => { try { return fmt2(pyInt(raw) / spd); } catch { return raw; } },
    parse: (t) => msDegreesToSteps(t, spd),
  });
  let angle = MOTOR_MAX_ANGLE;
  try { angle = Math.min(MOTOR_MAX_ANGLE, pyInt(val('ms_sweep')) / spd); } catch { /* unset */ }
  const spdBad = S.bad.ms_spd;
  if (spdBad) errors.push(`Motor S: ${spdBad.error}`);
  const spdInput = textInput({
    value: spdBad ? spdBad.text : fmt2(spd),
    dirty: !!spdBad,
    cls: 'compact',
    label: 'Motor S steps per degree',
    h: on({ change: (_e, el) => setMsStepsPerDeg(el.value) }),
  });
  let stepsNow = '';
  try { stepsNow = ` Stored in the config as ${pyInt(val('ms_sweep'))} steps.`; } catch { /* unset */ }
  const isRpm = val('ms_func') === '1';
  const highParam = isRpm ? 'ms_rpm_max' : 'speedo_max';
  const highUnit = isRpm ? 'RPM' : speedUnit;
  const high = isRpm ? bind('ms_rpm_max') : bind('speedo_max', {
    fmt: (raw) => speedoMaxDisplay(raw, speedUnit),
    parse: (t) => speedoMaxToRaw(t, speedUnit),
  });
  if (sweep.error) errors.push(`Motor S: ${sweep.error}`);
  if (high.error) errors.push(`Motor S: ${high.error}`);
  const funcSelect = select({
    value: isRpm ? '1' : '0',
    options: [['0', 'Speed'], ['1', 'RPM']],
    dirty: isDirty('ms_func'),
    cls: 'compact',
    h: on({ change: (_e, el) => { setVal('ms_func', el.value); delete S.bad.speedo_max; delete S.bad.ms_rpm_max; requestRender(); } }),
  });
  const highNote = isRpm ? '' : dependencyNote('speedo_max');
  if (highNote) notes.push(`Motor S High: ${highNote}`);
  return html`<tr>
    <td class="dial-cell">${dial(angle, false)}</td>
    <td class="row-head">Motor S</td>
    ${td(funcSelect, { tip: S.st.tooltips.ms_func })}
    ${td(textInput({ value: sweep.value, h: sweep.h, dirty: sweep.dirty, cls: 'compact', label: 'Motor S sweep angle' }), { unit: '°', tip: `Full needle sweep in degrees.${stepsNow}` })}
    ${td(spdInput, { tip: 'NEMA14 steps per degree: microsteps ÷ degrees per full step (64 ÷ 0.9 = 71.11). Saved with the tool, not the gauge config.' })}
    ${td(textInput({ value: '0', disabled: true, cls: 'compact', label: 'Motor S low' }), { off: true })}
    ${td(textInput({ value: high.value, h: high.h, dirty: high.dirty, disabled: !!highNote, cls: 'compact', label: 'Motor S high' }), { unit: highUnit, off: !!highNote, tip: highNote || S.st.tooltips[highParam] })}
    ${td(locked('0', 'CW'), { off: true })}
  </tr>`;
}

function motorsPanel() {
  const errors = [];
  const notes = [];
  const rows = [1, 2, 3, 4].map((n) => motorRow(n, errors));
  rows.push(motorSRow(errors, notes));
  if ([1, 2, 3, 4].some((n) => val(`m${n}_func`) === '0')) notes.unshift('Low, High and Direction are unused while a motor\'s function is Off; the needle parks at zero.');
  notes.push('Motor S always runs from 0 to High, clockwise; the firmware fixes these.');
  return panel({
    title: 'Gauge motors',
    tag: 'Switec Z27.168 · NEMA14',
    wide: true,
    body: [
      html`<div class="table-wrap"><table class="table motors">
        <thead><tr><th></th><th></th><th>Function</th><th>Sweep</th><th>Steps/°</th><th>Low</th><th>High</th><th>Direction</th></tr></thead>
        <tbody>${rows}</tbody>
      </table></div>`,
      errorBox(errors.filter(Boolean)),
      html`<div class="panel-notes">${notes.map((t) => html`<p>${t}</p>`)}</div>`,
    ],
  });
}

export function render() {
  return html`<div class="panel-grid">
    ${motorsPanel()}
    ${panel({ title: 'Motor S - Zeroing', body: fields([
      paramField('Zero step delay', 'ms_zero_delay', { unit: 'µs' }),
      paramField('Zero sweep fraction', 'ms_zero_factor'),
    ]) })}
    ${panel({ title: 'Startup sweep', body: fields([paramField('Sweep time', 'motor_sweep_ms', { unit: 'ms' })]) })}
  </div>`;
}
