// 07 / Outputs: low side outputs on D12 and D13 (on/off with hysteresis, or a PWM curve).

import { html, on, panel, fields, select, textInput, errorBox } from '../ui.js';
import { S, val, setVal, bind, paramField, selectField, imperial, requestRender, isDirty, intOr } from '../state.js';
import { rawToDisplay, displayToRaw, unitLabel, outputError, validatePwmCurve, pyInt, pyRound } from '../units.js';

const PINS = { 1: 'D12', 2: 'D13' };
const MODE_ON_OFF = '1';
const MODE_PWM = '2';

const points = () => [...Array(S.st.outputPwmPoints).keys()];

function kindOf(n) {
  const param = intOr(`out${n}_param`);
  return param >= 1 ? S.st.outputKinds[param - 1] ?? null : null;
}

function unitOf(n) {
  const param = intOr(`out${n}_param`);
  if (!(param >= 1 && param <= S.st.outputParams.length)) return '';
  return unitLabel(S.st.outputKinds[param - 1], imperial(), S.st.outputUnits[param - 1]);
}

/** A newly PWM output adopts the other output's frequency: they share Timer1. */
function pickMode(n, value) {
  setVal(`out${n}_mode`, value);
  const other = 3 - n;
  if (value === MODE_PWM && val(`out${other}_mode`) === MODE_PWM && val(`out${other}_freq`)) {
    setVal(`out${n}_freq`, val(`out${other}_freq`));
  }
}

/** Typing a frequency on one PWM output updates the other PWM output to match. */
function syncFreq(n) {
  const other = 3 - n;
  if (val(`out${n}_mode`) === MODE_PWM && val(`out${other}_mode`) === MODE_PWM) {
    setVal(`out${other}_freq`, val(`out${n}_freq`));
  }
}

/** Typical thresholds and an evenly spaced curve for a newly picked parameter (GaugeConfigApp._seed_output_values). */
function pickParam(n, value) {
  const p = `out${n}_`;
  setVal(`${p}param`, value);
  const range = S.st.outputRanges[Number(value) - 1];
  if (!range) return;
  const [lo, hi] = range;
  const span = hi - lo;
  let op = intOr(`${p}on_op`, 0);
  if (op !== 0 && op !== 1) op = 0;
  const band = Math.max(1, pyRound(span * 0.05));
  const onValue = lo + pyRound(span * (op === 0 ? 0.8 : 0.2));
  setVal(`${p}on_op`, String(op));
  setVal(`${p}on_val`, String(onValue));
  setVal(`${p}off_val`, String(op === 0 ? onValue - band : onValue + band));
  let last = null;
  points().forEach((i) => {
    let x = lo + pyRound(span * i / (points().length - 1));
    x = last === null || x > last ? x : last + 1;
    setVal(`${p}x${i}`, String(x));
    last = x;
  });
}

/** Value entry in the selected units; text that does not convert is kept so validation names it. */
function valueCell(p, kind, label) {
  const imp = imperial();
  const h = on({
    input: (_e, el) => {
      const text = el.value.trim();
      let stored = text;
      try { stored = String(displayToRaw(text, kind, imp)); } catch { /* left as typed */ }
      setVal(p, stored);
      requestRender();
    },
    change: (_e, el) => { el.value = rawToDisplay(val(p), kind, imp); requestRender(); },
  });
  return html`<td>${textInput({ value: rawToDisplay(val(p), kind, imp), h, dirty: isDirty(p), cls: 'compact', label })}</td>`;
}

function plainCell(p, label, after = null) {
  const b = bind(p, { after });
  return html`<td>${textInput({ value: b.value, h: b.h, dirty: b.dirty, cls: 'compact', label })}</td>`;
}

function onOffTable(n, kind, unit) {
  const p = `out${n}_`;
  let offOp = '';
  try { offOp = S.st.outputOnOps[1 - pyInt(val(`${p}on_op`))] ?? ''; } catch { /* unset */ }
  const opSelect = select({
    value: val(`${p}on_op`),
    options: S.st.outputOnOps.map((op, i) => [String(i), op]),
    dirty: isDirty(`${p}on_op`),
    cls: 'compact',
    h: on({ change: (_e, el) => { setVal(`${p}on_op`, el.value); requestRender(); } }),
  });
  return html`<div class="table-wrap"><table class="table">
    <thead><tr><th></th><th>Comparison</th><th>Value</th><th></th></tr></thead>
    <tbody>
      <tr><td class="row-head">Turn ON when value</td><td>${opSelect}</td>${valueCell(`${p}on_val`, kind, 'On value')}<td class="calc left">${unit}</td></tr>
      <tr><td class="row-head">Turn OFF when value</td><td class="calc center" title="Set automatically to the opposite of the on comparison.">${offOp}</td>${valueCell(`${p}off_val`, kind, 'Off value')}<td class="calc left">${unit}</td></tr>
    </tbody>
  </table></div>`;
}

