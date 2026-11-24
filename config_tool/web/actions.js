// Device and file workflows, ported from GaugeConfigApp's handlers in gui.py.

import { S, requestRender, merged, dirtyParams, setVal, imperial, tabOf } from './state.js';
import { api, openModal, closeModal, alertBox, confirmBox, failed, requireConnection } from './bridge.js';

export const TAB_TITLES = {
  connection: 'Connection', source: 'Signal Source', speed: 'Speed/RPM', sensors: 'Sensors',
  motors: 'Motor Config', tach: 'LED Tach', outputs: 'Outputs', display: 'Display Settings',
  faults: 'Fault Warnings', splash: 'Splash Screens', test: 'Bench testing', logging: 'Logging',
};

const UNIT_NAMES = { metric: 'Metric', imperial: "'Merican" };

// ---- preferences ----------------------------------------------------------------------

export function applyTheme(theme) {
  document.documentElement.dataset.theme = theme;
  try { localStorage.setItem('gauge-theme', theme); } catch { /* storage unavailable */ }
}

export function setTheme(theme) {
  if (S.prefs.theme === theme) return;
  S.prefs.theme = theme;
  applyTheme(theme);
  api.set_pref('theme', theme);
  S.status = `Theme set to ${theme === 'dark' ? 'Dark' : 'Light'}`;
  requestRender();
}

export function setUnits(units) {
  if (S.prefs.units === units) return;
  S.prefs.units = units;
  S.bad = {};  // half-typed text was in the old units; show the stored values again
  S.test.texts = {};
  api.set_pref('units', units);
  S.status = `Config Tool units set to ${UNIT_NAMES[units]}. The firmware always stores metric.`;
  requestRender();
}

// ---- Test tab log ---------------------------------------------------------------------

/** Append bridge log lines ("desc -> result") to the Test tab log as "> desc" / "< result". */
export function testLog(lines) {
  (lines || []).forEach((line) => {
    const [desc, result] = line.split(' -> ');
    S.test.log.push(`> ${desc}`);
    if (result !== undefined) S.test.log.push(`< ${result}`);
  });
  S.test.log = S.test.log.slice(-200);
}

let odoTimer = null;

export function stopOdoUi() {
  clearTimeout(odoTimer);
  odoTimer = null;
  S.test.odoMoving = false;
}

export function startOdoUi(ms) {
  stopOdoUi();
  S.test.odoMoving = true;
  odoTimer = setTimeout(() => { stopOdoUi(); requestRender(); }, ms);
}

/** Leaving the Test tab: revert the odometer button and restore the overridden sources. */
export async function leaveTest() {
  stopOdoUi();
  const res = await api.restore_sources();
  if (res.ok) testLog(res.log);
  requestRender();
}

// ---- LED tach live test -------------------------------------------------------------

let tachTimer = null;

function stopTachUi() {
  clearInterval(tachTimer);
  tachTimer = null;
  S.tach.testOn = false;
}

export async function stopTachTest() {
  if (!S.tach.testOn) return;
  stopTachUi();
  requestRender();
  failed(await api.tach_preview_stop(), 'Tach test error');
}

export async function toggleTachTest() {
  if (S.tach.testOn) {
    await stopTachTest();
    return;
  }
  if (!requireConnection()) return;
  if (failed(await api.tach_preview(merged()), 'Tach test error')) return;
  S.tach.testOn = true;
  S.status = 'Tach test running on device';
  tachTimer = setInterval(async () => {
    const res = await api.tach_preview(merged());
    if (res.ok || !S.tach.testOn) return;
    stopTachUi();
    S.status = 'Tach test stopped: check the tach settings.';
    alertBox('Tach test error', `${res.error}\nThe preview expires automatically.`);
  }, 1000);
  requestRender();
}

// ---- Logging tab ------------------------------------------------------------------------

export const LOG_WINDOW_MS = 20000;
let logTimer = null;

function stopLogUi() {
  clearInterval(logTimer);
  logTimer = null;
  S.log.live = false;
  S.log.recording = false;
}

function setLive() {
  // A stopped trace stays up for inspection until the next run starts.
  if (!S.log.live) S.log.samples = [];
  S.log.live = true;
  S.log.lastAt = Date.now();
  // Repaint while live so a stalled stream shows as "no data" even with no samples arriving.
  if (!logTimer) logTimer = setInterval(() => { if (S.tab === 'logging') requestRender(); }, 500);
}

