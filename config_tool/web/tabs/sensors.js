// 04 / Inputs: thermistor and fuel sender curves, analog input types and filtering.

import { html, on, panel, fields, select, textInput, errorBox } from '../ui.js';
import { S, val, setVal, bind, paramField, selectField, filterField, imperial, requestRender, isDirty, intOr } from '../state.js';
import {
  rawToDisplay, displayToRaw, unitLabel, previewMillivolts, pyRound, pyInt,
  voltsToAdc, adcToVolts, physicalToRaw, rawToPhysical, avPhysicalUnit, trimNumber,
} from '../units.js';

const range6 = [0, 1, 2, 3, 4, 5];

/** A compact table input bound to *p*. */
function cell(p, opts = {}) {
  const b = bind(p, opts);
  const disabled = !!opts.disabled;
  return html`<td>${textInput({ value: b.value, h: b.h, dirty: b.dirty, disabled, cls: 'compact', label: opts.label || p })}</td>`;
}

const calc = (text, align = '') => html`<td class="calc${align ? ` ${align}` : ''}">${text}</td>`;

function panelErrors(params) {
  return errorBox(params.filter((p) => S.bad[p]).map((p) => S.bad[p].error));
}

function switchTo(typeParam, custom) {
  return () => { if (val(typeParam) !== String(custom)) setVal(typeParam, String(custom)); };
}

// ---- thermistor --------------------------------------------------------------------------

function applyThermPreset(value) {
  setVal('therm_type', value);
  const preset = S.st.thermPresets[value];
  if (!preset) return;
  preset.forEach(([temp, res], i) => {
    setVal(`therm_t${i}`, String(temp));
    setVal(`therm_r${i}`, String(res));
  });
}

function thermistor() {
  const imp = imperial();
  const custom = switchTo('therm_type', S.st.thermTypeCustom);
  const tempOpts = (i) => ({
    fmt: (raw) => rawToDisplay(raw, 'temp', imp),
    parse: (t) => displayToRaw(t, 'temp', imp),
    after: custom,
    label: `Point ${i + 1} temperature`,
  });
  const params = range6.flatMap((i) => [`therm_t${i}`, `therm_r${i}`]);
  return panel({
    title: 'Thermistor (coolant / oil temperature)',
    wide: true,
    body: [
      fields([
        selectField('Curve', 'therm_type', S.st.thermTypes, { note: '', onPick: applyThermPreset }),
        paramField('Bias resistor', 'therm_bias', { unit: 'Ω' }),
        filterField('Filter', 'filter_therm'),
      ]),
      html`<div class="table-wrap"><table class="table">
        <thead><tr><th></th>${range6.map((i) => html`<th>P${i + 1}</th>`)}</tr></thead>
        <tbody>
          <tr><td class="row-head">Temperature (${unitLabel('temp', imp)})</td>${range6.map((i) => cell(`therm_t${i}`, tempOpts(i)))}</tr>
          <tr><td class="row-head">Resistance (Ω)</td>${range6.map((i) => cell(`therm_r${i}`, { after: custom, label: `Point ${i + 1} resistance` }))}</tr>
          <tr><td class="row-head">Voltage (mV)</td>${range6.map((i) => calc(previewMillivolts(val(`therm_r${i}`), val('therm_bias'))))}</tr>
        </tbody>
      </table></div>`,
      panelErrors(params),
    ],
  });
}

// ---- fuel sender -------------------------------------------------------------------------

function applyFuelPreset(value) {
  setVal('fuel_type', value);
  const preset = S.st.fuelPresets[value];
  if (!preset) return;
  const [empty, full] = preset;
  setVal('fuel_r_empty', String(empty));
  setVal('fuel_r_mid', String(pyRound((empty + full) / 2)));
  setVal('fuel_r_full', String(full));
  if (val('fuel_mid_pct') === '') setVal('fuel_mid_pct', String(S.st.fuelDefaultMidPct));
}

