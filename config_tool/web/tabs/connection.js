// 01 / Device: serial connection and firmware update (spec §13).

import { html, on, panel, fields, field, select, textInput, button, actions, progress, logView } from '../ui.js';
import { S, requestRender } from '../state.js';
import { api, failed, confirmBox, alertBox } from '../bridge.js';
import { refreshPorts, toggleConnection, resetDefaults, markDisconnected, readDevice } from '../actions.js';

const BAUDS = ['9600', '57600', '115200'];

function fwLog(lines) {
  S.fw.log.push(...lines);
  S.fw.log = S.fw.log.slice(-300);
}

export async function initFirmware() {
  const res = await api.firmware_autoselect();
  if (!res.ok) return;
  S.fw.available = res.available;
  S.fw.path = res.path;
  fwLog(res.log);
}

async function browse() {
  const res = await api.firmware_browse(S.fw.path);
  if (failed(res) || res.cancelled) return;
  S.fw.path = res.path;
  fwLog(res.log);
  if (res.usable) S.status = `Firmware selected: ${res.path.split(/[\\/]/).pop()}`;
  requestRender();
}

async function update() {
  if (!S.fw.available) {
    alertBox('Unavailable', 'pyserial is required. Run: pip install pyserial');
    return;
  }
  const check = await api.firmware_check(S.fw.path);
  if (failed(check, 'Cannot use this firmware file')) return;
  const port = S.conn.port;
  if (!port) {
    alertBox('No port', "Select the gauge's port first, then press Update Firmware.");
    return;
  }
  const lines = [`Update the firmware on ${port}?`, '', `File: ${S.fw.path.split(/[\\/]/).pop()}`, '',
    'The gauge will be unresponsive for about a minute.', 'Do not unplug it until this finishes.'];
  if (check.warnings.length) lines.push('', ...check.warnings.map((w) => `Note: ${w}`));
  if (!(await confirmBox('Confirm firmware update', lines.join('\n'), 'Update Firmware'))) return;

  S.fw.busy = true;
  S.fw.pct = 0;
  S.status = 'Updating firmware — do not unplug the gauge.';
  requestRender();
  const res = await api.firmware_update(S.fw.path, port, S.conn.baud);
  S.fw.busy = false;
  if (failed(res, 'Firmware update failed')) { requestRender(); return; }
  if (res.updated) {
    S.status = 'Firmware update complete.';
  } else {
    S.status = 'Firmware update failed.';
    alertBox('Firmware update failed', res.error);
  }
  if (res.reconnected) {
    S.conn.connected = true;
    S.conn.version = res.version;
    S.conn.state = 'Connected';
    S.status = `Connected: ${port}  |  ${res.version}`;
    requestRender();
    await readDevice();
    return;
  }
  if (S.conn.state === 'Disconnected (updating)') S.conn.state = 'Disconnected';
  requestRender();
}

/** Python dropped the connection to flash the board. */
export function onFirmwareDisconnect() {
  markDisconnected('Disconnected (updating)');
}

export function render() {
  const { conn } = S;
  const portOptions = S.ports.length ? S.ports : [];
  const portSelect = select({
    value: conn.port,
    options: portOptions,
    fallback: conn.port || 'No ports found',
    disabled: conn.connected || S.fw.busy,
    h: on({ change: (_e, el) => { S.conn.port = el.value; requestRender(); } }),
  });
  const baudSelect = select({
    value: conn.baud,
    options: BAUDS.map((b) => [b, b]),
    disabled: conn.connected || S.fw.busy,
    h: on({ change: (_e, el) => { S.conn.baud = el.value; requestRender(); } }),
  });
  const status = conn.connected ? `Connected on ${conn.port}` : conn.state;

  const serial = panel({
    title: 'Serial Connection',
    tag: conn.connected ? 'Connected' : 'Disconnected',
    body: [
      fields([
        field({ label: 'Port', control: portSelect }),
        field({ label: 'Baud', control: baudSelect }),
        field({ label: 'Status', control: html`<span class="static">${status}</span>` }),
        field({ label: 'Firmware', control: html`<span class="static">${conn.version || '--'}</span>` }),
      ]),
      actions([
        button('Refresh ports', { h: on({ click: () => refreshPorts() }), disabled: conn.connected }),
        button('Reset to defaults', { kind: 'ghost', h: on({ click: () => resetDefaults() }), disabled: !conn.connected,
          title: 'Reset all parameters to firmware defaults, then read them back' }),
        button(conn.connected ? 'Disconnect' : 'Connect', { kind: 'primary', h: on({ click: () => toggleConnection() }), disabled: S.fw.busy }),
      ]),
    ],
  });

  const firmware = panel({
    title: 'Firmware Update',
    tag: '.hex · no toolchain required',
    body: [
      fields([
        field({
          label: 'Firmware file',
          tip: 'The .hex firmware file supplied with your gauge.',
          control: textInput({
            value: S.fw.path, num: false, cls: 'wide', disabled: S.fw.busy, label: 'Firmware file',
            h: on({ input: (_e, el) => { S.fw.path = el.value; } }),
          }),
        }),
      ]),
      actions([
        button('Browse…', { h: on({ click: () => browse() }), disabled: S.fw.busy }),
        button('Update Firmware', { kind: 'primary', h: on({ click: () => update() }), disabled: S.fw.busy || !S.fw.available }),
      ]),
      progress(S.fw.pct),
      logView(S.fw.log),
    ],
  });

  return html`<div class="panel-grid">${serial}${firmware}</div>`;
}
