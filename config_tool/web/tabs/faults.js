// 09 / Outputs: thresholds and delays for the warning flashes.

import { html, panel, fields, textInput, errorBox } from '../ui.js';
import { S, bind, paramField, imperial } from '../state.js';
import {
  warningDisplayValue, oilWarningToRaw, temperatureWarningToRaw, floatText,
  debounceMsToSeconds, debounceSecondsToMs,
} from '../units.js';

function cell(p, opts, label, unit = '') {
  const b = bind(p, opts);
  return html`<td title="${S.st.tooltips[p] || ''}"><div class="cell">${textInput({ value: b.value, h: b.h, dirty: b.dirty, cls: 'compact', label })}${unit ? html`<span class="unit">${unit}</span>` : ''}</div></td>`;
}

export function render() {
  const imp = imperial();
  const pressure = imp ? 'PSI' : 'kPa';
  const temp = imp ? 'F' : 'C';
  const delay = { fmt: debounceMsToSeconds, parse: (t) => debounceSecondsToMs(t) };
  const rows = [
    ['Oil pressure', 'below', 'oil_warn_kpa', pressure, 'oil_warn_ms', {
      fmt: (raw) => warningDisplayValue(raw, 'oil_warn_kpa', pressure),
      parse: (t) => floatText(oilWarningToRaw(t, pressure)),
    }],
    ['Engine temp', 'above', 'coolant_warn_c', `°${temp}`, 'coolant_warn_ms', {
      fmt: (raw) => warningDisplayValue(raw, 'coolant_warn_c', temp),
      parse: (t) => floatText(temperatureWarningToRaw(t, temp)),
    }],
    ['Battery voltage', 'below', 'batt_warn_v', 'V', 'batt_warn_ms', {}],
    ['Low fuel', 'below', 'fuel_warn_pct', '%', 'fuel_warn_ms', {}],
  ];
  const params = rows.flatMap(([, , p, , d]) => [p, d]);
  return html`<div class="panel-grid">${panel({
    title: 'Fault Warning Thresholds',
    wide: true,
    body: [
      html`<div class="table-wrap"><table class="table fit">
        <thead><tr><th></th><th>Flash when</th><th>Threshold</th><th>Delay</th></tr></thead>
        <tbody>${rows.map(([label, when, p, unit, d, opts]) => html`<tr>
          <td class="row-head">${label}</td>
          <td class="calc left">${when}</td>
          ${cell(p, opts, `${label} threshold`, unit)}
          ${cell(d, delay, `${label} delay`, 's')}
        </tr>`)}</tbody>
      </table></div>`,
      fields([paramField('Engine running RPM', 'engine_run_rpm', { note: '' })]),
      errorBox(params.filter((p) => S.bad[p]).map((p) => S.bad[p].error)),
    ],
  })}</div>`;
}