/** PWM preview: viewBox 0 0 480 156, x 44-468, y = 130 - duty * 1.2. */
function chart(n, kind, unit) {
  const p = `out${n}_`;
  const xsText = points().map((i) => val(`${p}x${i}`));
  const dsText = points().map((i) => val(`${p}d${i}`));
  const grid = [0, 50, 100].map((d) => html`<line class="grid" x1="44" x2="468" y1="${130 - d * 1.2}" y2="${130 - d * 1.2}"></line><text x="38" y="${133 - d * 1.2}" text-anchor="end">${d}%</text>`);
  try {
    validatePwmCurve('20', xsText, dsText);
  } catch {
    return html`<svg class="chart" viewBox="0 0 480 156" role="img" aria-label="PWM curve preview">${grid}<text x="256" y="72" text-anchor="middle">Enter a valid curve to see the preview</text></svg>`;
  }
  const xs = xsText.map((x) => pyInt(x));
  const ds = dsText.map((d) => pyInt(d));
  const lo = xs[0];
  const span = xs[xs.length - 1] - lo || 1;
  const pts = xs.map((x, i) => [44 + (x - lo) / span * 424, 130 - ds[i] * 1.2]);
  const imp = imperial();
  const label = (raw) => `${rawToDisplay(String(raw), kind, imp)} ${unit}`.trim();
  return html`<svg class="chart" viewBox="0 0 480 156" role="img" aria-label="PWM curve preview">
    ${grid}
    <polyline class="curve" points="${pts.map(([x, y]) => `${x.toFixed(1)},${y.toFixed(1)}`).join(' ')}"></polyline>
    ${pts.map(([x, y]) => html`<circle class="pt" cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="3.5"></circle>`)}
    <text x="44" y="150">${label(xs[0])}</text>
    <text x="468" y="150" text-anchor="end">${label(xs[xs.length - 1])}</text>
  </svg>`;
}

function outputPanel(n) {
  const p = `out${n}_`;
  const mode = val(`${p}mode`);
  const kind = kindOf(n);
  const unit = unitOf(n);
  const list = [selectField('Mode', `${p}mode`, S.st.outputModes, { note: '', onPick: (v) => pickMode(n, v) })];
  if (mode === MODE_ON_OFF || mode === MODE_PWM) {
    list.push(selectField('Parameter', `${p}param`, S.st.outputParams, { note: '', offset: 1, onPick: (v) => pickParam(n, v) }));
  }
  if (mode === MODE_PWM) {
    list.push(paramField('Frequency', `${p}freq`, { note: '', unit: `Hz · ${S.st.outputFreqRange[0]}–${S.st.outputFreqRange[1]}`, after: () => syncFreq(n) }));
  }
  const body = [fields(list)];
  if (mode === MODE_ON_OFF) body.push(onOffTable(n, kind, unit));
  if (mode === MODE_PWM) {
    body.push(html`<div class="table-wrap"><table class="table">
      <thead><tr><th></th>${points().map((i) => html`<th>${i + 1}</th>`)}</tr></thead>
      <tbody>
        <tr><td class="row-head">${unit ? `Value (${unit})` : 'Value'}</td>${points().map((i) => valueCell(`${p}x${i}`, kind, `Point ${i + 1} value`))}</tr>
        <tr><td class="row-head">Duty (%)</td>${points().map((i) => plainCell(`${p}d${i}`, `Point ${i + 1} duty`))}</tr>
      </tbody>
    </table></div>`);
    body.push(chart(n, kind, unit));
  }
  let error = outputError(n, val, imperial(), S.st);
  if (!error && val('out1_mode') === MODE_PWM && val('out2_mode') === MODE_PWM && val('out1_freq') !== val('out2_freq')) {
    error = 'Both outputs are PWM and share one timer: set the same frequency on both.';
  }
  body.push(errorBox(error));
  const modeName = S.st.outputModes[intOr(`${p}mode`, -1)] || '';
  return panel({ title: `Output ${n} (${PINS[n]})`, tag: modeName, wide: true, body });
}

export function render() {
  return html`
    ${errorBox(['Low side outputs ground the load when ON. DO NOT EXCEED 0.5A PER OUTPUT.',
      'Both outputs use one PWM timer, so when both are PWM they share a frequency.'])}
    <div class="panel-grid">${outputPanel(1)}${outputPanel(2)}</div>`;
}
