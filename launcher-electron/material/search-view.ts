// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement, nothing } from 'lit';
import { live } from 'lit/directives/live.js';

import type { PropertyValues, TemplateResult } from 'lit';

import './divider';
import './icon-button';

// Docked search view where the user enters a query and sees what matches
// it. For example:
//
// <m3-search-view placeholder="Search">
//   ...results...
// </m3-search-view>
//
// The view floats at the top of the window over a scrim. It is 60% of the
// window wide (between 360 and 720dp) and at most two thirds of the window
// high. The header holds the back button, the query, and (while there is a
// query) a button that clears it. The default slot holds what goes under the
// header (filters and results, say), which scrolls.
//
// Setting open (or calling show()) opens the view with the focus on the
// query and makes the rest of the window inert. The view closes with
// close(), the back button, Escape, or a click on the scrim, and then
// dispatches the close event. Note that if the query is not empty, then the
// first Escape clears it (as in any search field).
//
// The value follows the user's input and the input events of the query
// reach the page as the user types.
//
export class search_view extends LitElement
{
  static override properties =
  {
    value:       {type: String},
    placeholder: {type: String},
    open:        {type: Boolean, reflect: true}
  };

  static override styles = css`
    :host
    {
      display: contents;
    }

    /*
     * The view grows down from its header with the default spatial spring
     * while it and the scrim fade in with the default effects spring.
     * Closing plays this in reverse.
     *
     * Note that the display stays flex until the exit ends (allow-discrete),
     * so we give the closed state the column layout as well.
     */
    dialog
    {
      box-sizing: border-box;
      width: clamp(360px, 60vw, 720px);
      max-width: calc(100vw - 48px);
      max-height: calc(200vh / 3);
      margin: 24px auto auto;
      padding: 0;
      border: none;
      border-radius: var(--md-sys-shape-corner-extra-large);
      background: var(--md-sys-color-surface-container-high);
      box-shadow: var(--m3-elevation-shadow-level3);
      color: var(--md-sys-color-on-surface);
      flex-direction: column;
      opacity: 0;
      clip-path: inset(0 0 calc(100% - 56px) 0 round
                      var(--md-sys-shape-corner-extra-large));
      transition-property: opacity, clip-path, overlay, display;
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
      clip-path: inset(0 0 0 0 round var(--md-sys-shape-corner-extra-large));
    }

    @starting-style
    {
      dialog[open]
      {
        opacity: 0;
        clip-path: inset(0 0 calc(100% - 56px) 0 round
                        var(--md-sys-shape-corner-extra-large));
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
      gap: 4px;
      height: 56px;
      padding-inline: 4px;
    }

    m3-divider
    {
      --m3-divider-color: var(--md-sys-color-outline);
    }

    input
    {
      flex: 1;
      min-width: 0;
      margin: 0;
      padding: 0 12px;
      border: none;
      outline: none;
      background: none;
      color: inherit;
      caret-color: var(--md-sys-color-primary);
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

    .content
    {
      flex: 1;
      min-height: 0;
      overflow: auto;
      scrollbar-width: thin;
      scrollbar-color: var(--md-sys-color-outline-variant) transparent;
    }
  `;

  declare value: string;
  declare placeholder: string;
  declare open: boolean;

  constructor ()
  {
    super ();
    this.value = '';
    this.placeholder = '';
    this.open = false;
  }

  // Open the view.
  //
  show (): void
  {
    this.open = true;
  }

  // Close the view.
  //
  close (): void
  {
    this.open = false;
  }

  override render (): TemplateResult
  {
    return html`
      <dialog aria-label=${this.placeholder}
              @cancel=${this.on_cancel}
              @click=${this.on_click}
              @close=${this.on_close}>
        <header>
          <m3-icon-button icon="arrow_back" label="Close search"
                          @click=${this.close}>
          </m3-icon-button>
          <input type="search"
                 .value=${live (this.value)}
                 placeholder=${this.placeholder}
                 aria-label=${this.placeholder}
                 autocomplete="off"
                 spellcheck="false"
                 @input=${this.on_input}>
          ${this.value !== ''
            ? html`
              <m3-icon-button icon="close" label="Clear search"
                              @click=${this.on_clear}>
              </m3-icon-button>`
            : nothing}
        </header>
        <m3-divider></m3-divider>
        <div class="content"><slot></slot></div>
      </dialog>`;
  }

  // Open or close the native dialog according to the open property, moving
  // the focus to the query on opening. Note that if the native dialog closed
  // by itself (see on_close()), then it is already closed by the time open
  // becomes false.
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
      this.renderRoot.querySelector ('input')?.focus ();
    }
    else if (!this.open && d.open)
      d.close ();
  }

  private on_input (e: Event): void
  {
    this.value = (e.currentTarget as HTMLInputElement).value;
  }

  // Clear the query and dispatch the input event (the same as if the user
  // deleted the query), then return the focus to the query.
  //
  private on_clear (): void
  {
    this.value = '';
    this.dispatchEvent (new Event ('input', {bubbles: true}));
    this.renderRoot.querySelector ('input')?.focus ();
  }

  // Close the view on Escape (which the native dialog reports with the
  // cancel event) through close(), so that open stays in step.
  //
  private on_cancel (e: Event): void
  {
    e.preventDefault ();
    this.close ();
  }

  // Close the view on a click on the scrim. Such a click targets the dialog
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
}

customElements.define ('m3-search-view', search_view);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-search-view': search_view;
  }
}
