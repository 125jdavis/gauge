// 12 / Tests: live data logging with rolling traces and CSV recording.

import { html, on, button, actions, select, unsafe } from '../ui.js';
import { S, imperial, requestRender, sourceLabel } from '../state.js';
import { KMH_PER_MPH, PSI_PER_KPA } from '../units.js';
import { LOG_WINDOW_MS, startLogging, stopLogging, startRecording, stopRecording } from '../actions.js';

// Source parameter shown under each trace (battery voltage has no selectable source).
const SOURCE_PARAM = {
  speed: 'speed_source', rpm: 'rpm_source', oil: 'oil_prs_source', fuel: 'fuel_prs_source',
  lambda: 'lambda_source', coolant: 'coolant_src', map: 'map_source',
};
const GAP_MS = 400;     // break the line where samples are missing
const STALE_MS = 1500;  // "no data" after this long without a sample

// Charts are drawn at the measured pixel size of their plot box (S.log.size), so they
// stretch to share the window height without distorting text.
const PAD = { l: 46, r: 12, t: 8, b: 18 };
const CORNERS = unsafe('<i class="corner tl"></i><i class="corner tr"></i><i class="corner bl"></i><i class="corner br"></i>');

// ---- signals and units ------------------------------------------------------------------

const specs = () => S.st.logSignals;
const specOf = (key) => specs().find(([k]) => k === key);
const indexOf = (key) => specs().findIndex(([k]) => k === key);

function convert(kind, v) {
  if (!imperial() || !kind) return v;
  if (kind === 'speed') return v / KMH_PER_MPH;
  if (kind === 'pressure') return v * PSI_PER_KPA;
  if (kind === 'temp') return v * 1.8 + 32;
  return v;
}

/** Same conversion for a difference (no temperature offset). */
const convertSpan = (kind, span) => convert(kind, span) - convert(kind, 0);

function unitOf(spec) { return imperial() ? spec[3] : spec[2]; }
function decimalsOf(spec) { return spec[6][imperial() ? 1 : 0]; }
const fmt = (v, decimals) => v.toLocaleString('en-US', { minimumFractionDigits: decimals, maximumFractionDigits: decimals });

// ---- scales ----------------------------------------------------------------------------

function niceStep(span, count) {
  const raw = span / count;
  const mag = 10 ** Math.floor(Math.log10(raw));
  const norm = raw / mag;
  return (norm <= 1 ? 1 : norm <= 2 ? 2 : norm <= 2.5 ? 2.5 : norm <= 5 ? 5 : 10) * mag;
}

const stepDecimals = (step) => (String(+step.toPrecision(3)).split('.')[1] || '').length;

/** Y domain from the visible values: at least the signal's minimum span, snapped to nice ticks. */
function yScale(values, minSpan, count) {
  let lo = Math.min(...values);
  let hi = Math.max(...values);
  if (!values.length) { lo = 0; hi = minSpan; }
  if (hi - lo < minSpan) {
    const mid = (lo + hi) / 2;
    lo = mid - minSpan / 2;
    hi = mid + minSpan / 2;
  }
  const step = niceStep(hi - lo, count);
  lo = Math.floor(lo / step + 1e-9) * step;
  hi = Math.ceil(hi / step - 1e-9) * step;
  const ticks = [];
  for (let v = lo; v <= hi + step / 2; v += step) ticks.push(v);
  return { lo, hi, ticks, decimals: stepDecimals(step) };
}

// ---- trace window ----------------------------------------------------------------------

function pickSignal(slot, key) {
  S.log.traces[slot] = key;
  requestRender();
}

function hoverHandlers(slot) {
  return on({
    pointermove: (e, el) => {
      const r = el.getBoundingClientRect();
      const frac = (e.clientX - r.left - PAD.l) / (r.width - PAD.l - PAD.r);
      const next = frac >= 0 && frac <= 1 ? frac : null;
      if (S.log.hover[slot] !== next) { S.log.hover[slot] = next; requestRender(); }
    },
    pointerleave: (e, el) => {
      if (e.target !== el) return;  // leaving a child of the chart, not the chart
      S.log.hover[slot] = null;
      requestRender();
    },
  });
}

