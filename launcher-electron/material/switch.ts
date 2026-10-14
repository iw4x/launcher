// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement } from 'lit';
import { live } from 'lit/directives/live.js';

import type { TemplateResult } from 'lit';

import './focus-ring';

// Switch that turns a setting on or off immediately. For example:
//
// <m3-switch selected>Friend activity</m3-switch>
//
// The text content is the label, which comes first and also takes clicks,
// with the switch at the end of the row. A click toggles selected and
// dispatches the change event.
//
// The track is 52 by 32dp. When off, it is surface container highest with an
// outline and has a 16dp handle in the outline role at the start. When on,
// it is primary and has a 24dp handle in on-primary at the end. The handle
// moves with the fast spatial spring and grows to 28dp while pressed.
// Underneath is a native checkbox with the switch role, so the keyboard and
// assistive technology handle it as a switch.
//
// Note that the class name has a trailing underscore since switch is a
// keyword.
//
export class switch_ extends LitElement
{
  static override shadowRootOptions =
    {...LitElement.shadowRootOptions, delegatesFocus: true};

  static override properties =
  {
    selected: {type: Boolean, reflect: true},
    disabled: {type: Boolean, reflect: true}
  };

  static override styles = css`
    :host
    {
      display: flex;
      --_track: var(--md-sys-color-surface-container-highest);
      --_outline: var(--md-sys-color-outline);
      --_handle: var(--md-sys-color-outline);
      --_handle-size: 16px;
      --_label: var(--md-sys-color-on-surface);
    }

    :host([selected])
    {
      --_track: var(--md-sys-color-primary);
      --_outline: var(--md-sys-color-primary);
      --_handle: var(--md-sys-color-on-primary);
      --_handle-size: 24px;
    }

    :host([disabled])
    {
      --_track: rgb(from var(--md-sys-color-surface-container-highest)
                    r g b / 0.12);
      --_outline: rgb(from var(--md-sys-color-on-surface) r g b / 0.12);
      --_handle: rgb(from var(--md-sys-color-on-surface) r g b / 0.38);
      --_label: rgb(from var(--md-sys-color-on-surface) r g b / 0.38);
    }

    :host([disabled][selected])
    {
      --_track: rgb(from var(--md-sys-color-on-surface) r g b / 0.12);
      --_outline: transparent;
      --_handle: var(--md-sys-color-surface);
    }

    label
    {
      display: flex;
      flex: 1;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
      color: var(--_label);
      cursor: pointer;
    }

    :host([disabled]) label
    {
      cursor: default;
    }

    .track
    {
      position: relative;
      display: inline-flex;
      flex: none;
      align-items: center;
      box-sizing: border-box;
      width: 52px;
      height: 32px;
      border: 2px solid var(--_outline);
      border-radius: var(--md-sys-shape-corner-full);
      background: var(--_track);
      transition-property: background-color, border-color;
      transition-duration:
        var(--md-sys-motion-spring-fast-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-effects-easing);
    }

    /*
     * The native checkbox is invisible and covers the track, so it receives
     * the input.
     */
    input
    {
      position: absolute;
      inset: -2px;
      z-index: 1;
      margin: 0;
      opacity: 0;
      cursor: inherit;
    }

    /*
     * The handle is centered in a 28dp slot that sits at the start of the
     * track or, while selected, at its end.
     */
    .handle
    {
      position: absolute;
      inset-block-start: 0;
      inset-inline-start: 0;
      display: flex;
      align-items: center;
      justify-content: center;
      width: 28px;
      height: 28px;
      pointer-events: none;
      transition-property: transform;
      transition-duration:
        var(--md-sys-motion-spring-fast-spatial-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-spatial-easing);
    }

    :host([selected]) .handle
    {
      transform: translateX(20px);
    }

    :host(:dir(rtl)[selected]) .handle
    {
      transform: translateX(-20px);
    }

    /*
     * The state layer is a 40dp circle around the handle.
     */
    .handle::before
    {
      content: '';
      position: absolute;
      inset: -6px;
      border-radius: var(--md-sys-shape-corner-full);
      background: var(--md-sys-color-on-surface);
      opacity: 0;
      transition-property: opacity;
      transition-duration:
        var(--md-sys-motion-spring-fast-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-effects-easing);
    }

    :host([selected]) .handle::before
    {
      background: var(--md-sys-color-primary);
    }

    :host(:not([disabled])) label:hover .handle::before
    {
      opacity: var(--md-sys-state-hover-state-layer-opacity);
    }

    .knob
    {
      width: var(--_handle-size);
      height: var(--_handle-size);
      border-radius: var(--md-sys-shape-corner-full);
      background: var(--_handle);
      transition-property: width, height, background-color;
      transition-duration:
        var(--md-sys-motion-spring-fast-spatial-duration),
        var(--md-sys-motion-spring-fast-spatial-duration),
        var(--md-sys-motion-spring-fast-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-spatial-easing),
        var(--md-sys-motion-spring-fast-spatial-easing),
        var(--md-sys-motion-spring-fast-effects-easing);
    }

    :host(:not([disabled])) label:active .knob
    {
      width: 28px;
      height: 28px;
    }

    :host(:not([disabled])) label:hover .knob
    {
      --_handle: var(--md-sys-color-on-surface-variant);
    }

    :host([selected]:not([disabled])) label:hover .knob
    {
      --_handle: var(--md-sys-color-primary-container);
    }
  `;

  declare selected: boolean;
  declare disabled: boolean;

  constructor ()
  {
    super ();
    this.selected = false;
    this.disabled = false;
  }

  override render (): TemplateResult
  {
    return html`
      <label>
        <slot></slot>
        <span class="track">
          <m3-focus-ring></m3-focus-ring>
          <input type="checkbox" role="switch"
                 .checked=${live (this.selected)}
                 ?disabled=${this.disabled}
                 @change=${this.on_change}>
          <span class="handle"><span class="knob"></span></span>
        </span>
      </label>`;
  }

  // Attach the focus ring to the native checkbox, so that the ring goes
  // around the track while the checkbox has keyboard focus.
  //
  protected override firstUpdated (): void
  {
    const i = this.renderRoot.querySelector ('input');

    if (i !== null)
      this.renderRoot.querySelector ('m3-focus-ring')?.attach (i);
  }

  // Update selected and dispatch our own change event since the native one
  // doesn't leave the shadow tree (it's not composed).
  //
  private on_change (e: Event): void
  {
    this.selected = (e.currentTarget as HTMLInputElement).checked;
    this.dispatchEvent (new Event ('change', {bubbles: true}));
  }
}

customElements.define ('m3-switch', switch_);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-switch': switch_;
  }
}
