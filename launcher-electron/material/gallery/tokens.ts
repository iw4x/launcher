// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement, unsafeCSS } from 'lit';

import type { CSSResult, TemplateResult } from 'lit';

// Gallery sections of the system tokens, one element per kind of token. Each
// section paints its specimens from the tokens alone, so the page shows what
// the custom properties resolve to in the current scheme.
//
// Note that the page's content security policy forbids style attributes, so
// a specimen selects its tokens with a class and each section generates the
// rules for these classes from its list of tokens (see rules()).
//

// Return the stylesheet made of the rules that the function returns for the
// items.
//
function
rules<T> (items: readonly T[], rule: (x: T, i: number) => string): CSSResult
{
  return unsafeCSS (items.map (rule).join ('\n'));
}

// Styles that every section shares, which is a heading (in title large) over
// a grid of specimens, each named in label medium.
//
export const section_styles = css`
  :host
  {
    display: block;
    margin-block-end: 48px;
  }

  h2
  {
    margin: 0 0 16px;
    font-family: var(--md-sys-typescale-title-large-font);
    font-size: var(--md-sys-typescale-title-large-size);
    line-height: var(--md-sys-typescale-title-large-line-height);
    font-weight: var(--md-sys-typescale-title-large-weight);
    letter-spacing: var(--md-sys-typescale-title-large-tracking);
  }

  .grid
  {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(176px, 1fr));
    gap: 16px;
  }

  .name
  {
    color: var(--md-sys-color-on-surface-variant);
    font-family: var(--md-sys-typescale-label-medium-font);
    font-size: var(--md-sys-typescale-label-medium-size);
    line-height: var(--md-sys-typescale-label-medium-line-height);
    font-weight: var(--md-sys-typescale-label-medium-weight);
    letter-spacing: var(--md-sys-typescale-label-medium-tracking);
  }
`;

// Color role and the role of the content that sits on it.
//
interface color_pair
{
  role: string;
  on: string;
}

// Color roles by family. Note that we leave out the shadow and scrim roles
// since no content sits on them (the elevation section shows the shadow).
//
const color_families: readonly (readonly color_pair[])[] =
[
  [
    {role: 'primary',                   on: 'on-primary'},
    {role: 'primary-dim',               on: 'on-primary'},
    {role: 'primary-container',         on: 'on-primary-container'},
    {role: 'primary-fixed',             on: 'on-primary-fixed'},
    {role: 'primary-fixed-dim',         on: 'on-primary-fixed-variant'}
  ],
  [
    {role: 'secondary',                 on: 'on-secondary'},
    {role: 'secondary-dim',             on: 'on-secondary'},
    {role: 'secondary-container',       on: 'on-secondary-container'},
    {role: 'secondary-fixed',           on: 'on-secondary-fixed'},
    {role: 'secondary-fixed-dim',       on: 'on-secondary-fixed-variant'}
  ],
  [
    {role: 'tertiary',                  on: 'on-tertiary'},
    {role: 'tertiary-dim',              on: 'on-tertiary'},
    {role: 'tertiary-container',        on: 'on-tertiary-container'},
    {role: 'tertiary-fixed',            on: 'on-tertiary-fixed'},
    {role: 'tertiary-fixed-dim',        on: 'on-tertiary-fixed-variant'}
  ],
  [
    {role: 'error',                     on: 'on-error'},
    {role: 'error-dim',                 on: 'on-error'},
    {role: 'error-container',           on: 'on-error-container'}
  ],
  [
    {role: 'surface-dim',               on: 'on-surface'},
    {role: 'surface',                   on: 'on-surface'},
    {role: 'surface-bright',            on: 'on-surface'},
    {role: 'surface-container-lowest',  on: 'on-surface'},
    {role: 'surface-container-low',     on: 'on-surface'},
    {role: 'surface-container',         on: 'on-surface'},
    {role: 'surface-container-high',    on: 'on-surface'},
    {role: 'surface-container-highest', on: 'on-surface'},
    {role: 'surface',                   on: 'on-surface-variant'},
    {role: 'surface',                   on: 'outline'},
    {role: 'surface',                   on: 'outline-variant'},
    {role: 'background',                on: 'on-background'}
  ],
  [
    {role: 'inverse-surface',           on: 'inverse-on-surface'},
    {role: 'inverse-surface',           on: 'inverse-primary'}
  ]
];

// Roles that the color section paints with, without duplicates.
//
const color_roles: readonly string[] =
  [...new Set (color_families.flat ().flatMap ((p) => [p.role, p.on]))];

