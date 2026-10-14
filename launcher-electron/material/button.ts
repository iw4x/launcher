// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement, nothing } from 'lit';

import type { TemplateResult } from 'lit';

import './focus-ring';
import './icon';
import './ripple';

// Button, a pill-shaped label that acts on a click (or a link if there is an
// href). For example:
//
// <m3-button variant="filled" icon="play_arrow">Play</m3-button>
// <m3-button variant="text" href="#/servers">Explore all</m3-button>
//
// The text content is the label and the icon, if any, names the Material
// Symbols icon shown before it. In the order of decreasing emphasis, the
// variant is filled (the default), tonal, elevated, outlined, or text. The
// size is small (40dp high with label large type, the default) or medium
// (56dp high with title medium type).
//
// While pressed, the container takes a smaller corner (small or medium,
// according to the size), changing shape with the default effects spring
// (which doesn't bounce). On hover, the filled and tonal buttons rise to
// elevation level 1 and the elevated one to level 2.
//
export class button extends LitElement
{
  static override shadowRootOptions =
    {...LitElement.shadowRootOptions, delegatesFocus: true};

  static override properties =
  {
    variant:  {type: String, reflect: true},
    size:     {type: String, reflect: true},
    icon:     {type: String},
    href:     {type: String},
    disabled: {type: Boolean, reflect: true}
  };

  static override styles = css`
    :host
    {
      display: inline-flex;
      flex: none;
      vertical-align: middle;
      --_height: 40px;
      --_padding: 16px;
      --_icon-size: 20px;
      --_pressed-corner: var(--md-sys-shape-corner-small);
      --_font: var(--md-sys-typescale-label-large-font);
      --_font-size: var(--md-sys-typescale-label-large-size);
      --_line-height: var(--md-sys-typescale-label-large-line-height);
      --_weight: var(--md-sys-typescale-label-large-weight);
      --_tracking: var(--md-sys-typescale-label-large-tracking);
      --_color: var(--md-sys-color-on-primary);
      --_container: var(--md-sys-color-primary);
      --_outline: transparent;
      --_shadow: none;
    }

    :host([size='medium'])
    {
      --_height: 56px;
      --_padding: 24px;
      --_icon-size: 24px;
      --_pressed-corner: var(--md-sys-shape-corner-medium);
      --_font: var(--md-sys-typescale-title-medium-font);
      --_font-size: var(--md-sys-typescale-title-medium-size);
      --_line-height: var(--md-sys-typescale-title-medium-line-height);
      --_weight: var(--md-sys-typescale-title-medium-weight);
      --_tracking: var(--md-sys-typescale-title-medium-tracking);
    }

    :host([variant='tonal'])
    {
      --_color: var(--md-sys-color-on-secondary-container);
      --_container: var(--md-sys-color-secondary-container);
    }

    :host([variant='outlined'])
    {
      --_color: var(--md-sys-color-on-surface-variant);
      --_container: transparent;
      --_outline: var(--md-sys-color-outline-variant);
    }

    :host([variant='text'])
    {
      --_color: var(--md-sys-color-primary);
      --_container: transparent;
      --_padding: 12px;
    }

    :host([variant='elevated'])
    {
      --_color: var(--md-sys-color-primary);
      --_container: var(--md-sys-color-surface-container-low);
      --_shadow: var(--m3-elevation-shadow-level1);
    }

    :host([disabled])
    {
      --_color: rgb(from var(--md-sys-color-on-surface-variant) r g b / 0.38);
      --_shadow: none;
    }

    :host([disabled]:not([variant='outlined']):not([variant='text']))
    {
      --_container: rgb(from var(--md-sys-color-on-surface) r g b / 0.1);
    }

    :host([disabled][variant='outlined'])
    {
      --_outline: var(--md-sys-color-outline-variant);
    }

    /*
     * The control fills its host, so if the page stretches the host (across
     * a form, say), then the button is as wide.
     */
    .control
    {
      position: relative;
      display: inline-flex;
      flex: 1;
      align-items: center;
      justify-content: center;
      gap: 8px;
      box-sizing: border-box;
      height: var(--_height);
      padding-inline: var(--_padding);
      border: 1px solid var(--_outline);
      border-radius: calc(var(--_height) / 2);
      background: var(--_container);
      box-shadow: var(--_shadow);
      color: var(--_color);
      font-family: var(--_font);
      font-size: var(--_font-size);
      line-height: var(--_line-height);
      font-weight: var(--_weight);
      letter-spacing: var(--_tracking);
      white-space: nowrap;
      text-decoration: none;
      outline: none;
      cursor: pointer;
      -webkit-tap-highlight-color: transparent;
      -webkit-user-drag: none;
      transition-property: border-radius, box-shadow;
      transition-duration:
        var(--md-sys-motion-spring-default-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-effects-easing);
    }

    :host(:is([variant='filled'], [variant='tonal']):not([disabled]))
      .control:hover
    {
      box-shadow: var(--m3-elevation-shadow-level1);
    }

    :host([variant='elevated']:not([disabled])) .control:hover
    {
      box-shadow: var(--m3-elevation-shadow-level2);
    }

    .control:active:not(:disabled)
    {
      border-radius: var(--_pressed-corner);
    }

    .control:disabled
    {
      cursor: default;
    }

    m3-icon
    {
      --m3-icon-size: var(--_icon-size);
    }
  `;

  declare variant: 'filled' | 'tonal' | 'outlined' | 'text' | 'elevated';
  declare size: 'small' | 'medium';
  declare icon: string;
  declare href: string;
  declare disabled: boolean;

  constructor ()
  {
    super ();
    this.variant = 'filled';
    this.size = 'small';
    this.icon = '';
    this.href = '';
    this.disabled = false;
  }

  // Render a link if we have an href and the button is enabled. Otherwise,
  // render a button. Note that HTML has no disabled link, so a disabled
  // button with an href is still a button.
  //
  override render (): TemplateResult
  {
    const content = html`
      <m3-focus-ring></m3-focus-ring>
      <m3-ripple></m3-ripple>
      ${this.icon !== '' ? html`<m3-icon>${this.icon}</m3-icon>` : nothing}
      <slot></slot>`;

    return this.href !== '' && !this.disabled
      ? html`<a class="control" href=${this.href}>${content}</a>`
      : html`
        <button class="control" type="button" ?disabled=${this.disabled}>
          ${content}
        </button>`;
  }
}

customElements.define ('m3-button', button);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-button': button;
  }
}
