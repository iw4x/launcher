// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement } from 'lit';

import type { TemplateResult } from 'lit';

import './focus-ring';
import './ripple';

// Card, a container for the content and actions of one subject. For
// example:
//
// <m3-card variant="filled" href="#/servers">...</m3-card>
//
// The variant is elevated (surface container low at elevation level 1),
// filled (surface container highest, the default), or outlined (surface with
// an outline variant border). The page can override the variant's color with
// --m3-card-container-color and the medium corner with --m3-card-shape.
//
// If there is an href, then the whole card is a link that shows the state
// layer and the focus ring and that rises by one level on hover (an elevated
// card to level 2 and the others to level 1). Note that the content of such
// a card should have no links or buttons of its own since HTML doesn't allow
// interactive content inside a link.
//
export class card extends LitElement
{
  static override shadowRootOptions =
    {...LitElement.shadowRootOptions, delegatesFocus: true};

  static override properties =
  {
    variant: {type: String, reflect: true},
    href:    {type: String}
  };

  static override styles = css`
    :host
    {
      display: block;
      border-radius: var(--m3-card-shape, var(--md-sys-shape-corner-medium));
      --_container: var(--md-sys-color-surface-container-highest);
      --_outline: transparent;
      --_shadow: none;
      --_hover-shadow: var(--m3-elevation-shadow-level1);
    }

    :host([variant='elevated'])
    {
      --_container: var(--md-sys-color-surface-container-low);
      --_shadow: var(--m3-elevation-shadow-level1);
      --_hover-shadow: var(--m3-elevation-shadow-level2);
    }

    :host([variant='outlined'])
    {
      --_container: var(--md-sys-color-surface);
      --_outline: var(--md-sys-color-outline-variant);
    }

    .card
    {
      position: relative;
      display: block;
      box-sizing: border-box;
      height: 100%;
      border: 1px solid var(--_outline);
      border-radius: inherit;
      background: var(--m3-card-container-color, var(--_container));
      box-shadow: var(--_shadow);
      color: var(--md-sys-color-on-surface);
      text-decoration: none;
      outline: none;
      -webkit-tap-highlight-color: transparent;
      -webkit-user-drag: none;
      transition-property: box-shadow;
      transition-duration:
        var(--md-sys-motion-spring-default-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-effects-easing);
    }

    a.card:hover
    {
      box-shadow: var(--_hover-shadow);
    }

    /*
     * The content is clipped to the corners. The state layer comes after it
     * so that it covers the whole card, images included.
     */
    .content
    {
      position: relative;
      height: 100%;
      border-radius: inherit;
      overflow: hidden;
    }
  `;

  declare variant: 'elevated' | 'filled' | 'outlined';
  declare href: string;

  constructor ()
  {
    super ();
    this.variant = 'filled';
    this.href = '';
  }

  override render (): TemplateResult
  {
    const content = html`<div class="content"><slot></slot></div>`;

    return this.href !== ''
      ? html`
        <a class="card" href=${this.href}>
          ${content}
          <m3-ripple></m3-ripple>
          <m3-focus-ring></m3-focus-ring>
        </a>`
      : html`<div class="card">${content}</div>`;
  }
}

customElements.define ('m3-card', card);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-card': card;
  }
}