function chart(slot, spec) {
  const [key, name, , , kind, minSpan] = spec;
  const { w: W, h: H } = S.log.size;
  const PW = W - PAD.l - PAD.r;
  const PH = H - PAD.t - PAD.b;
  const i = indexOf(key);
  const samples = S.log.samples;
  const now = samples.length ? samples[samples.length - 1].t : 0;
  const visible = samples.filter((s) => s.t >= now - LOG_WINDOW_MS);
  const values = visible.map((s) => convert(kind, s.v[i]));
  // Fewer gridlines on short charts so the labels never crowd.
  const y = yScale(values, convertSpan(kind, minSpan), Math.max(2, Math.min(4, Math.floor(PH / 28))));
  const px = (t) => PAD.l + PW - ((now - t) / LOG_WINDOW_MS) * PW;
  const py = (v) => PAD.t + PH - ((v - y.lo) / (y.hi - y.lo)) * PH;

  let d = '';
  visible.forEach((s, n) => {
    const gap = n === 0 || s.t - visible[n - 1].t > GAP_MS;
    d += `${gap ? 'M' : 'L'}${px(s.t).toFixed(1)} ${py(values[n]).toFixed(1)}`;
  });

  const grid = y.ticks.map((v) => html`
    <line class="grid" x1="${PAD.l}" x2="${W - PAD.r}" y1="${py(v).toFixed(1)}" y2="${py(v).toFixed(1)}"></line>
    <text x="${PAD.l - 6}" y="${(py(v) + 3.5).toFixed(1)}" text-anchor="end">${fmt(v, y.decimals)}</text>`);
  const xTicks = [20, 15, 10, 5, 0].map((sec) => {
    const x = PAD.l + PW - (sec * 1000 / LOG_WINDOW_MS) * PW;
    return html`<text x="${x.toFixed(1)}" y="${H - 6}" text-anchor="${sec === 0 ? 'end' : sec === 20 ? 'start' : 'middle'}">${sec === 0 ? 'now' : `−${sec} s`}</text>`;
  });

  // Crosshair: the nearest sample to the pointer's time, with its value and age.
  let hover = '';
  const frac = S.log.hover[slot];
  if (frac !== null && visible.length) {
    const t = now - (1 - frac) * LOG_WINDOW_MS;
    let best = 0;
    visible.forEach((s, n) => { if (Math.abs(s.t - t) < Math.abs(visible[best].t - t)) best = n; });
    if (Math.abs(visible[best].t - t) <= GAP_MS) {
      const s = visible[best];
      const hx = px(s.t);
      const hy = py(values[best]);
      const label = `${fmt(values[best], decimalsOf(spec))} ${unitOf(spec)} · −${((now - s.t) / 1000).toFixed(1)} s`;
      const right = hx > PAD.l + PW * 0.6;
      hover = html`<line class="crosshair" x1="${hx.toFixed(1)}" x2="${hx.toFixed(1)}" y1="${PAD.t}" y2="${PAD.t + PH}"></line>
        <circle class="hover-pt" cx="${hx.toFixed(1)}" cy="${hy.toFixed(1)}" r="4"></circle>
        <text class="hover-label" x="${(hx + (right ? -8 : 8)).toFixed(1)}" y="${PAD.t + 12}" text-anchor="${right ? 'end' : 'start'}">${label}</text>`;
    }
  }

  return html`<svg class="trace" viewBox="0 0 ${W} ${H}" preserveAspectRatio="none" role="img" aria-label="${name}, last 20 seconds"${hoverHandlers(slot)}>
    ${grid}${xTicks}
    <path class="line" d="${d}"></path>
    ${hover}
  </svg>`;
}

function traceWindow(slot) {
  const spec = specOf(S.log.traces[slot]) || specs()[slot];
  const [key, , , , kind] = spec;
  const last = S.log.samples[S.log.samples.length - 1];
  const reading = last ? fmt(convert(kind, last.v[indexOf(key)]), decimalsOf(spec)) : '--';
  const pick = select({
    value: key,
    options: specs().map(([k, name]) => [k, name]),
    cls: 'compact',
    h: on({ change: (_e, el) => pickSignal(slot, el.value) }),
  });
  const source = SOURCE_PARAM[key] ? `source: ${sourceLabel(SOURCE_PARAM[key])}` : 'measured by the gauge';
  return html`<section class="panel blueprint trace-panel">${CORNERS}
    <div class="trace-side">
      ${pick}
      <span class="trace-source">${source}</span>
      <div class="trace-value"><span class="big-readout">${reading}</span><span class="unit">${unitOf(spec)}</span></div>
    </div>
    <div class="trace-plot">${chart(slot, spec)}</div>
  </section>`;
}

/** After each render: draw at the plot box's real size (re-renders once when it changes). */
export function afterRender() {
  const el = document.querySelector('.trace-plot');
  if (!el) return;
  const w = Math.round(el.clientWidth);
  const h = Math.round(el.clientHeight);
  if (w > PAD.l + PAD.r && h > PAD.t + PAD.b && (w !== S.log.size.w || h !== S.log.size.h)) {
    S.log.size = { w, h };
    requestRender();
  }
}

window.addEventListener('resize', () => { if (S.tab === 'logging') requestRender(); });

// ---- page ------------------------------------------------------------------------------

function statusLine() {
  const { live, recording, lastAt, samples, rows, name } = S.log;
  if (!S.conn.connected) return 'Connect to the gauge to log data.';
  if (!live) return 'Stopped. Start live view to see the traces, or record straight to a CSV file.';
  const quiet = Date.now() - lastAt;
  const parts = [quiet > STALE_MS ? `No data from the gauge for ${Math.floor(quiet / 1000)} s` : 'Live, 10 samples per second'];
  if (!samples.length && quiet <= STALE_MS) parts[0] = 'Waiting for data';
  if (recording) parts.push(`recording ${rows} rows to ${name}`);
  return parts.join(' · ');
}

export function render() {
  const { live, recording } = S.log;
  const offline = !S.conn.connected;
  const stale = live && Date.now() - S.log.lastAt > STALE_MS;
  const controls = html`<section class="panel blueprint log-bar">${CORNERS}
    ${actions([
      live
        ? button('Stop logging', { h: on({ click: () => stopLogging() }) })
        : button('Start live view', { h: on({ click: () => startLogging() }), disabled: offline }),
      recording
        ? button('Stop recording', { kind: 'primary', h: on({ click: () => stopRecording() }) })
        : button('Record to CSV…', { kind: 'primary', iconName: 'save', h: on({ click: () => startRecording() }), disabled: offline }),
    ])}
    <p class="log-status${stale ? ' is-stale' : ''}" aria-live="polite">${statusLine()}</p>
    <span class="panel-tag">${recording ? 'Recording' : live ? 'Live' : 'Stopped'}</span>
  </section>`;
  return html`<div class="log-layout">${controls}${[0, 1, 2, 3].map(traceWindow)}</div>`;
}
