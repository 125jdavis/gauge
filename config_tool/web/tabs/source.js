// 02 / Inputs: where each signal comes from.

import { html, panel, fields } from '../ui.js';
import { S, selectField } from '../state.js';

const sourceField = (label, p) => selectField(label, p, S.st.sources[p], { note: '', fallback: 'Unsupported source' });

export function render() {
  return html`<div class="panel-grid">
    ${panel({ title: 'CAN Bus', body: fields([selectField('CAN protocol', 'can_protocol', S.st.canProtocols, { note: '' })]) })}
    ${panel({ title: 'Speed Source', body: fields([sourceField('Speed source', 'speed_source')]) })}
    ${panel({ title: 'RPM Source', body: fields([sourceField('RPM source', 'rpm_source')]) })}
    ${panel({
      title: 'Other Signal Sources',
      body: fields([
        sourceField('Oil pressure', 'oil_prs_source'),
        sourceField('Fuel pressure', 'fuel_prs_source'),
        sourceField('Coolant temp', 'coolant_src'),
        sourceField('Oil temp', 'oil_temp_src'),
        sourceField('MAP / boost', 'map_source'),
        sourceField('Lambda / AFR', 'lambda_source'),
        sourceField('Fuel level', 'fuel_lvl_src'),
      ]),
    })}
  </div>`;
}
