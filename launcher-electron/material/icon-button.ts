// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement } from 'lit';

import type { TemplateResult } from 'lit';

import './focus-ring';
import './icon';
import './ripple';

// Icon button, an icon in a round container that acts on a click. For
// example:
//
// <m3-icon-button icon="arrow_back" label="Back"></m3-icon-button>
//
// The icon names the Material Symbols icon. The label is the accessible name,
// which the button needs since it shows no text.
//
// The variant is standard (no container, the default), filled, tonal, or
// outlined. The size is extra small (a 32dp container with a 20dp icon) or
// small (a 40dp container with a 24dp icon, the default).
//
// While pressed, the container takes the small corner, changing shape with
// the default effects spring (which doesn't bounce).
//
export class icon_button extends LitElement
{
  static override shadowRootOptions =
    {...LitElement.shadowRootOptions, delegatesFocus: true};

  static override properties =
  {
    icon:     {type: String},
    label:    {type: String},
    variant:  {type: String, reflect: true},
    size:     {type: String, reflect: true},
    disabled: {type: Boolean, reflect: true}
  };

  static override styles = css`
    :host
    {
      display: inline-flex;
      flex: none;
      --_size: 40px;
      --_icon-size: 24px;
      --_color: var(--md-sys-color-on-surface-variant);
      --_container: transparent;
      --_outline: transparent;
    }

    :host([size='extra-small'])
    {
      --_size: 32px;
      --_icon-size: 20px;
    }

    :host([variant='filled'])
    {
      --_color: var(--md-sys-color-on-primary);
      --_container: var(--md-sys-color-primary);
    }

    :host([variant='tonal'])
    {
      --_color: var(--md-sys-color-on-secondary-container);
      --_container: var(--md-sys-color-secondary-container);
    }

    :host([variant='outlined'])
    {
      --_outline: var(--md-sys-color-outline-variant);
    }

    :host([disabled])
    {
      --_color: rgb(from var(--md-sys-color-on-surface) r g b / 0.38);
    }

    :host([disabled][variant='filled']),
    :host([disabled][variant='tonal'])
    {
      --_container: rgb(from var(--md-sys-color-on-surface) r g b / 0.1);
    }

    :host([disabled][variant='outlined'])
    {
      --_outline: var(--md-sys-color-outline-variant);
    }

    button
    {
      position: relative;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      width: var(--_size);
      height: var(--_size);
      padding: 0;
      border: 1px solid var(--_outline);
      border-radius: calc(var(--_size) / 2);
      box-sizing: border-box;
      background: var(--_container);
      color: var(--_color);
      outline: none;
      cursor: pointer;
      -webkit-tap-highlight-color: transparent;
      transition-property: border-radius;
      transition-duration:
        var(--md-sys-motion-spring-default-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-effects-easing);
    }

    button:active:not(:disabled)
    {
      border-radius: var(--md-sys-shape-corner-small);
    }

    button:disabled
    {
      cursor: default;
    }

    m3-icon
    {
      --m3-icon-size: var(--_icon-size);
    }
  `;

  declare icon: string;
  declare label: string;
  declare variant: 'standard' | 'filled' | 'tonal' | 'outlined';
  declare size: 'extra-small' | 'small';
  declare disabled: boolean;

  constructor ()
  {
    super ();
    this.icon = '';
    this.label = '';
    this.variant = 'standard';
    this.size = 'small';
    this.disabled = false;
  }

  override render (): TemplateResult
  {
    return html`
      <button type="button"
              aria-label=${this.label}
              ?disabled=${this.disabled}>
        <m3-focus-ring></m3-focus-ring>
        <m3-ripple></m3-ripple>
        <m3-icon>${this.icon}</m3-icon>
      </button>`;
  }
}

customElements.define ('m3-icon-button', icon_button);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-icon-button': icon_button;
  }
}
