// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, LitElement } from 'lit';

import type { PropertyValues } from 'lit';

// Divider, a thin line that groups content in lists and containers. For
// example:
//
// <m3-divider inset="start"></m3-divider>
//
// The divider is a 1dp line in the outline variant role. It is horizontal
// unless vertical is set (for a row of items, say). The inset is none (the
// full length, the default), start (indented at the start), or middle
// (indented at both ends). The page can override the 16dp indent with
// --m3-divider-inset and the color with --m3-divider-color.
//
// To assistive technology the divider is a separator with the same
// orientation.
//
export class divider extends LitElement
{
  static override properties =
  {
    vertical: {type: Boolean, reflect: true},
    inset:    {type: String, reflect: true}
  };

  static override styles = css`
    :host
    {
      display: block;
      flex: none;
      height: 1px;
      background: var(--m3-divider-color,
                      var(--md-sys-color-outline-variant));
      --_inset: var(--m3-divider-inset, 16px);
    }

    :host([vertical])
    {
      align-self: stretch;
      width: 1px;
      height: auto;
    }

    :host([inset='start'])
    {
      margin-inline-start: var(--_inset);
    }

    :host([inset='middle'])
    {
      margin-inline: var(--_inset);
    }

    :host([vertical][inset='start'])
    {
      margin-inline: 0;
      margin-block-start: var(--_inset);
    }

    :host([vertical][inset='middle'])
    {
      margin-inline: 0;
      margin-block: var(--_inset);
    }
  `;

  declare vertical: boolean;
  declare inset: 'none' | 'start' | 'middle';

  constructor ()
  {
    super ();
    this.vertical = false;
    this.inset = 'none';
  }

  override connectedCallback (): void
  {
    super.connectedCallback ();
    this.setAttribute ('role', 'separator');
  }

  // Note that a separator is horizontal by default, so we only specify the
  // vertical orientation.
  //
  protected override updated (changed: PropertyValues<this>): void
  {
    if (changed.has ('vertical'))
    {
      if (this.vertical)
        this.setAttribute ('aria-orientation', 'vertical');
      else
        this.removeAttribute ('aria-orientation');
    }
  }
}

customElements.define ('m3-divider', divider);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-divider': divider;
  }
}
