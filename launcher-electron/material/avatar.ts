// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement, nothing } from 'lit';

import type { TemplateResult } from 'lit';

// Presence names that we append to the accessible name (the dot itself is
// hidden from assistive technology).
//
const presence_labels: Record<string, string> =
{
  online:  'Online',
  active:  'Active',
  away:    'Away',
  offline: 'Offline'
};

// Number of tones that an avatar without an image picks from by its name
// (see tone()).
//
const tones = 3;

// Return the tone of the name, which is the same wherever the name appears.
//
// We hash the name with 32-bit FNV-1a and scale the hash to the number of
// tones, which takes its high bits. Note that the multiplication only carries
// each character into the higher bits, so the low bits (which a remainder
// would take) are poorly mixed.
//
function
tone (name: string): number
{
  let h = 0x811c9dc5;

  for (const c of name)
    h = Math.imul (h ^ (c.codePointAt (0) ?? 0), 0x01000193) >>> 0;

  return Math.floor (h * tones / 2 ** 32);
}

// Return the initials of the name, that is, the first letters of its first
// two words (or of its only word).
//
function
initials (name: string): string
{
  const ws = name.trim ().split (/\s+/).filter ((w) => w !== '');
  return ws.slice (0, 2).map ((w) => [...w][0] ?? '').join ('').toUpperCase ();
}

// Avatar, a round picture of a person or a group. For example:
//
// <m3-avatar name="Kestrel" presence="online"></m3-avatar>
//
// The name is also the accessible name. If there is no image (src), then the
// avatar shows the name's initials on a tone that the name picks, so that a
// person has the same color everywhere. The tones are the primary and
// tertiary containers plus the secondary fixed dim role (we leave out the
// secondary container since that's what a selected list item sits on).
//
// The size is small (32dp), medium (40dp, the default), or large (56dp).
//
// The presence, if any, is a dot on the bottom end corner of the avatar,
// colored as follows:
//
// online   primary
// active   tertiary (the person is playing, say)
// away     outline
// offline  outline ring
//
// The dot has a ring of the surface that the avatar sits on, which is the
// surface role unless --m3-avatar-surface says otherwise.
//
export class avatar extends LitElement
{
  static override properties =
  {
    name:     {type: String},
    src:      {type: String},
    size:     {type: String, reflect: true},
    presence: {type: String, reflect: true}
  };

  static override styles = css`
    :host
    {
      position: relative;
      display: inline-flex;
      flex: none;
      --_size: 40px;
      --_dot: 12px;
      --_type-size: var(--md-sys-typescale-title-medium-size);
    }

    :host([size='small'])
    {
      --_size: 32px;
      --_dot: 10px;
      --_type-size: var(--md-sys-typescale-label-large-size);
    }

    :host([size='large'])
    {
      --_size: 56px;
      --_dot: 16px;
      --_type-size: var(--md-sys-typescale-title-large-size);
    }

    .picture
    {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      width: var(--_size);
      height: var(--_size);
      border-radius: var(--md-sys-shape-corner-full);
      overflow: hidden;
      background: var(--_tone);
      color: var(--_on);
      font-family: var(--md-sys-typescale-title-medium-font);
      font-size: var(--_type-size);
      font-weight: var(--md-sys-typescale-title-medium-weight);
      user-select: none;
    }

    .picture.tone-0
    {
      --_tone: var(--md-sys-color-primary-container);
      --_on: var(--md-sys-color-on-primary-container);
    }

    .picture.tone-1
    {
      --_tone: var(--md-sys-color-secondary-fixed-dim);
      --_on: var(--md-sys-color-on-secondary-fixed);
    }

    .picture.tone-2
    {
      --_tone: var(--md-sys-color-tertiary-container);
      --_on: var(--md-sys-color-on-tertiary-container);
    }

    img
    {
      width: 100%;
      height: 100%;
      object-fit: cover;
    }

    /*
     * The ring separates the dot from the picture, whatever its colors.
     */
    .dot
    {
      position: absolute;
      inset-block-end: 0;
      inset-inline-end: 0;
      width: var(--_dot);
      height: var(--_dot);
      border: 2px solid var(--m3-avatar-surface, var(--md-sys-color-surface));
      border-radius: var(--md-sys-shape-corner-full);
      box-sizing: content-box;
      margin: -2px;
      background: var(--_dot-color);
    }

    :host([presence='online']) .dot
    {
      --_dot-color: var(--md-sys-color-primary);
    }

    :host([presence='active']) .dot
    {
      --_dot-color: var(--md-sys-color-tertiary);
    }

    :host([presence='away']) .dot
    {
      --_dot-color: var(--md-sys-color-outline);
    }

    :host([presence='offline']) .dot
    {
      --_dot-color: var(--m3-avatar-surface, var(--md-sys-color-surface));
      box-shadow: inset 0 0 0 2px var(--md-sys-color-outline);
    }
  `;

  declare name: string;
  declare src: string;
  declare size: 'small' | 'medium' | 'large';
  declare presence: '' | 'online' | 'active' | 'away' | 'offline';

  constructor ()
  {
    super ();
    this.name = '';
    this.src = '';
    this.size = 'medium';
    this.presence = '';
  }

  override render (): TemplateResult
  {
    const p = presence_labels[this.presence];
    const label = p !== undefined ? `${this.name}, ${p}` : this.name;

    return html`
      <div class="picture tone-${tone (this.name)}" role="img"
           aria-label=${label}>
        ${this.src !== ''
          ? html`<img src=${this.src} alt="">`
          : html`<span aria-hidden="true">${initials (this.name)}</span>`}
      </div>
      ${p !== undefined
        ? html`<span class="dot" aria-hidden="true"></span>`
        : nothing}`;
  }
}

customElements.define ('m3-avatar', avatar);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-avatar': avatar;
  }
}
