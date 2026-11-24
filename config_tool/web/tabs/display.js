// 08 / Outputs: gauge units, screen order for both OLEDs, the clock and the fuel tank.

import { html, on, panel, fields, button, attr } from '../ui.js';
import { S, val, setVal, paramField, selectField, imperial, requestRender, isDirty } from '../state.js';
import { parseScreenOrder, fuelCapacityDisplay, fuelCapacityToRaw, floatText } from '../units.js';

/**
 * UI order for a screen list. The stored value holds only the enabled screens, in order;
 * disabled ones follow in default order. An empty value means every screen, in default order.
 */
function listState(param) {
  const value = val(param);
  const ids = S.st.screens.map(([id]) => id);
  let entry = S.screens[param];
  if (!entry || entry.forValue !== value) {
    const enabled = parseScreenOrder(value, ids);
    const on = enabled.length ? enabled : [...ids];
    entry = { order: [...on, ...ids.filter((id) => !on.includes(id))], on, forValue: value };
    S.screens[param] = entry;
  }
  return entry;
}

function commit(param, entry) {
  const value = entry.order.filter((id) => entry.on.includes(id)).join(',');
  entry.forValue = value;
  setVal(param, value);
  requestRender();
}

function screenList(title, param, settingsLast) {
  const entry = listState(param);
  const names = Object.fromEntries(S.st.screens);
  const enabledOrder = entry.order.filter((id) => entry.on.includes(id));
  const move = (i, d) => on({ click: () => {
    const order = [...entry.order];
    [order[i], order[i + d]] = [order[i + d], order[i]];
    entry.order = order;
    commit(param, entry);
  } });
  const rows = entry.order.map((id, i) => {
    const enabled = entry.on.includes(id);
    const toggle = on({ change: (_e, el) => {
      if (!el.checked && entry.on.length === 1) {
        el.checked = true;
        S.status = 'At least one screen must stay enabled.';
        requestRender();
        return;
      }
      entry.on = el.checked ? [...entry.on, id] : entry.on.filter((x) => x !== id);
      commit(param, entry);
    } });
    return html`<div class="screen-row${enabled ? '' : ' off'}">
      <input type="checkbox"${attr('checked', enabled)}${toggle} aria-label="Show ${names[id]}">
      <span class="pos">${enabled ? String(enabledOrder.indexOf(id) + 1).padStart(2, '0') : ''}</span>
      <span class="name">${names[id]}</span>
      <div class="moves">
        ${button('', { kind: 'ghost', cls: 'btn-icon sm', iconName: 'chevron-up', title: 'Move up', h: move(i, -1), disabled: i === 0 })}
        ${button('', { kind: 'ghost', cls: 'btn-icon sm', iconName: 'chevron-down', title: 'Move down', h: move(i, 1), disabled: i === entry.order.length - 1 })}
      </div>
    </div>`;
  });
  const settings = settingsLast
    ? html`<div class="screen-row fixed"><span></span><span class="pos">—</span><span class="name">Settings</span><span class="unit">always last</span></div>`
    : '';
  return html`<section class="panel blueprint" title="${S.st.tooltips[param]}">
    <i class="corner tl"></i><i class="corner tr"></i><i class="corner bl"></i><i class="corner br"></i>
    <div class="panel-head"><h4>${isDirty(param) ? html`<span class="dirty-dot"></span>` : ''}${title}</h4><span class="panel-tag">${entry.on.length} enabled</span></div>
    <div class="screen-list">${rows}${settings}</div>
  </section>`;
}

export function render() {
  const units = imperial() ? 'gal' : 'L';
  return html`
    <div class="panel-grid three">
      <div class="stack">
      ${panel({ title: 'Display units', body: fields([selectField('Gauge unit system', 'units', S.st.unitSystemOptions, { note: '' })]) })}
      ${panel({
        title: 'Clock & Fuel',
        body: fields([
          paramField('UTC offset', 'clock_offset', { unit: 'hours', note: '' }),
          paramField('Fuel tank capacity', 'fuel_capacity', {
            unit: units, note: '',
            fmt: (raw) => fuelCapacityDisplay(raw, units),
            parse: (t) => floatText(fuelCapacityToRaw(t, units)),
          }),
        ]),
      })}
      <p class="panel-text">Checked screens are reachable with the rotary encoder. Display 1 always ends with Settings; display 2 is chosen from the Settings menu.</p>
      </div>
      ${screenList('Display 1 screens', 'disp1_order', true)}
      ${screenList('Display 2 screens', 'disp2_order', false)}
    </div>`;
}
