// Rendering primitives shared by the shell and the tabs.
//
// Views are plain functions returning `html` template results. render() in app.js
// morphs the new markup into the live DOM, so focus, caret position and slider drags
// survive a re-render, and event handlers are looked up through data-h attributes.

import { ICONS } from './icons.js';

// ---- templating ----------------------------------------------------------------

class Html {
  constructor(text) { this.text = text; }
  toString() { return this.text; }
}

const ESC = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' };
export const esc = (value) => String(value).replace(/[&<>"']/g, (c) => ESC[c]);

function flatten(value) {
  if (value == null || value === false || value === true) return '';
  if (value instanceof Html) return value.text;
  if (Array.isArray(value)) return value.map(flatten).join('');
  return esc(value);
}

/** Tagged template: interpolations are escaped unless they are html`` results (or arrays of them). */
export function html(strings, ...values) {
  let out = strings[0];
  values.forEach((value, i) => { out += flatten(value) + strings[i + 1]; });
  return new Html(out);
}

/** Trusted markup (icons). */
export const unsafe = (text) => new Html(text);

/** Boolean attribute helper: html`<input ${attr('disabled', x)}>` */
export const attr = (name, on) => unsafe(on ? ` ${name}` : '');

export const icon = (name) => unsafe(ICONS[name] || '');

// ---- event handlers --------------------------------------------------------------

let handlers = new Map();
let nextId = 0;

/** Register {input, change, click} callbacks for one element; returns the data-h attribute. */
export function on(callbacks) {
  const id = `h${nextId++}`;
  handlers.set(id, callbacks);
  return unsafe(` data-h="${id}"`);
}

export function resetHandlers() {
  handlers = new Map();
  nextId = 0;
}

export function dispatch(event) {
  const el = event.target.closest && event.target.closest('[data-h]');
  if (!el || el.disabled) return;
  const callbacks = handlers.get(el.dataset.h);
  const fn = callbacks && callbacks[event.type];
  if (fn) fn(event, el);
}

// ---- DOM morphing ----------------------------------------------------------------

function syncAttributes(from, to) {
  for (const { name } of [...from.attributes]) {
    if (!to.hasAttribute(name)) from.removeAttribute(name);
  }
  for (const { name, value } of [...to.attributes]) {
    if (from.getAttribute(name) !== value) from.setAttribute(name, value);
  }
}

function morphNode(from, to) {
  const sameKind = from.nodeType === to.nodeType && from.nodeName === to.nodeName
    && (from.nodeName !== 'INPUT' || from.type === to.type);
  if (!sameKind) {
    from.replaceWith(to);
    return;
  }
  if (from.nodeType !== Node.ELEMENT_NODE) {
    if (from.nodeValue !== to.nodeValue) from.nodeValue = to.nodeValue;
    return;
  }
  syncAttributes(from, to);
  if (from.nodeName === 'INPUT') {
    if (from.type === 'checkbox' || from.type === 'radio') {
      from.checked = to.hasAttribute('checked');
    } else if (document.activeElement !== from) {
      // Never rewrite what the user is typing; blur re-renders the canonical value.
      const value = to.getAttribute('value') ?? '';
      if (from.value !== value) from.value = value;
    }
  }
  morphChildren(from, to);
  if (from.nodeName === 'SELECT') {
    const value = to.getAttribute('data-value') ?? '';
    if (from.value !== value) from.value = value;
  }
}

function morphChildren(from, to) {
  const oldKids = [...from.childNodes];
  const newKids = [...to.childNodes];
  newKids.forEach((kid, i) => {
    if (i < oldKids.length) morphNode(oldKids[i], kid);
    else from.appendChild(kid);
  });
  for (let i = newKids.length; i < oldKids.length; i++) oldKids[i].remove();
}

/** Replace the contents of *container* with *markup*, reusing matching nodes. */
export function morph(container, markup) {
  const tpl = document.createElement('template');
  tpl.innerHTML = String(markup);
  morphChildren(container, tpl.content);
  // A select's value can only be applied once its options exist.
  container.querySelectorAll('select[data-value]').forEach((sel) => {
    if (sel.value !== sel.dataset.value) sel.value = sel.dataset.value;
  });
}

// ---- components ------------------------------------------------------------------

const CORNERS = unsafe('<i class="corner tl"></i><i class="corner tr"></i><i class="corner bl"></i><i class="corner br"></i>');

/** A blueprint card. body is any html. */
export function panel({ title, tag = '', wide = false, head = null, body, cls = '', dirty = false }) {
  return html`<section class="panel blueprint${wide ? ' wide' : ''}${cls ? ` ${cls}` : ''}">${CORNERS}
    <div class="panel-head"><h4>${dirty ? html`<span class="dirty-dot"></span>` : ''}${title}</h4>${head ?? html`<span class="panel-tag">${tag}</span>`}</div>
    ${body}
  </section>`;
}

/**
 * One labelled row. control is html; note explains a disabled state or an error.
 */
export function field({ label, control, unit = '', note = '', error = '', disabled = false, dirty = false, tip = '' }) {
  const text = error || note;
  return html`<div class="field"${attr('aria-disabled="true"', disabled)}>
    <label${tip ? html` title="${tip}"` : ''}>${dirty ? html`<span class="dirty-dot"></span>` : ''}<span>${label}</span></label>
    <div class="control">${control}${unit ? html`<span class="unit">${unit}</span>` : ''}</div>
    ${text ? html`<div class="note${error ? ' is-error' : ''}">${text}</div>` : ''}
  </div>`;
}

export const fields = (list) => html`<div class="fields">${list}</div>`;

export function button(label, { kind = 'secondary', h = '', disabled = false, iconName = '', title = '', cls = '' } = {}) {
  const primary = kind === 'primary';
  return html`<button type="button" class="btn btn-${kind}${primary ? ' blueprint' : ''}${cls ? ` ${cls}` : ''}"${h}${attr('disabled', disabled)}${title ? html` title="${title}"` : ''}>${primary ? CORNERS : ''}${iconName ? icon(iconName) : ''}${label}</button>`;
}

export const actions = (list) => html`<div class="actions">${list}</div>`;

export function progress(pct, busy = false) {
  const value = Math.max(0, Math.min(100, Math.round(pct || 0)));
  return html`<div class="progress"><div class="track${busy ? ' busy' : ''}"><div class="fill" style="width:${busy ? 30 : value}%"></div></div>${busy ? '' : html`<span class="pct">${value}%</span>`}</div>`;
}

export function errorBox(messages) {
  const list = (Array.isArray(messages) ? messages : [messages]).filter(Boolean);
  if (!list.length) return '';
  return html`<div class="error-box" role="alert">${icon('alert')}<div>${list.map((m) => html`<p>${m}</p>`)}</div></div>`;
}

export function logView(lines) {
  return html`<div class="log" aria-live="polite">${lines.map((line) => html`<div class="${line.startsWith('>') ? 'tx' : ''}">${line || ' '}</div>`)}</div>`;
}

/** <select> whose options are [value, label] pairs. An unknown value shows as `fallback`. */
export function select({ value, options, h, disabled = false, dirty = false, cls = '', fallback = 'Unsupported' }) {
  const known = options.some(([v]) => v === value);
  const extra = !known ? html`<option value="${value}" disabled>${value === '' ? '' : fallback}</option>` : '';
  return html`<select class="input${cls ? ` ${cls}` : ''}${dirty ? ' is-dirty' : ''}" data-value="${value}"${h}${attr('disabled', disabled)}>${extra}${options.map(([v, label]) => html`<option value="${v}">${label}</option>`)}</select>`;
}

export function textInput({ value, h, disabled = false, dirty = false, num = true, cls = '', label = '' }) {
  return html`<input class="input${num ? ' num' : ''}${cls ? ` ${cls}` : ''}${dirty ? ' is-dirty' : ''}" value="${value}"${h}${attr('disabled', disabled)} spellcheck="false" autocomplete="off"${label ? html` aria-label="${label}"` : ''}>`;
}

export function range({ value, min, max, step = 1, h, disabled = false, label = '' }) {
  return html`<input type="range" min="${min}" max="${max}" step="${step}" value="${value}"${h}${attr('disabled', disabled)}${label ? html` aria-label="${label}"` : ''}>`;
}

export function dirtyDot(on) {
  return on ? html`<span class="dirty-dot"></span>` : '';
}
