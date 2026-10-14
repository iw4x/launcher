// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement } from 'lit';

import type { TemplateResult } from 'lit';

// State layer of an interactive element, which shows while a pointer hovers
// the element and spreads from the point of a press.
//
// The layer covers its containing block (usually its parent with a position)
// and takes its parent's corners. It paints in the current text color (the
// element's content color) at the opacities of the state tokens. Similar to
// the focus ring, it watches its parent element unless the component
// attaches it to another control (see attach()).
//
// A press spreads from the pointer (or from the center for a keyboard press)
// with the default spatial spring. It fades in with the fast effects spring
// and out with the default effects spring. Note that a disabled control
// shows no state.
//
export class ripple extends LitElement
{
  static override properties =
  {
    hovered: {type: Boolean, reflect: true},
    pressed: {type: Boolean, reflect: true}
  };

  static override styles = css`
    :host
    {
      position: absolute;
      inset: 0;
      border-radius: inherit;
      overflow: hidden;
      pointer-events: none;
    }

    .hover,
    .press
    {
      position: absolute;
      background: currentColor;
      opacity: 0;
      transition-property: opacity;
      transition-duration: var(--md-sys-motion-spring-default-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-effects-easing);
    }

    .hover
    {
      inset: 0;
    }

    :host([hovered]) .hover
    {
      opacity: var(--md-sys-state-hover-state-layer-opacity);
    }

    /*
     * The press is a circle that press() sizes and positions.
     */
    .press
    {
      border-radius: 50%;
      left: var(--_x, 0);
      top: var(--_y, 0);
      width: var(--_size, 0);
      height: var(--_size, 0);
    }

    :host([pressed]) .press
    {
      opacity: var(--md-sys-state-pressed-state-layer-opacity);
      transition-duration: var(--md-sys-motion-spring-fast-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-effects-easing);
    }
  `;

  declare hovered: boolean;
  declare pressed: boolean;

  // Control that the component attached the layer to, if any, and control
  // whose events the layer currently watches.
  //
  private attached_: HTMLElement | null = null;
  private control_: HTMLElement | null = null;

  private readonly on_pointerenter = (e: PointerEvent): void =>
  {
    if (e.pointerType !== 'touch' && this.enabled ())
      this.hovered = true;
  };

  private readonly on_pointerleave = (): void =>
  {
    this.hovered = false;
    this.pressed = false;
  };

  private readonly on_pointerdown = (e: PointerEvent): void =>
  {
    if (e.button === 0 && this.enabled ())
      this.press (e.clientX, e.clientY);
  };

  private readonly on_pointerup = (): void =>
  {
    this.pressed = false;
  };

  // A click without a pointer press (detail is 0) comes from the keyboard (or
  // a script), so we show a press from the center that ends with the next
  // frame.
  //
  private readonly on_click = (e: MouseEvent): void =>
  {
    if (e.detail !== 0 || !this.enabled ())
      return;

    const r = this.getBoundingClientRect ();

    this.press (r.left + r.width / 2, r.top + r.height / 2);
    requestAnimationFrame (() => {this.pressed = false;});
  };

  constructor ()
  {
    super ();
    this.hovered = false;
    this.pressed = false;
  }

  // Watch the control for pointer and keyboard presses in place of the
  // parent element.
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

  override render (): TemplateResult
  {
    return html`<div class="hover"></div><div class="press"></div>`;
  }

  // Move the listeners from the current control to c (if not null) and reset
  // the state.
  //
  private watch (c: HTMLElement | null): void
  {
    const es =
    [
      ['pointerenter',  this.on_pointerenter],
      ['pointerleave',  this.on_pointerleave],
      ['pointerdown',   this.on_pointerdown],
      ['pointerup',     this.on_pointerup],
      ['pointercancel', this.on_pointerup],
      ['click',         this.on_click]
    ] as const;

    if (this.control_ !== null)
    {
      for (const [n, f] of es)
        this.control_.removeEventListener (n, f as EventListener);
    }

    this.control_ = c;
    this.hovered = false;
    this.pressed = false;

    if (c !== null)
    {
      for (const [n, f] of es)
        c.addEventListener (n, f as EventListener);
    }
  }

  // Return true if there is a control and it is not disabled.
  //
  private enabled (): boolean
  {
    return this.control_ !== null && !this.control_.matches (':disabled');
  }

  // Start a press at the point, in client coordinates.
  //
  // The circle grows from a fifth of its size until it reaches the corner of
  // the layer that is farthest from the point. We use the Web Animations API
  // to restart the growth on every press, reading the spring's duration and
  // easing from the tokens.
  //
  private press (x: number, y: number): void
  {
    const p = this.renderRoot.querySelector<HTMLElement> ('.press');

    if (p === null)
      return;

    // Center the circle on the point and size it to reach the farthest
    // corner.
    //
    const r = this.getBoundingClientRect ();
    const dx = Math.max (x - r.left, r.right - x);
    const dy = Math.max (y - r.top, r.bottom - y);
    const size = 2 * Math.hypot (dx, dy);

    p.style.setProperty ('--_x', `${x - r.left - size / 2}px`);
    p.style.setProperty ('--_y', `${y - r.top - size / 2}px`);
    p.style.setProperty ('--_size', `${size}px`);

    // Restart the growth, dropping whatever is left of the previous press.
    // Note that the fade of the previous release is among these animations
    // and that setting pressed below starts a new one.
    //
    for (const a of p.getAnimations ())
      a.cancel ();

    const cs = getComputedStyle (this);
    const t = '--md-sys-motion-spring-default-spatial';

    p.animate ([{transform: 'scale(0.2)'}, {transform: 'scale(1)'}],
               {
                 duration: parseFloat (cs.getPropertyValue (`${t}-duration`)),
                 easing: cs.getPropertyValue (`${t}-easing`).trim (),
                 fill: 'forwards'
               });

    this.pressed = true;
  }
}

customElements.define ('m3-ripple', ripple);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-ripple': ripple;
  }
}
