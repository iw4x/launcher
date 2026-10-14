// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement } from 'lit';

import type { PropertyValues, TemplateResult } from 'lit';

import './divider';
import './icon-button';

// Modal side sheet that shows details or secondary content along the end
// edge of the window. For example:
//
// <m3-side-sheet headline="Kestrel">
//   ...
//   <m3-button slot="actions">Join</m3-button>
// </m3-side-sheet>
//
// The headline names the sheet and sits in the header beside the close
// button. The content goes into the default slot and scrolls. The buttons go
// into the actions slot, which, if not empty, sits under a divider at the
// bottom.
//
// Setting open (or calling show()) slides the sheet in over a scrim and
// makes the rest of the window inert. The first element with the autofocus
// attribute receives the focus or, if there is none, the close button does.
// The sheet closes with close(), the close button, Escape, or a click on the
// scrim, and then dispatches the close event.
//
// The sheet is 400dp wide (unless --m3-side-sheet-width says otherwise) and
// leaves at least 56dp of the window uncovered.
//
export class side_sheet extends LitElement
{
  static override properties =
  {
    headline: {type: String},
    open:     {type: Boolean, reflect: true},
    actions_: {state: true}
  };

  static override styles = css`
    :host
    {
      display: contents;
    }

    /*
     * The sheet slides in from the end edge with the default spatial spring
     * while the scrim fades in with the default effects spring. Closing
     * plays this in reverse.
     *
     * Note that the display stays flex until the exit ends (allow-discrete),
     * so we give the closed state the column layout as well.
     */
    dialog
    {
      box-sizing: border-box;
      width: min(var(--m3-side-sheet-width, 400px), calc(100vw - 56px));
      max-width: none;
      height: 100vh;
      max-height: none;
      margin: 0;
      margin-inline-start: auto;
      padding: 0;
      border: none;
      border-start-start-radius: var(--md-sys-shape-corner-large);
      border-end-start-radius: var(--md-sys-shape-corner-large);
      background: var(--md-sys-color-surface-container-low);
      box-shadow: var(--m3-elevation-shadow-level1);
      color: var(--md-sys-color-on-surface-variant);
      flex-direction: column;
      transform: translateX(100%);
      transition-property: transform, overlay, display;
      transition-behavior: allow-discrete;
      transition-duration:
        var(--md-sys-motion-spring-default-spatial-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-spatial-easing),
        linear,
        linear;
    }

    :host(:dir(rtl)) dialog
    {
      transform: translateX(-100%);
    }

    dialog[open]
    {
      display: flex;
      transform: none;
    }

    @starting-style
    {
      dialog[open]
      {
        transform: translateX(100%);
      }

      :host(:dir(rtl)) dialog[open]
      {
        transform: translateX(-100%);
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

    header
    {
      display: flex;
      flex: none;
      align-items: center;
      gap: 12px;
      padding: 12px 12px 12px 24px;
    }

    h2
    {
      flex: 1;
      min-width: 0;
      margin: 0;
      overflow: hidden;
      color: var(--md-sys-color-on-surface);
      text-overflow: ellipsis;
      white-space: nowrap;
      font-family: var(--md-sys-typescale-title-large-font);
      font-size: var(--md-sys-typescale-title-large-size);
      line-height: var(--md-sys-typescale-title-large-line-height);
      font-weight: var(--md-sys-typescale-title-large-weight);
      letter-spacing: var(--md-sys-typescale-title-large-tracking);
    }

    .content
    {
      display: flex;
      flex: 1;
      flex-direction: column;
      min-height: 0;
      overflow: auto;
      padding-inline: 24px;
      padding-block-end: 24px;
    }

    footer
    {
      flex: none;
    }

    footer:not(.filled)
    {
      display: none;
    }

    .actions
    {
      display: flex;
      gap: 8px;
      padding: 16px 24px 24px;
    }
  `;

  declare headline: string;
  declare open: boolean;
  declare private actions_: boolean;

  constructor ()
  {
    super ();
    this.headline = '';
    this.open = false;
    this.actions_ = false;
  }

  // Open the sheet.
  //
  show (): void
  {
    this.open = true;
  }

  // Close the sheet.
  //
  close (): void
  {
    this.open = false;
  }

  override render (): TemplateResult
  {
    return html`
      <dialog aria-labelledby="headline"
              @cancel=${this.on_cancel}
              @click=${this.on_click}
              @close=${this.on_close}>
        <header>
          <h2 id="headline">${this.headline}</h2>
          <m3-icon-button icon="close" label="Close"
                          @click=${this.close}>
          </m3-icon-button>
        </header>
        <div class="content"><slot></slot></div>
        <footer class=${this.actions_ ? 'filled' : ''}>
          <m3-divider></m3-divider>
          <div class="actions">
            <slot name="actions" @slotchange=${this.on_slot}></slot>
          </div>
        </footer>
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
      d.showModal ();
    }
    else if (!this.open && d.open)
      d.close ();
  }

  // Close the sheet on Escape (which the native dialog reports with the
  // cancel event) through close(), so that open stays in step.
  //
  private on_cancel (e: Event): void
  {
    e.preventDefault ();
    this.close ();
  }

  // Close the sheet on a click on the scrim. Such a click targets the dialog
  // element itself and lands outside of its box.
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

  // Show the footer only if the actions slot has content.
  //
  private on_slot (e: Event): void
  {
    const s = e.currentTarget as HTMLSlotElement;
    this.actions_ = s.assignedNodes ({flatten: true}).length !== 0;
  }
}

customElements.define ('m3-side-sheet', side_sheet);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-side-sheet': side_sheet;
  }
}