function fuelSender() {
  const custom = switchTo('fuel_type', S.st.fuelTypeCustom);
  const res = ['fuel_r_empty', 'fuel_r_mid', 'fuel_r_full'];
  return panel({
    title: 'Fuel Level Sender',
    body: [
      fields([
        selectField('Sender', 'fuel_type', S.st.fuelTypes, { note: '', onPick: applyFuelPreset }),
        paramField('Bias resistor', 'fuel_bias', { unit: 'Ω' }),
        filterField('Filter', 'filter_fuel'),
      ]),
      html`<div class="table-wrap"><table class="table">
        <thead><tr><th></th><th>Empty</th><th>Mid</th><th>Full</th></tr></thead>
        <tbody>
          <tr><td class="row-head">Level (%)</td>${calc('0')}${cell('fuel_mid_pct', { label: 'Mid level' })}${calc('100')}</tr>
          <tr><td class="row-head">Resistance (Ω)</td>${res.map((p) => cell(p, { after: custom }))}</tr>
          <tr><td class="row-head">Voltage (mV)</td>${res.map((p) => calc(previewMillivolts(val(p), val('fuel_bias'))))}</tr>
        </tbody>
      </table></div>`,
    ],
  });
}

// ---- analog inputs -------------------------------------------------------------------------

function applyAvPreset(n, value) {
  setVal(`av${n}_type`, value);
  const preset = S.st.avPresets[value];
  if (!preset) return;
  ['adc_lo', 'adc_hi', 'phys_lo', 'phys_hi'].forEach((f, i) => setVal(`av${n}_${f}`, String(preset[i])));
}

function avRow(n) {
  const p = `av${n}_`;
  const type = intOr(`${p}type`);
  const unit = avPhysicalUnit(type, imperial());
  const disabled = type === S.st.avTypeNone;
  const toCustom = () => { if (type !== S.st.avTypeLambda) setVal(`${p}type`, String(S.st.avTypeCustom)); };
  const volts = (field, other) => ({
    disabled,
    label: `AV${n} ${field === 'adc_lo' ? 'voltage low' : 'voltage high'}`,
    fmt: (raw) => { try { return trimNumber(adcToVolts(pyInt(raw)), 2); } catch { return ''; } },
    parse: (t) => {
      const adc = voltsToAdc(t);
      if (adc === intOr(`${p}${other}`)) throw new Error('Voltage low and high must be different.');
      return adc;
    },
    after: toCustom,
  });
  const phys = (field) => ({
    disabled,
    label: `AV${n} ${field === 'phys_lo' ? 'physical low' : 'physical high'}`,
    fmt: (raw) => { try { return trimNumber(rawToPhysical(pyInt(raw), unit)); } catch { return ''; } },
    parse: (t) => physicalToRaw(t, unit),
    after: toCustom,
  });
  const typeSelect = select({
    value: val(`${p}type`),
    options: S.st.avTypes.map((label, i) => [String(i), label]),
    dirty: isDirty(`${p}type`),
    cls: 'compact',
    h: on({ change: (_e, el) => { applyAvPreset(n, el.value); requestRender(); } }),
  });
  return html`<tr>
    <td class="row-head">AV${n}</td>
    <td>${typeSelect}</td>
    ${cell(`${p}adc_lo`, volts('adc_lo', 'adc_hi'))}${cell(`${p}adc_hi`, volts('adc_hi', 'adc_lo'))}
    ${cell(`${p}phys_lo`, phys('phys_lo'))}${cell(`${p}phys_hi`, phys('phys_hi'))}
    ${calc(unit, 'left')}
  </tr>`;
}

function analogCalibration() {
  const params = [1, 2, 3].flatMap((n) => ['adc_lo', 'adc_hi', 'phys_lo', 'phys_hi'].map((f) => `av${n}_${f}`));
  return panel({
    title: 'Analog Sensor Types and Calibration',
    tag: '0.5–4.5 V presets',
    wide: true,
    body: [
      html`<div class="table-wrap"><table class="table">
        <thead><tr><th></th><th>Type</th><th>Voltage low (V)</th><th>Voltage high (V)</th><th>Physical low</th><th>Physical high</th><th></th></tr></thead>
        <tbody>${[1, 2, 3].map(avRow)}</tbody>
      </table></div>`,
      panelErrors(params),
    ],
  });
}

export function render() {
  return html`<div class="panel-grid">
    ${thermistor()}
    ${fuelSender()}
    ${panel({
      title: 'Analog Sensor Filters',
      body: fields([
        filterField('AV1', 'filter_av1'),
        filterField('AV2', 'filter_av2'),
        filterField('AV3', 'filter_av3'),
        filterField('Battery filter', 'filter_vbatt'),
        paramField('Battery scaler', 'vbatt_scaler'),
      ]),
    })}
    ${analogCalibration()}
  </div>`;
}
