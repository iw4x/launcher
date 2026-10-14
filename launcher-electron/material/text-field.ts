// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement, nothing } from 'lit';
import { live } from 'lit/directives/live.js';

import type { TemplateResult } from 'lit';

import './icon';

// Text field where the user enters or edits text. For example:
//
// <m3-text-field label="Name" supporting="As other players see it">
// </m3-text-field>
//
// The variant is filled (the default) or outlined. The label names the field
// and sits inside it until the field has focus or a value, at which point it
// floats to the top edge. A field without a label shows its placeholder in
// place of one and is shorter (as suits a search field): 56dp adjusted by
// the density offset (see tokens/density.css), which is 48dp by default.
//
// The icon, if any, names a leading Material Symbols icon. The end slot holds
// trailing controls (a button that clears the field, say).
//
// The supporting text goes under the field. If the error text is not empty,
// then it replaces the supporting text and puts the field into the error
// state.
//
// The value follows the user's input. The input events of the native input
// reach the page as the user types and the field dispatches the change event
// when the user commits the value. The type is text (the default), search,
// email, or password.
//
export class text_field extends LitElement
{
  static override shadowRootOptions =
    {...LitElement.shadowRootOptions, delegatesFocus: true};

  static override properties =
  {
    variant:      {type: String, reflect: true},
    type:         {type: String},
    label:        {type: String},
    value:        {type: String},
    placeholder:  {type: String},
    icon:         {type: String},
    supporting:   {type: String},
    error:        {type: String},
    maxlength:    {type: Number},
    autocomplete: {type: String},
    required:     {type: Boolean, reflect: true},
    disabled:     {type: Boolean, reflect: true},
    end_:         {state: true}
  };