const savedText = (res) => (res.path ? `Saved ${res.rows} rows to ${res.path}` : 'Recording stopped');

export async function startLogging() {
  if (S.log.live || !requireConnection()) return;
  const res = await api.log_start();
  if (failed(res, 'Logging error')) { requestRender(); return; }
  setLive();
  S.status = 'Logging live data';
  requestRender();
}

export async function stopLogging() {
  if (!S.log.live) return;
  const wasRecording = S.log.recording;
  stopLogUi();
  requestRender();
  const res = await api.log_stop();
  if (failed(res, 'Logging error')) return;
  S.status = wasRecording ? savedText(res) : 'Logging stopped';
  requestRender();
}

export async function startRecording() {
  if (S.log.recording || (!S.log.live && !requireConnection())) return;
  const res = await api.log_record_start(imperial());
  if (failed(res, 'Recording error') || res.cancelled) { requestRender(); return; }
  setLive();
  S.log.recording = true;
  S.log.name = res.name;
  S.log.rows = 0;
  S.status = `Recording to ${res.path}`;
  requestRender();
}

export async function stopRecording() {
  if (!S.log.recording) return;
  S.log.recording = false;
  requestRender();
  const res = await api.log_record_stop();
  if (!failed(res, 'Recording error')) S.status = savedText(res);
  requestRender();
}

/** One sample pushed from Python: {t: gauge millis, v: metric values, rows: CSV rows so far}. */
export function onLogSample(evt) {
  if (!S.log.live) return;  // a line that was in flight when logging stopped
  const samples = S.log.samples;
  // The gauge restarted (millis went back): start the traces over.
  if (samples.length && evt.t < samples[samples.length - 1].t) samples.length = 0;
  samples.push({ t: evt.t, v: evt.v });
  const cutoff = evt.t - LOG_WINDOW_MS - 500;
  while (samples.length && samples[0].t < cutoff) samples.shift();
  S.log.lastAt = Date.now();
  if (S.log.recording) S.log.rows = evt.rows;
  if (S.tab === 'logging') requestRender();
}

/** The reader stopped on its own (port lost or the CSV could not be written). */
export function onLogError(evt) {
  stopLogUi();
  S.status = evt.text;
  const saved = evt.path ? `\n\n${evt.rows} rows were saved to ${evt.path}.` : '';
  alertBox('Logging stopped', `${evt.text}${saved}`);
  api.log_stop();  // stop the gauge's stream if it is still reachable
  requestRender();
}

// ---- connection -----------------------------------------------------------------------

export async function refreshPorts() {
  const details = await api.list_ports();
  S.ports = Array.isArray(details) ? details : [];
  if (S.ports.length && !S.ports.some(([device]) => device === S.conn.port)) S.conn.port = S.ports[0][0];
  else if (!S.ports.length) S.conn.port = '';
  requestRender();
}

function markConnected(version) {
  S.conn.connected = true;
  S.conn.version = version;
  S.conn.state = 'Connected';
}

export function markDisconnected(state = 'Disconnected') {
  stopTachUi();
  stopOdoUi();
  stopLogUi();
  S.conn.connected = false;
  S.conn.version = '';
  S.conn.state = state;
}

/** Connect behind a cancellable "Connecting" dialog (GaugeConfigApp._connect_with_progress). */
export async function connectWithProgress(port, baud) {
  openModal({
    title: `Connecting to ${port}`,
    message: 'Starting...',
    busy: true,
    buttons: [{ label: 'Cancel', value: 'cancel', kind: 'secondary', onClick: () => {
      api.connect_cancel();
      if (S.modal) { S.modal.message = 'Cancelling...'; S.modal.cancelling = true; requestRender(); }
    } }],
  });
  const res = await api.connect(port, baud);
  closeModal();
  return res;
}

