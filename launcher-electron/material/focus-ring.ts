// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, LitElement } from 'lit';

// Focus ring of an interactive element, which shows while the element has
// keyboard focus, that is, while it matches :focus-visible. Note that the
// element keeps matching :focus-visible if the user switches from the
// keyboard to the pointer, so a pointer press hides the ring until the next
// focus.
//
// The ring covers its containing block (usually its parent with a position)
// and takes its parent's corners. It is a 3dp line in the secondary role, 2dp
// outside of the block.
//
// The ring watches its parent element for focus unless the component
// attaches it to another control (see attach()). This way a component can
// draw the ring around one part while another part receives the focus.
//
export class focus_ring extends LitElement
{
  static override properties = {visible: {type: Boolean, reflect: true}};

  static override styles = css`
    :host
    {
      position: absolute;
      inset: 0;
      border-radius: inherit;
      pointer-events: none;
      display: none;
    }

    :host([visible])
    {
      display: block;
      outline: 3px solid var(--md-sys-color-secondary);
      outline-offset: 2px;
    }
  `;

  declare visible: boolean;

  // Control that the component attached the ring to, if any, and control
  // whose focus the ring currently watches.
  //
  private attached_: HTMLElement | null = null;
  private control_: HTMLElement | null = null;

  // Show the ring if the control has keyboard focus.
  //
  private readonly on_focusin = (): void =>
  {
    this.visible = this.control_?.matches (':focus-visible') ?? false;
  };

  // Hide the ring without checking :focus-visible since the control still
  // matches it while the focus is leaving.
  //
  private readonly on_focusout = (): void =>
  {
    this.visible = false;
  };

  // Hide the ring on a pointer press (see above).
  //
  private readonly on_pointerdown = (): void =>
  {
    this.visible = false;
  };

  constructor ()
  {
    super ();
    this.visible = false;
  }

  // Watch the control for focus in place of the parent element.
  //
  attach (c: HTMLElement): void
  {
    this.attached_ = c;

    if (this.isConnected)
      this.watch (c);
  }

  override connectedCallback (): void
  {
    super.connectedCallback ();
    this.watch (this.attached_ ?? this.parentElement);
  }

  override disconnectedCallback (): void
  {
    super.disconnectedCallback ();
    this.watch (null);
  }

  // Move the listeners from the current control to c (if not null) and show
  // or hide the ring according to its focus.
  //
  private watch (c: HTMLElement | null): void
  {
    if (this.control_ !== null)
    {
      this.control_.removeEventListener ('focusin', this.on_focusin);
      this.control_.removeEventListener ('focusout', this.on_focusout);
      this.control_.removeEventListener ('pointerdown', this.on_pointerdown);
    }

    this.control_ = c;

    if (c !== null)
    {
      c.addEventListener ('focusin', this.on_focusin);
      c.addEventListener ('focusout', this.on_focusout);
      c.addEventListener ('pointerdown', this.on_pointerdown);
    }

    this.on_focusin ();
  }
}

customElements.define ('m3-focus-ring', focus_ring);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-focus-ring': focus_ring;
  }
}
