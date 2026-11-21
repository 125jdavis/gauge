// 05 / Outputs: function, sweep and dial range for each gauge motor, including Motor S.

import { html, on, panel, fields, field, select, textInput } from '../ui.js';
import { S, val, setVal, paramField, selectField, imperial, requestRender, isDirty, intOr } from '../state.js';
import {
  angleToSteps, stepsToAngle, gaugeRangeToDevice, gaugeRangeFromDevice,
  rawToDisplay, displayToRaw, unitLabel, pyInt, MOTOR_STEPS_PER_DEGREE, MOTOR_MAX_ANGLE,
  speedoMaxToRaw, speedoMaxDisplay,
} from '../units.js';

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

/** Low or High entry; edits re-store min/max with the direction applied. */
function endField(n, which, kind, unit, offNote) {
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
  return field({
    label: which === 'lo' ? 'Low' : 'High', unit, note: offNote, disabled: !!offNote, dirty,
    error: badEntry ? badEntry.error : '',
    tip: S.st.tooltips[which === 'lo' ? `m${n}_min` : `m${n}_max`],
    control: textInput({ value: badEntry ? badEntry.text : rawToDisplay(current, kind, imp), h, dirty, disabled: !!offNote, label: `Motor ${n} ${which === 'lo' ? 'low' : 'high'}` }),
  });
}

function motorPanel(n) {
  const fn = intOr(`m${n}_func`, 0);
  const kind = kindOf(fn);
  const unit = unitLabel(kind, imperial(), S.st.motorUnits[fn] || '');
  const offNote = val(`m${n}_func`) === '0' ? 'Function is Off: the needle parks at zero.' : '';
  const [, , ccw] = range(n);
  let angle = 0;
  try { angle = pyInt(val(`m${n}_sweep`)) / MOTOR_STEPS_PER_DEGREE; } catch { /* unset */ }
  const sweepTip = `Full needle sweep in degrees (Switec Z27.168: ${MOTOR_STEPS_PER_DEGREE} steps per degree). Stored in the config as steps.`;

  const dirSelect = select({
    value: ccw ? '1' : '0',
    options: [['0', 'CW'], ['1', 'CCW']],
    disabled: !!offNote,
    dirty: isDirty(`m${n}_min`) || isDirty(`m${n}_max`),
    h: on({ change: (_e, el) => { const [lo, hi] = range(n); storeRange(n, lo, hi, el.value === '1'); requestRender(); } }),
  });

  return panel({
    title: `Motor ${n}`,
    tag: S.st.motorFunctions[fn] || '',
    body: html`<div class="panel-row">${dial(angle, ccw)}${fields([
      selectField('Function', `m${n}_func`, S.st.motorFunctions, { note: '', onPick: (v) => pickFunction(n, v) }),
      paramField('Sweep angle', `m${n}_sweep`, {
        unit: '°', note: '', tip: sweepTip,
        fmt: (raw) => { try { return stepsToAngle(pyInt(raw)); } catch { return raw; } },
        parse: (t) => angleToSteps(t),
      }),
      endField(n, 'lo', kind, unit, offNote),
      endField(n, 'hi', kind, unit, offNote),
      field({ label: 'Direction', control: dirSelect, note: offNote, disabled: !!offNote, tip: S.st.tooltips.motor_direction }),
    ])}</div>`,
  });
}

/**
 * Motor S (NEMA14 speedometer) laid out like motors 1-4. The firmware fixes its function
 * to speed, its low end to 0 and its direction, so those show locked; High is speedo_max.
 * Sweep stays in steps because the NEMA14 steps per degree depends on the driver microstepping.
 */
function motorSPanel() {
  const speedUnit = imperial() ? 'mph' : 'km/h';
  const locked = (label, value, text, note = '') => field({
    label, note, disabled: true,
    control: select({ value, options: [[value, text]], disabled: true }),
  });
  return panel({
    title: 'Motor S',
    tag: 'Speed',
    body: html`<div class="panel-row">${dial(MOTOR_MAX_ANGLE, false)}${fields([
      locked('Function', 'speed', 'Speed', 'Motor S always shows speed from 0 to High, clockwise; the firmware fixes these.'),
      paramField('Sweep', 'ms_sweep', { unit: 'steps', note: '' }),
      field({
        label: 'Low', unit: speedUnit, disabled: true,
        control: textInput({ value: '0', disabled: true, label: 'Motor S low' }),
      }),
      paramField('High', 'speedo_max', {
        unit: speedUnit,
        fmt: (raw) => speedoMaxDisplay(raw, speedUnit),
        parse: (t) => speedoMaxToRaw(t, speedUnit),
      }),
      locked('Direction', '0', 'CW'),
    ])}</div>`,
  });
}

export function render() {
  return html`<div class="panel-grid">
    ${[1, 2, 3, 4].map(motorPanel)}
    ${motorSPanel()}
    ${panel({ title: 'Motor S - Zeroing', body: fields([
      paramField('Zero step delay', 'ms_zero_delay', { unit: 'µs' }),
      paramField('Zero sweep fraction', 'ms_zero_factor'),
    ]) })}
    ${panel({ title: 'Startup sweep', body: fields([paramField('Sweep time', 'motor_sweep_ms', { unit: 'ms' })]) })}
  </div>`;
}