export async function toggleConnection() {
  if (S.conn.connected) {
    const res = await api.disconnect();
    markDisconnected();
    S.status = 'Disconnected';
    if (!failed(res)) testLog(res.log);
    requestRender();
    return;
  }
  const port = S.conn.port;
  const baud = S.conn.baud;
  if (!port) {
    alertBox('No port', "Select the gauge's port first.");
    return;
  }
  const res = await connectWithProgress(port, baud);
  if (failed(res, 'Connection error')) return;
  if (res.cancelled) {
    S.status = 'Connection cancelled';
  } else if (res.noPing) {
    alertBox('Warning', res.warning);
  } else {
    markConnected(res.version);
    S.status = `Connected: ${port} @ ${baud}  |  ${res.version}`;
  }
  requestRender();
}

// ---- configuration ----------------------------------------------------------------------

function applyDeviceValues(values) {
  S.raw = { ...S.st.tachColors, ...values };
  S.edits = {};
  S.bad = {};
  S.screens = {};
  S.dirHint = {};
}

export async function readDevice() {
  if (!requireConnection()) return;
  stopTachUi();
  const res = await api.read_device();
  if (failed(res, 'Read error')) { requestRender(); return; }
  testLog(res.log);
  applyDeviceValues(res.values);
  S.status = `Read ${res.count} parameters from device.`;
  if (res.warning) alertBox('Firmware update required', res.warning);
  requestRender();
}

export async function resetDefaults() {
  if (!requireConnection()) return;
  if (!(await confirmBox('Confirm', 'Reset all parameters to firmware defaults?', 'Reset'))) return;
  stopTachUi();
  const res = await api.reset_defaults();
  if (failed(res)) { requestRender(); return; }
  testLog(res.log);
  applyDeviceValues(res.values);
  S.status = 'Parameters reset to firmware defaults.';
  requestRender();
}

/** Typed text that never converted blocks Write and Save, like gui.py's validators. */
function rejectBadText() {
  const entries = Object.entries(S.bad);
  if (!entries.length) return false;
  const [param, bad] = entries[0];
  const more = entries.length > 1 ? `\n\n${entries.length - 1} other field(s) also need fixing.` : '';
  S.tab = tabOf(param);
  alertBox('Invalid value', `${TAB_TITLES[S.tab]}: ${bad.error}${more}`);
  requestRender();
  return true;
}

/** Fold computed curve values back in: written ones become device values, the rest stay edits. */
function applyUpdates(updates, written = []) {
  Object.entries(updates || {}).forEach(([p, v]) => {
    if (!written.includes(p)) setVal(p, v);
  });
}

export async function writeDevice() {
  if (!requireConnection()) return;
  if (rejectBadText()) return;
  stopTachUi();
  const values = merged();
  const res = await api.write_device(values, dirtyParams(), imperial());
  if (failed(res, 'Write error')) { requestRender(); return; }
  testLog(res.log);
  if (res.nothing) {
    applyUpdates(res.updates);
    alertBox('Nothing to write', 'No parameters have been edited.');
    requestRender();
    return;
  }
  const finalValues = { ...values, ...res.updates };
  res.written.forEach((p) => {
    S.raw[p] = finalValues[p];
    delete S.edits[p];
  });
  applyUpdates(res.updates, res.written);
  S.status = `Wrote ${res.written.length} parameter(s) to device and saved.`;
  requestRender();
}

export async function saveFile() {
  if (rejectBadText()) return;
  const res = await api.save_file(merged(), dirtyParams(), S.prefs.units);
  if (failed(res, 'Save error') || res.cancelled) { requestRender(); return; }
  applyUpdates(res.updates);
  S.status = `Configuration saved to ${res.path}`;
  requestRender();
}

export async function loadFile() {
  const res = await api.load_file();
  if (failed(res, 'Load error') || res.cancelled) { requestRender(); return; }
  Object.entries(res.values).forEach(([p, v]) => setVal(p, v));
  S.screens = {};
  S.dirHint = {};
  if (res.system && res.system !== S.prefs.units) {
    S.prefs.units = res.system;
    api.set_pref('units', res.system);
  }
  S.status = `Loaded ${res.count} parameters from ${res.name}`;
  requestRender();
}

// ---- tab changes --------------------------------------------------------------------

export function goTab(tab) {
  if (tab === S.tab) return;
  const leaving = S.tab;
  S.tab = tab;
  if (leaving === 'tach') stopTachTest();
  if (leaving === 'test') leaveTest();
  if (leaving === 'logging') stopLogging();
  requestRender();
  const scroller = document.getElementById('page-scroll');
  if (scroller) scroller.scrollTop = 0;
}