  static override styles = css`
    :host
    {
      display: inline-flex;
      flex-direction: column;
      min-width: 200px;
      --_height: 56px;
      --_container: var(--md-sys-color-surface-container-highest);
      --_indicator: var(--md-sys-color-on-surface-variant);
      --_label: var(--md-sys-color-on-surface-variant);
      --_caret: var(--md-sys-color-primary);
    }

    :host(:focus-within)
    {
      --_indicator: var(--md-sys-color-primary);
      --_label: var(--md-sys-color-primary);
    }

    :host([variant='outlined'])
    {
      --_container: transparent;
      --_indicator: var(--md-sys-color-outline);
    }

    :host([variant='outlined']:focus-within)
    {
      --_indicator: var(--md-sys-color-primary);
    }

    /*
     * Hovering an unfocused field gives its active indicator (or outline) the
     * on-surface role (on-error-container in the error state).
     */
    :host(:not([disabled]):not(:focus-within)) .field:hover
    {
      --_indicator: var(--md-sys-color-on-surface);
    }

    :host(:not([disabled]):not(:focus-within)) .field.error:hover
    {
      --_indicator: var(--md-sys-color-on-error-container);
    }

    :host([disabled])
    {
      --_container: rgb(from var(--md-sys-color-on-surface) r g b / 0.04);
      --_indicator: rgb(from var(--md-sys-color-on-surface) r g b / 0.38);
      --_label: rgb(from var(--md-sys-color-on-surface) r g b / 0.38);
    }

    :host([variant='outlined'][disabled])
    {
      --_container: transparent;
      --_indicator: rgb(from var(--md-sys-color-on-surface) r g b / 0.12);
    }

    .field
    {
      position: relative;
      display: flex;
      align-items: center;
      gap: 16px;
      box-sizing: border-box;
      height: var(--_height);
      padding-inline: 16px;
      border-start-start-radius: var(--md-sys-shape-corner-extra-small);
      border-start-end-radius: var(--md-sys-shape-corner-extra-small);
      background: var(--_container);
      color: var(--md-sys-color-on-surface);
      cursor: text;
    }

    .field.bare
    {
      --_height: calc(56px + var(--m3-density-offset, 0px));
    }

    .field.error
    {
      --_indicator: var(--md-sys-color-error);
      --_label: var(--md-sys-color-error);
      --_caret: var(--md-sys-color-error);
    }

    :host([variant='outlined']) .field
    {
      border-radius: var(--md-sys-shape-corner-extra-small);
    }

    /*
     * The active indicator of the filled field is a line along its bottom
     * edge, which is 1dp thick at rest and 2dp thick while the field has
     * focus.
     */
    :host(:not([variant='outlined'])) .field::after
    {
      content: '';
      position: absolute;
      inset-inline: 0;
      inset-block-end: 0;
      height: 1px;
      background: var(--_indicator);
    }

    :host(:not([variant='outlined']):focus-within) .field::after
    {
      height: 2px;
    }

    :host(:not([variant='outlined']):not([disabled])) .field:hover::before
    {
      content: '';
      position: absolute;
      inset: 0;
      border-radius: inherit;
      background: var(--md-sys-color-on-surface);
      opacity: var(--md-sys-state-hover-state-layer-opacity);
      pointer-events: none;
    }

    /*
     * The outline of the outlined field is a fieldset. Its legend is as wide
     * as the floating label and cuts the notch that the label sits in.
     */
    fieldset
    {
      position: absolute;
      inset: -5px 0 0;
      margin: 0;
      padding: 0 12px;
      border: 1px solid var(--_indicator);
      border-radius: inherit;
      pointer-events: none;
    }

    :host(:focus-within) fieldset,
    .error fieldset
    {
      border-width: 2px;
      padding-inline: 11px;
    }

    legend
    {
      max-width: 0;
      height: 11px;
      padding: 0;
      visibility: hidden;
      white-space: nowrap;
      font-family: var(--md-sys-typescale-body-small-font);
      font-size: var(--md-sys-typescale-body-small-size);
      transition-property: max-width;
      transition-duration:
        var(--md-sys-motion-spring-fast-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-effects-easing);
    }

    legend span
    {
      padding-inline: 4px;
    }

    .floating legend,
    :host(:focus-within) .labeled legend
    {
      max-width: 100%;
    }

    .input
    {
      position: relative;
      display: flex;
      flex: 1;
      align-self: stretch;
      min-width: 0;
    }

    input
    {
      flex: 1;
      min-width: 0;
      margin: 0;
      padding: 0;
      border: none;
      outline: none;
      background: none;
      color: inherit;
      caret-color: var(--_caret);
      font-family: var(--md-sys-typescale-body-large-font);
      font-size: var(--md-sys-typescale-body-large-size);
      line-height: var(--md-sys-typescale-body-large-line-height);
      font-weight: var(--md-sys-typescale-body-large-weight);
      letter-spacing: var(--md-sys-typescale-body-large-tracking);
    }

    input::placeholder
    {
      color: var(--md-sys-color-on-surface-variant);
    }

    input::-webkit-search-cancel-button
    {
      display: none;
    }

    :host([disabled]) input
    {
      color: rgb(from var(--md-sys-color-on-surface) r g b / 0.38);
    }

    /*
     * At rest, the label is centered vertically. When floating, the label of
     * the filled field sits at the top with the input under it and the label
     * of the outlined field sits on the outline.
     *
     * The label keeps its body large layout and floats by transform alone,
     * scaling to body small (12 of 16dp), so that its position and size
     * change together with the fast spatial spring. The offsets place the
     * center of the scaled 24dp line box 16dp from the top for the filled
     * field (7dp plus 9dp, which is half of the scaled box) and on the
     * outline for the outlined field.
     */
    :host(:not([variant='outlined'])) .labeled input
    {
      padding-block-start: 16px;
    }

    label
    {
      position: absolute;
      inset-inline-start: 0;
      top: 0;
      max-width: 100%;
      overflow: hidden;
      color: var(--_label);
      text-overflow: ellipsis;
      white-space: nowrap;
      pointer-events: none;
      transform: translateY(calc((var(--_height) - 24px) / 2));
      transform-origin: top left;
      font-family: var(--md-sys-typescale-body-large-font);
      font-size: var(--md-sys-typescale-body-large-size);
      line-height: var(--md-sys-typescale-body-large-line-height);
      letter-spacing: var(--md-sys-typescale-body-large-tracking);
      transition-property: transform;
      transition-duration:
        var(--md-sys-motion-spring-fast-spatial-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-spatial-easing);
    }

    .floating label,
    :host(:focus-within) .labeled label
    {
      max-width: calc(100% / 0.75);
      transform: translateY(7px) scale(0.75);
    }

    :host([variant='outlined']) .floating label,
    :host([variant='outlined']:focus-within) .labeled label
    {
      transform: translateY(-9px) scale(0.75);
    }

    /*
     * With a leading icon, the label of the outlined field floats back over
     * the icon and the gap after it (40dp) into the notch at the start of the
     * outline.
     */
    :host([variant='outlined']) .icon.floating label,
    :host([variant='outlined']:focus-within) .icon.labeled label
    {
      transform: translate(-40px, -9px) scale(0.75);
    }

    :host([variant='outlined']:dir(rtl)) .icon.floating label,
    :host([variant='outlined']:dir(rtl):focus-within) .icon.labeled label
    {
      transform: translate(40px, -9px) scale(0.75);
    }

    m3-icon
    {
      flex: none;
      color: var(--md-sys-color-on-surface-variant);
    }

    .end
    {
      display: flex;
      flex: none;
      align-items: center;
      margin-inline-end: -12px;
      color: var(--md-sys-color-on-surface-variant);
    }

    .end:not(.filled)
    {
      display: none;
    }

    .supporting
    {
      padding: 4px 16px 0;
      color: var(--md-sys-color-on-surface-variant);
      font-family: var(--md-sys-typescale-body-small-font);
      font-size: var(--md-sys-typescale-body-small-size);
      line-height: var(--md-sys-typescale-body-small-line-height);
      font-weight: var(--md-sys-typescale-body-small-weight);
      letter-spacing: var(--md-sys-typescale-body-small-tracking);
    }

    .supporting.error
    {
      color: var(--md-sys-color-error);
    }
  `;

