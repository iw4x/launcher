// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement, nothing } from 'lit';

import type { TemplateResult } from 'lit';

import './focus-ring';
import './icon';
import './ripple';

// Filter chip that narrows content to what it names. For example:
//
// <m3-filter-chip selected>Servers</m3-filter-chip>
//
// The text content is the label. A click toggles selected and dispatches the
// change event. While selected, the chip has the secondary container color
// and a leading check mark. To assistive technology the chip is a toggle
// button.
//
// The icon, if any, names a leading Material Symbols icon in the primary
// role, which the check mark replaces while the chip is selected.
//
// If there is no icon, then on selection the check mark makes room for
// itself with the fast spatial spring and fades in with the slow effects
// spring. On deselection, it fades out with the fast effects spring while
// the room closes with the default effects spring. If there is an icon, then
// the icon and the check mark cross-fade.
//
// The chip is 32dp high with the small corner. While not selected, it has an
// outline in the outline variant role. While selected, it rises to
// elevation level 1 on hover.
//
export class filter_chip extends LitElement
{
  static override shadowRootOptions =
    {...LitElement.shadowRootOptions, delegatesFocus: true};

  static override properties =
  {
    selected: {type: Boolean, reflect: true},
    disabled: {type: Boolean, reflect: true},
    icon:     {type: String}
  };

  static override styles = css`
    :host
    {
      display: inline-flex;
      flex: none;
      vertical-align: middle;
      --_color: var(--md-sys-color-on-surface-variant);
      --_container: transparent;
      --_outline: var(--md-sys-color-outline-variant);
    }

    :host([selected])
    {
      --_color: var(--md-sys-color-on-secondary-container);
      --_container: var(--md-sys-color-secondary-container);
      --_outline: transparent;
    }

    :host([disabled])
    {
      --_color: rgb(from var(--md-sys-color-on-surface) r g b / 0.38);
      --_outline: rgb(from var(--md-sys-color-on-surface) r g b / 0.12);
    }

    :host([disabled][selected])
    {
      --_container: rgb(from var(--md-sys-color-on-surface) r g b / 0.12);
      --_outline: transparent;
    }

    button
    {
      position: relative;
      display: inline-flex;
      align-items: center;
      box-sizing: border-box;
      height: 32px;
      padding-inline: 8px 16px;
      border: 1px solid var(--_outline);
      border-radius: var(--md-sys-shape-corner-small);
      background: var(--_container);
      color: var(--_color);
      outline: none;
      cursor: pointer;
      -webkit-tap-highlight-color: transparent;
      font-family: var(--md-sys-typescale-label-large-font);
      font-size: var(--md-sys-typescale-label-large-size);
      line-height: var(--md-sys-typescale-label-large-line-height);
      font-weight: var(--md-sys-typescale-label-large-weight);
      letter-spacing: var(--md-sys-typescale-label-large-tracking);
      transition-property: background-color, border-color, box-shadow;
      transition-duration:
        var(--md-sys-motion-spring-fast-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-effects-easing);
    }

    :host([selected]:not([disabled])) button:hover
    {
      box-shadow: var(--m3-elevation-shadow-level1);
    }

    button:disabled
    {
      cursor: default;
    }

    /*
     * If there is no icon and the chip is not selected, then the leading
     * room is 0dp wide, which leaves 16dp (the padding plus the margin)
     * before the label.
     */
    .leading
    {
      position: relative;
      flex: none;
      width: 0;
      height: 18px;
      margin-inline-end: 8px;
      overflow: clip;
      transition-property: width;
      transition-duration:
        var(--md-sys-motion-spring-default-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-effects-easing);
    }

    :host([selected]) .leading,
    .icon .leading
    {
      width: 18px;
      transition-duration:
        var(--md-sys-motion-spring-fast-spatial-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-spatial-easing);
    }

    m3-icon
    {
      position: absolute;
      inset-block-start: 0;
      inset-inline-start: 0;
      --m3-icon-size: 18px;
      transition-property: opacity;
      transition-duration:
        var(--md-sys-motion-spring-fast-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-effects-easing);
    }

    .check,
    :host([selected]) .custom
    {
      opacity: 0;
    }

    :host([selected]) .check
    {
      opacity: 1;
      transition-duration:
        var(--md-sys-motion-spring-slow-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-slow-effects-easing);
    }

    .custom
    {
      color: var(--md-sys-color-primary);
    }

    :host([disabled]) .custom
    {
      color: inherit;
    }
  `;

  declare selected: boolean;
  declare disabled: boolean;
  declare icon: string;

  constructor ()
  {
    super ();
    this.selected = false;
    this.disabled = false;
    this.icon = '';
  }

  override render (): TemplateResult
  {
    return html`
      <button type="button"
              class=${this.icon !== '' ? 'icon' : ''}
              aria-pressed=${this.selected ? 'true' : 'false'}
              ?disabled=${this.disabled}
              @click=${this.on_click}>
        <m3-focus-ring></m3-focus-ring>
        ${this.disabled ? nothing : html`<m3-ripple></m3-ripple>`}
        <span class="leading">
          ${this.icon !== ''
            ? html`<m3-icon class="custom">${this.icon}</m3-icon>`
            : nothing}
          <m3-icon class="check">check</m3-icon>
        </span>
        <slot></slot>
      </button>`;
  }

  private on_click (): void
  {
    this.selected = !this.selected;
    this.dispatchEvent (new Event ('change', {bubbles: true}));
  }
}

customElements.define ('m3-filter-chip', filter_chip);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-filter-chip': filter_chip;
  }
}
