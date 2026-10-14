// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement, nothing } from 'lit';

import type { TemplateResult } from 'lit';

// Badge that marks new or pending items on the element it goes with. For
// example:
//
// <m3-tab name="requests">Requests <m3-badge value="3"></m3-badge></m3-tab>
//
// If there is no value, then the badge is a 6dp dot (the small badge).
// Otherwise, it is a 16dp pill that shows the value (the large badge). A
// value above max (999 by default) is shown as max followed by a plus sign,
// as in 999+. Note that the badge sits in the flow, so the element decides
// where it goes.
//
// The label, if any, is the badge's accessible name, for example "3 pending
// requests". Otherwise, the badge is hidden from assistive technology and the
// element should convey what it marks.
//
export class badge extends LitElement
{
  static override properties =
  {
    value: {type: Number},
    max:   {type: Number},
    label: {type: String}
  };

  static override styles = css`
    :host
    {
      display: inline-flex;
      flex: none;
      vertical-align: middle;
    }

    span
    {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      box-sizing: border-box;
      border-radius: var(--md-sys-shape-corner-full);
      background: var(--md-sys-color-error);
      color: var(--md-sys-color-on-error);
    }

    .small
    {
      width: 6px;
      height: 6px;
    }

    .large
    {
      min-width: 16px;
      height: 16px;
      padding-inline: 4px;
      font-family: var(--md-sys-typescale-label-small-font);
      font-size: var(--md-sys-typescale-label-small-size);
      line-height: var(--md-sys-typescale-label-small-line-height);
      font-weight: var(--md-sys-typescale-label-small-weight);
      letter-spacing: var(--md-sys-typescale-label-small-tracking);
      font-variant-numeric: tabular-nums;
    }
  `;

  declare value: number | null;
  declare max: number;
  declare label: string;

  constructor ()
  {
    super ();
    this.value = null;
    this.max = 999;
    this.label = '';
  }

  override render (): TemplateResult
  {
    const v = this.value;
    const named = this.label !== '';

    return html`
      <span class=${v === null ? 'small' : 'large'}
            role=${named ? 'img' : nothing}
            aria-label=${named ? this.label : nothing}
            aria-hidden=${named ? nothing : 'true'}>
        ${v === null ? '' : v > this.max ? `${this.max}+` : String (v)}
      </span>`;
  }
}

customElements.define ('m3-badge', badge);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-badge': badge;
  }
}
