// 03 / Inputs: vehicle speed sensor, engine RPM pickup and the mechanical odometer.

import { html, panel, fields } from '../ui.js';
import { paramField, filterField } from '../state.js';
import { displayNumber, hallSpeedMinToRaw, pyInt } from '../units.js';

export function render() {
  const hallMinDisplay = (raw) => { try { return displayNumber(pyInt(raw) / 100); } catch { return raw; } };

  return html`<div class="panel-grid">
    ${panel({
      title: 'Speed & Hall Sensor',
      body: fields([
        paramField('Revs per km', 'revs_per_km'),
        paramField('Teeth per rev', 'teeth_per_rev'),
        paramField('Minimum speed cutoff', 'hall_speed_min', {
          unit: 'km/h', fmt: hallMinDisplay, parse: (t) => hallSpeedMinToRaw(t),
        }),
        filterField('Hall filter', 'filter_hall'),
        filterField('GPS filter', 'filter_gps'),
      ]),
    })}
    ${panel({
      title: 'Engine RPM Sensor',
      body: fields([
        paramField('Cylinder count', 'cyl_count'),
        paramField('Debounce', 'rpm_debounce_us', { unit: 'µs' }),
        paramField('Engine RPM min', 'engine_rpm_min'),
        filterField('RPM filter', 'filter_rpm'),
      ]),
    })}
    ${panel({
      title: 'Mechanical odometer calibration',
      body: fields([
        paramField('Steps per revolution', 'odo_steps'),
        paramField('Motor gear teeth', 'odo_motor_teeth'),
        paramField('Driven gear teeth', 'odo_gear_teeth'),
      ]),
    })}
  </div>`;
}