class gallery_color extends LitElement
{
  static override styles = [section_styles, css`
    .family
    {
      margin-block-end: 16px;
    }

    .swatch
    {
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      height: 80px;
      padding: 12px;
      box-sizing: border-box;
      border: 1px solid var(--md-sys-color-outline-variant);
      border-radius: var(--md-sys-shape-corner-medium);
    }

    .swatch .name
    {
      color: inherit;
    }
  `,
  rules (color_roles, (r) => `
    .background-${r} {background: var(--md-sys-color-${r});}
    .content-${r} {color: var(--md-sys-color-${r});}`)];

  override render (): TemplateResult
  {
    const swatch = (p: color_pair): TemplateResult => html`
      <div class="swatch background-${p.role} content-${p.on}">
        <span class="name">${p.role}</span>
        <span class="name">${p.on}</span>
      </div>`;

    return html`
      <h2>Color</h2>
      ${color_families.map ((f) => html`
        <div class="grid family">${f.map (swatch)}</div>`)}`;
  }
}

// Type scale roles in the order of the specification.
//
const type_roles: readonly string[] =
[
  'display-large',  'display-medium',  'display-small',
  'headline-large', 'headline-medium', 'headline-small',
  'title-large',    'title-medium',    'title-small',
  'body-large',     'body-medium',     'body-small',
  'label-large',    'label-medium',    'label-small'
];

class gallery_type extends LitElement
{
  static override styles = [section_styles, css`
    .role
    {
      display: grid;
      grid-template-columns: 176px 1fr 1fr;
      align-items: baseline;
      gap: 16px;
      margin-block-end: 8px;
    }

    p
    {
      margin: 0;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }
  `,
  rules (type_roles, (r) => `
    .type-${r}
    {
      font-family: var(--md-sys-typescale-${r}-font);
      font-size: var(--md-sys-typescale-${r}-size);
      line-height: var(--md-sys-typescale-${r}-line-height);
      font-weight: var(--md-sys-typescale-${r}-weight);
      letter-spacing: var(--md-sys-typescale-${r}-tracking);
    }

    .type-${r}.emphasized
    {
      font-family: var(--md-sys-typescale-emphasized-${r}-font);
      font-size: var(--md-sys-typescale-emphasized-${r}-size);
      line-height: var(--md-sys-typescale-emphasized-${r}-line-height);
      font-weight: var(--md-sys-typescale-emphasized-${r}-weight);
      letter-spacing: var(--md-sys-typescale-emphasized-${r}-tracking);
    }`)];

  override render (): TemplateResult
  {
    return html`
      <h2>Type</h2>
      ${type_roles.map ((r) => html`
        <div class="role">
          <span class="name">${r}</span>
          <p class="type-${r}">Regular</p>
          <p class="type-${r} emphasized">Emphasized</p>
        </div>`)}`;
  }
}

// Corner scale from square to fully rounded.
//
const shape_corners: readonly string[] =
[
  'none',
  'extra-small',
  'small',
  'medium',
  'large',
  'large-increased',
  'extra-large',
  'extra-large-increased',
  'extra-extra-large',
  'full'
];

class gallery_shape extends LitElement
{
  static override styles = [section_styles, css`
    .shape
    {
      height: 112px;
      margin-block-end: 8px;
      background: var(--md-sys-color-primary-container);
    }
  `,
  rules (shape_corners, (c) => `
    .corner-${c} {border-radius: var(--md-sys-shape-corner-${c});}`)];

  override render (): TemplateResult
  {
    return html`
      <h2>Shape</h2>
      <div class="grid">
        ${shape_corners.map ((c) => html`
          <div>
            <div class="shape corner-${c}"></div>
            <span class="name">${c}</span>
          </div>`)}
      </div>`;
  }
}

// Elevation levels with their heights in dp.
//
const elevation_levels: readonly number[] = [0, 1, 3, 6, 8, 12];

class gallery_elevation extends LitElement
{
  static override styles = [section_styles, css`
    .level
    {
      display: flex;
      align-items: flex-end;
      height: 112px;
      padding: 12px;
      box-sizing: border-box;
      border-radius: var(--md-sys-shape-corner-medium);
      background: var(--md-sys-color-surface-container-low);
    }
  `,
  rules (elevation_levels, (_, i) => `
    .level${i} {box-shadow: var(--m3-elevation-shadow-level${i});}`)];

  override render (): TemplateResult
  {
    return html`
      <h2>Elevation</h2>
      <div class="grid">
        ${elevation_levels.map ((dp, i) => html`
          <div class="level level${i}">
            <span class="name">level${i}, ${dp}dp</span>
          </div>`)}
      </div>`;
  }
}

// Interaction states that paint a state layer.
//
const states: readonly string[] = ['hover', 'focus', 'pressed', 'dragged'];

