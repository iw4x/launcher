// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement } from 'lit';

import type { TemplateResult } from 'lit';

// Icon from Material Symbols Rounded, which the library bundles (see
// index.ts). The text content names the icon, for example:
//
// <m3-icon>home</m3-icon>
//
// The font draws the name as the icon through a ligature. If filled is set,
// then the icon is drawn filled, which is how components show the selected
// state. The size is 24dp unless --m3-icon-size says otherwise and the color
// is the current text color.
//
// Unless the page sets aria-hidden itself, the icon is hidden from assistive
// technology since the control that shows it carries the accessible name.
//
export class icon extends LitElement
{
  static override properties = {filled: {type: Boolean, reflect: true}};

  static override styles = css`
    :host
    {
      display: inline-block;
      flex: none;
      width: var(--m3-icon-size, 24px);
      height: var(--m3-icon-size, 24px);
      overflow: hidden;

      font-family: 'Material Symbols Rounded';
      font-size: var(--m3-icon-size, 24px);
      font-weight: 400;
      font-style: normal;
      font-feature-settings: 'liga';
      font-variation-settings: 'FILL' 0;
      line-height: 1;
      letter-spacing: normal;
      text-transform: none;
      white-space: nowrap;
      direction: ltr;
      -webkit-font-smoothing: antialiased;
      user-select: none;
    }

    :host([filled])
    {
      font-variation-settings: 'FILL' 1;
    }
  `;

  declare filled: boolean;

  constructor ()
  {
    super ();
    this.filled = false;
  }

  override connectedCallback (): void
  {
    super.connectedCallback ();

    if (!this.hasAttribute ('aria-hidden'))
      this.setAttribute ('aria-hidden', 'true');
  }

  override render (): TemplateResult
  {
    return html`<slot></slot>`;
  }
}

customElements.define ('m3-icon', icon);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-icon': icon;
  }
}