  declare variant: 'filled' | 'outlined';
  declare type: 'text' | 'search' | 'email' | 'password';
  declare label: string;
  declare value: string;
  declare placeholder: string;
  declare icon: string;
  declare supporting: string;
  declare error: string;
  declare maxlength: number | null;
  declare autocomplete: string;
  declare required: boolean;
  declare disabled: boolean;
  declare private end_: boolean;

  constructor ()
  {
    super ();
    this.variant = 'filled';
    this.type = 'text';
    this.label = '';
    this.value = '';
    this.placeholder = '';
    this.icon = '';
    this.supporting = '';
    this.error = '';
    this.maxlength = null;
    this.autocomplete = '';
    this.required = false;
    this.disabled = false;
    this.end_ = false;
  }

  // Select the field's text.
  //
  select (): void
  {
    this.renderRoot.querySelector ('input')?.select ();
  }

  override render (): TemplateResult
  {
    const labeled = this.label !== '';
    const error = this.error !== '';
    const below = error ? this.error : this.supporting;

    // The label floats while the field has a value or shows its placeholder
    // as well as while it has focus (see the styles).
    //
    const floating = labeled && (this.value !== '' || this.placeholder !== '');

    const c = ['field',
               labeled ? 'labeled' : 'bare',
               floating ? 'floating' : '',
               this.icon !== '' ? 'icon' : '',
               error ? 'error' : ''].join (' ');

    // Return the attribute value or nothing (no attribute) if it is empty.
    //
    const opt = (v: string): string | typeof nothing =>
      v !== '' ? v : nothing;

    return html`
      <div class=${c} @click=${this.on_click}>
        ${this.variant === 'outlined'
          ? html`
            <fieldset aria-hidden="true">
              <legend>
                ${labeled ? html`<span>${this.label}</span>` : nothing}
              </legend>
            </fieldset>`
          : nothing}
        ${this.icon !== '' ? html`<m3-icon>${this.icon}</m3-icon>` : nothing}
        <span class="input ${labeled ? 'labeled' : ''}">
          ${labeled ? html`<label for="input">${this.label}</label>` : nothing}
          <input id="input"
                 type=${this.type}
                 .value=${live (this.value)}
                 placeholder=${opt (this.placeholder)}
                 aria-label=${labeled ? nothing : opt (this.placeholder)}
                 aria-invalid=${error ? 'true' : nothing}
                 aria-describedby=${below !== '' ? 'supporting' : nothing}
                 maxlength=${this.maxlength ?? nothing}
                 autocomplete=${opt (this.autocomplete)}
                 ?required=${this.required}
                 ?disabled=${this.disabled}
                 @input=${this.on_input}
                 @change=${this.on_change}>
        </span>
        <span class=${this.end_ ? 'end filled' : 'end'}>
          <slot name="end" @slotchange=${this.on_slot}></slot>
        </span>
      </div>
      ${below !== ''
        ? html`
          <div id="supporting"
               class=${error ? 'supporting error' : 'supporting'}>
            ${below}
          </div>`
        : nothing}`;
  }

  // Focus the input when the user clicks anywhere on the field except on an
  // element in the end slot.
  //
  private on_click (e: Event): void
  {
    if (!(e.target instanceof HTMLElement) || e.target.slot !== 'end')
      this.renderRoot.querySelector ('input')?.focus ();
  }

  private on_input (e: Event): void
  {
    this.value = (e.currentTarget as HTMLInputElement).value;
  }

  // Dispatch our own change event since the native one doesn't leave the
  // shadow tree (it's not composed).
  //
  private on_change (): void
  {
    this.dispatchEvent (new Event ('change', {bubbles: true}));
  }

  // Hide the end slot while it is empty.
  //
  private on_slot (e: Event): void
  {
    const s = e.currentTarget as HTMLSlotElement;
    this.end_ = s.assignedNodes ({flatten: true}).length !== 0;
  }
}

customElements.define ('m3-text-field', text_field);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-text-field': text_field;
  }
}