class gallery_state extends LitElement
{
  static override styles = [section_styles, css`
    .tile
    {
      position: relative;
      display: flex;
      align-items: flex-end;
      height: 80px;
      padding: 12px;
      box-sizing: border-box;
      border: none;
      border-radius: var(--md-sys-shape-corner-medium);
      background: var(--md-sys-color-secondary-container);
      color: var(--md-sys-color-on-secondary-container);
      font: inherit;
      text-align: start;
      overflow: hidden;
    }

    .layer
    {
      position: absolute;
      inset: 0;
      background: currentColor;
      opacity: 0;
      pointer-events: none;
    }

    .tile .name
    {
      position: relative;
      color: inherit;
    }

    /*
     * The live tile paints its layer from the state that it is in, the
     * pressed state taking precedence over the focus and hover states.
     */
    button.tile:hover .layer
    {
      opacity: var(--md-sys-state-hover-state-layer-opacity);
    }

    button.tile:focus-visible
    {
      outline: 3px solid var(--md-sys-color-secondary);
      outline-offset: 2px;
    }

    button.tile:focus-visible .layer
    {
      opacity: var(--md-sys-state-focus-state-layer-opacity);
    }

    button.tile:active .layer
    {
      opacity: var(--md-sys-state-pressed-state-layer-opacity);
    }
  `,
  rules (states, (s) => `
    .layer.${s} {opacity: var(--md-sys-state-${s}-state-layer-opacity);}`)];

  override render (): TemplateResult
  {
    return html`
      <h2>State</h2>
      <div class="grid">
        ${states.map ((s) => html`
          <div class="tile">
            <div class="layer ${s}"></div>
            <span class="name">${s}</span>
          </div>`)}
        <button class="tile" type="button">
          <div class="layer"></div>
          <span class="name">live</span>
        </button>
      </div>`;
  }
}

// Speeds of the motion springs.
//
const motion_speeds: readonly string[] = ['fast', 'default', 'slow'];

class gallery_motion extends LitElement
{
  static override properties = {moved: {type: Boolean, reflect: true}};

  static override styles = [section_styles, css`
    .spring
    {
      display: grid;
      grid-template-columns: 176px 1fr 96px;
      align-items: center;
      gap: 16px;
      margin-block-end: 16px;
    }

    .track
    {
      position: relative;
      height: 32px;
      border-radius: var(--md-sys-shape-corner-full);
      background: var(--md-sys-color-surface-container-highest);
    }

    /*
     * The spatial specimen moves along its track while the effects specimen
     * changes color.
     */
    .ball
    {
      position: absolute;
      inset-block: 4px;
      inset-inline-start: 4px;
      width: 24px;
      border-radius: var(--md-sys-shape-corner-full);
      background: var(--md-sys-color-primary);
      transition-property: inset-inline-start;
    }

    :host([moved]) .ball
    {
      inset-inline-start: calc(100% - 28px);
    }

    .fade
    {
      height: 32px;
      border-radius: var(--md-sys-shape-corner-full);
      background: var(--md-sys-color-surface-container-highest);
      transition-property: background-color;
    }

    :host([moved]) .fade
    {
      background: var(--md-sys-color-tertiary);
    }

    button
    {
      height: 32px;
      padding: 0 16px;
      margin-block-end: 16px;
      border: none;
      border-radius: var(--md-sys-shape-corner-full);
      background: var(--md-sys-color-primary);
      color: var(--md-sys-color-on-primary);
      font: inherit;
    }
  `,
  rules (motion_speeds, (s) =>
         ['spatial', 'effects'].map ((k) => `
    .${s}-${k}
    {
      transition-duration: var(--md-sys-motion-spring-${s}-${k}-duration);
      transition-timing-function: var(--md-sys-motion-spring-${s}-${k}-easing);
    }`).join (''))];

  // Whether the specimens are at their destination. Play toggles it, which
  // sends them there or back. Note that it is reflected for the styles.
  //
  declare moved: boolean;

  constructor ()
  {
    super ();
    this.moved = false;
  }

  override render (): TemplateResult
  {
    return html`
      <h2>Motion</h2>
      <button type="button" @click=${() => {this.moved = !this.moved;}}>
        Play
      </button>
      ${motion_speeds.map ((s) => html`
        <div class="spring">
          <span class="name">${s}-spatial, ${s}-effects</span>
          <div class="track">
            <div class="ball ${s}-spatial"></div>
          </div>
          <div class="fade ${s}-effects"></div>
        </div>`)}`;
  }
}

customElements.define ('m3-gallery-color', gallery_color);
customElements.define ('m3-gallery-type', gallery_type);
customElements.define ('m3-gallery-shape', gallery_shape);
customElements.define ('m3-gallery-elevation', gallery_elevation);
customElements.define ('m3-gallery-state', gallery_state);
customElements.define ('m3-gallery-motion', gallery_motion);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-gallery-color': gallery_color;
    'm3-gallery-type': gallery_type;
    'm3-gallery-shape': gallery_shape;
    'm3-gallery-elevation': gallery_elevation;
    'm3-gallery-state': gallery_state;
    'm3-gallery-motion': gallery_motion;
  }
}
