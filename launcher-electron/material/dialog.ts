// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement } from 'lit';

import type { PropertyValues, TemplateResult } from 'lit';

// Basic dialog that asks the user to decide or enter something before they
// can continue. For example:
//
// <m3-dialog headline="Add friend">
//   <m3-text-field label="Name"></m3-text-field>
//   <m3-button slot="actions" variant="text">Cancel</m3-button>
//   <m3-button slot="actions" variant="text">Add</m3-button>
// </m3-dialog>
//
// The headline names the dialog. The content goes into the default slot and
// the buttons into the actions slot, which aligns them to the end.
//
// Setting open (or calling show()) opens the dialog over a scrim and makes
// the rest of the window inert. The first element with the autofocus
// attribute, if any, receives the focus. The dialog closes with close() or
// when the user presses Escape or clicks the scrim.
//
// Note that the buttons are in the page's tree, out of reach of a dialog
// form (<form method="dialog">), so their click handlers call close() with
// the value of their choice. Once closed, the dialog dispatches the close
// event with this value in return_value, which is empty if the user
// dismissed the dialog.
//
export class dialog extends LitElement
{
  static override properties =
  {
    headline: {type: String},
    open:     {type: Boolean, reflect: true}
  };

  static override styles = css`
    :host
    {
      display: contents;
    }

    /*
     * The dialog grows from 90% of its size with the default spatial spring
     * while it and the scrim fade in with the default effects spring.
     * Closing plays this in reverse.
     *
     * Note that the display stays flex until the exit ends (allow-discrete),
     * so we give the closed state the column layout as well.
     */
    dialog
    {
      box-sizing: border-box;
      min-width: 280px;
      max-width: min(560px, calc(100vw - 48px));
      max-height: calc(100vh - 48px);
      margin: auto;
      padding: 24px;
      border: none;
      border-radius: var(--md-sys-shape-corner-extra-large);
      background: var(--md-sys-color-surface-container-high);
      box-shadow: var(--m3-elevation-shadow-level3);
      color: var(--md-sys-color-on-surface-variant);
      flex-direction: column;
      gap: 16px;
      opacity: 0;
      transform: scale(0.9);
      transition-property: opacity, transform, overlay, display;
      transition-behavior: allow-discrete;
      transition-duration:
        var(--md-sys-motion-spring-default-effects-duration),
        var(--md-sys-motion-spring-default-spatial-duration),
        var(--md-sys-motion-spring-default-effects-duration),
        var(--md-sys-motion-spring-default-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-effects-easing),
        var(--md-sys-motion-spring-default-spatial-easing),
        linear,
        linear;
    }

    dialog[open]
    {
      display: flex;
      opacity: 1;
      transform: none;
    }

    @starting-style
    {
      dialog[open]
      {
        opacity: 0;
        transform: scale(0.9);
      }
    }

    dialog::backdrop
    {
      background: rgb(from var(--md-sys-color-scrim) r g b / 0);
      transition-property: background-color, overlay, display;
      transition-behavior: allow-discrete;
      transition-duration:
        var(--md-sys-motion-spring-default-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-effects-easing);
    }

    dialog[open]::backdrop
    {
      background: rgb(from var(--md-sys-color-scrim) r g b / 0.32);
    }

    @starting-style
    {
      dialog[open]::backdrop
      {
        background: rgb(from var(--md-sys-color-scrim) r g b / 0);
      }
    }

    h2
    {
      margin: 0;
      color: var(--md-sys-color-on-surface);
      font-family: var(--md-sys-typescale-headline-small-font);
      font-size: var(--md-sys-typescale-headline-small-size);
      line-height: var(--md-sys-typescale-headline-small-line-height);
      font-weight: var(--md-sys-typescale-headline-small-weight);
      letter-spacing: var(--md-sys-typescale-headline-small-tracking);
    }

    .content
    {
      display: flex;
      flex-direction: column;
      gap: 16px;
      min-height: 0;
      overflow: auto;
      font-family: var(--md-sys-typescale-body-medium-font);
      font-size: var(--md-sys-typescale-body-medium-size);
      line-height: var(--md-sys-typescale-body-medium-line-height);
      font-weight: var(--md-sys-typescale-body-medium-weight);
      letter-spacing: var(--md-sys-typescale-body-medium-tracking);
    }

    .actions
    {
      display: flex;
      justify-content: flex-end;
      gap: 8px;
      padding-block-start: 8px;
    }
  `;

  declare headline: string;
  declare open: boolean;

  // Value of the choice that closed the dialog (see close()).
  //
  return_value = '';

  constructor ()
  {
    super ();
    this.headline = '';
    this.open = false;
  }

  // Open the dialog.
  //
  show (): void
  {
    this.open = true;
  }

  // Close the dialog with the value of the user's choice.
  //
  close (value = ''): void
  {
    this.return_value = value;
    this.open = false;
  }

  override render (): TemplateResult
  {
    return html`
      <dialog aria-labelledby="headline"
              @cancel=${this.on_cancel}
              @click=${this.on_click}
              @close=${this.on_close}>
        <h2 id="headline">${this.headline}</h2>
        <div class="content"><slot></slot></div>
        <div class="actions"><slot name="actions"></slot></div>
      </dialog>`;
  }

  // Open or close the native dialog according to the open property. Note
  // that if the native dialog closed by itself (see on_close()), then it is
  // already closed by the time open becomes false.
  //
  protected override updated (changed: PropertyValues<this>): void
  {
    if (!changed.has ('open'))
      return;

    const d = this.renderRoot.querySelector ('dialog');

    if (d === null)
      return;

    if (this.open && !d.open)
    {
      this.return_value = '';
      d.showModal ();
    }
    else if (!this.open && d.open)
      d.close ();
  }

  // Close the dialog on Escape (which the native dialog reports with the
  // cancel event) through close(), so that open and return_value stay in
  // step.
  //
  private on_cancel (e: Event): void
  {
    e.preventDefault ();
    this.close ();
  }

  // Close the dialog on a click on the scrim. Such a click targets the
  // dialog element itself and lands outside of its box.
  //
  private on_click (e: MouseEvent): void
  {
    const d = e.currentTarget as HTMLDialogElement;

    if (e.target !== d)
      return;

    const r = d.getBoundingClientRect ();

    if (e.clientX < r.left || e.clientX > r.right ||
        e.clientY < r.top || e.clientY > r.bottom)
      this.close ();
  }

  // Update open and dispatch our own close event since the native one
  // doesn't leave the shadow tree (it's not composed).
  //
  private on_close (): void
  {
    this.open = false;
    this.dispatchEvent (new Event ('close'));
  }
}

customElements.define ('m3-dialog', dialog);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-dialog': dialog;
  }
}
