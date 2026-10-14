// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement } from 'lit';
import { live } from 'lit/directives/live.js';

import type { TemplateResult } from 'lit';

import './focus-ring';

// Checkbox that turns an option on or off. For example:
//
// <m3-checkbox checked>Remember me</m3-checkbox>
//
// The text content is the label, which also takes clicks. A click toggles
// checked and dispatches the change event.
//
// The box is 18dp with a 2dp outline in the on-surface-variant role. While
// checked, it is filled with primary and shows the check mark in on-primary.
// Underneath is a native checkbox, so the keyboard and assistive technology
// handle it as one.
//
// When checked, the box fills with the default effects spring and the mark
// draws itself with the default spatial spring. When unchecked, the box
// empties with the fast effects spring and the mark disappears 100ms later.
//
export class checkbox extends LitElement
{
  static override shadowRootOptions =
    {...LitElement.shadowRootOptions, delegatesFocus: true};

  static override properties =
  {
    checked:  {type: Boolean, reflect: true},
    disabled: {type: Boolean, reflect: true}
  };

  static override styles = css`
    :host
    {
      display: inline-flex;
      vertical-align: middle;
      --_outline: var(--md-sys-color-on-surface-variant);
      --_fill: transparent;
      --_mark: transparent;
      --_label: var(--md-sys-color-on-surface);
    }

    :host([checked])
    {
      --_outline: var(--md-sys-color-primary);
      --_fill: var(--md-sys-color-primary);
      --_mark: var(--md-sys-color-on-primary);
    }

    :host([disabled])
    {
      --_outline: rgb(from var(--md-sys-color-on-surface) r g b / 0.38);
      --_label: rgb(from var(--md-sys-color-on-surface) r g b / 0.38);
    }

    :host([disabled][checked])
    {
      --_fill: rgb(from var(--md-sys-color-on-surface) r g b / 0.38);
      --_outline: transparent;
      --_mark: var(--md-sys-color-surface);
    }

    label
    {
      display: inline-flex;
      align-items: center;
      gap: 4px;
      color: var(--_label);
      cursor: pointer;
    }

    :host([disabled]) label
    {
      cursor: default;
    }

    /*
     * The target is a 40dp circle around the box that shows the state layer
     * while the label is hovered or pressed.
     */
    .target
    {
      position: relative;
      display: inline-flex;
      flex: none;
      align-items: center;
      justify-content: center;
      width: 40px;
      height: 40px;
      border-radius: var(--md-sys-shape-corner-full);
    }

    .target::before
    {
      content: '';
      position: absolute;
      inset: 0;
      border-radius: var(--md-sys-shape-corner-full);
      background: var(--md-sys-color-on-surface);
      opacity: 0;
      transition-property: opacity;
      transition-duration:
        var(--md-sys-motion-spring-fast-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-effects-easing);
    }

    :host([checked]) .target::before
    {
      background: var(--md-sys-color-primary);
    }

    :host(:not([disabled])) label:hover .target::before
    {
      opacity: var(--md-sys-state-hover-state-layer-opacity);
    }

    :host(:not([disabled])) label:active .target::before
    {
      opacity: var(--md-sys-state-pressed-state-layer-opacity);
    }

    /*
     * The native checkbox is invisible and covers the target, so it receives
     * the input.
     */
    input
    {
      position: absolute;
      inset: 0;
      margin: 0;
      opacity: 0;
      cursor: inherit;
    }

    .box
    {
      position: relative;
      box-sizing: border-box;
      width: 18px;
      height: 18px;
      border: 2px solid var(--_outline);
      border-radius: 2px;
      background: var(--_fill);
      color: var(--_mark);
      pointer-events: none;
      transition-property: background-color, border-color;
      transition-duration:
        var(--md-sys-motion-spring-fast-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-effects-easing);
    }

    :host([checked]) .box
    {
      transition-duration:
        var(--md-sys-motion-spring-default-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-effects-easing);
    }

    /*
     * The mark covers the whole box, outline included. Its path is of unit
     * length, so the dash offset is the part that is not yet drawn.
     */
    svg
    {
      position: absolute;
      inset: -2px;
      width: 18px;
      height: 18px;
      fill: none;
      stroke: var(--_mark);
      stroke-width: 2px;
      stroke-linecap: square;
      stroke-dasharray: 1;
      stroke-dashoffset: 1;
      transition-property: stroke-dashoffset;
      transition-duration: 0s;
      transition-delay: 100ms;
    }

    :host([checked]) svg
    {
      stroke-dashoffset: 0;
      transition-duration:
        var(--md-sys-motion-spring-default-spatial-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-spatial-easing);
      transition-delay: 0s;
    }
  `;

  declare checked: boolean;
  declare disabled: boolean;

  constructor ()
  {
    super ();
    this.checked = false;
    this.disabled = false;
  }

  override render (): TemplateResult
  {
    return html`
      <label>
        <span class="target">
          <m3-focus-ring></m3-focus-ring>
          <input type="checkbox"
                 .checked=${live (this.checked)}
                 ?disabled=${this.disabled}
                 @change=${this.on_change}>
          <span class="box">
            <svg viewBox="0 0 18 18" aria-hidden="true">
              <polyline points="4.5 9, 7.2 11.7, 13.5 5.4" pathLength="1"/>
            </svg>
          </span>
        </span>
        <slot></slot>
      </label>`;
  }

  // Attach the focus ring to the native checkbox, so that the ring goes
  // around the target while the checkbox has keyboard focus.
  //
  protected override firstUpdated (): void
  {
    const i = this.renderRoot.querySelector ('input');

    if (i !== null)
      this.renderRoot.querySelector ('m3-focus-ring')?.attach (i);
  }

  // Update checked and dispatch our own change event since the native one
  // doesn't leave the shadow tree (it's not composed).
  //
  private on_change (e: Event): void
  {
    this.checked = (e.currentTarget as HTMLInputElement).checked;
    this.dispatchEvent (new Event ('change', {bubbles: true}));
  }
}

customElements.define ('m3-checkbox', checkbox);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-checkbox': checkbox;
  }
}
