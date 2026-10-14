// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement, nothing } from 'lit';

import type { PropertyValues, TemplateResult } from 'lit';

import './focus-ring';
import './icon';
import './ripple';

// Navigation rail, the vertical bar that holds the application's
// destinations. For example:
//
// <m3-navigation-rail label="Main" expanded>
//   <div slot="header">...</div>
//   <m3-navigation-rail-item icon="home" label="Home" href="#home" selected>
//   </m3-navigation-rail-item>
//   ...
//   <div slot="footer">...</div>
// </m3-navigation-rail>
//
// When collapsed, the rail is 96dp wide and shows each label under its icon.
// When expanded, it shows the labels beside the icons and is as wide as its
// widest item, between 220 and 360dp. The rail changes width with the
// default spatial spring and passes expanded on to its items, which change
// their layout with the same spring.
//
// The items form a navigation landmark named by the label. The header slot
// holds what goes above the items (a menu button, say) and the footer slot
// holds what stays at the bottom. The content starts 44dp from the top and
// the items start 40dp below the header.
//
// A modal rail sits above the content with the surface container color at
// elevation level 2, has the large corner on its end side, and changes width
// with the fast spatial spring. Note that the application positions the
// modal rail and provides the scrim.
//
export class navigation_rail extends LitElement
{
  static override properties =
  {
    label:    {type: String},
    expanded: {type: Boolean, reflect: true},
    modal:    {type: Boolean, reflect: true}
  };

  static override styles = css`
    :host
    {
      display: flex;
      flex-direction: column;
      box-sizing: border-box;
      width: 96px;
      height: 100%;
      padding-block-start: 44px;
      overflow: hidden;
      background: var(--m3-navigation-rail-container-color,
                      var(--md-sys-color-surface));
      transition-property: width;
      transition-duration:
        var(--md-sys-motion-spring-default-spatial-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-spatial-easing);
    }

    :host([expanded])
    {
      width: calc-size(fit-content, clamp(220px, size, 360px));
    }

    :host([modal])
    {
      background: var(--md-sys-color-surface-container);
      box-shadow: var(--m3-elevation-shadow-level2);
      border-start-end-radius: var(--md-sys-shape-corner-large);
      border-end-end-radius: var(--md-sys-shape-corner-large);
      transition-duration:
        var(--md-sys-motion-spring-fast-spatial-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-spatial-easing);
    }

    ::slotted([slot='header'])
    {
      margin-block-end: 40px;
    }

    /*
     * The collapsed items are 4dp apart and the expanded ones touch.
     */
    nav
    {
      display: flex;
      flex-direction: column;
      gap: 4px;
      transition-property: gap;
      transition-duration: inherit;
      transition-timing-function: inherit;
    }

    :host([expanded]) nav
    {
      gap: 0;
    }

    .footer
    {
      margin-block-start: auto;
    }
  `;

  declare label: string;
  declare expanded: boolean;
  declare modal: boolean;

  constructor ()
  {
    super ();
    this.label = '';
    this.expanded = false;
    this.modal = false;
  }

  override render (): TemplateResult
  {
    return html`
      <slot name="header"></slot>
      <nav aria-label=${this.label || nothing}>
        <slot @slotchange=${() => {this.update_items ();}}></slot>
      </nav>
      <div class="footer"><slot name="footer"></slot></div>`;
  }

  protected override updated (changed: PropertyValues<this>): void
  {
    if (changed.has ('expanded') || changed.has ('modal'))
      this.update_items ();
  }

  // Pass expanded and modal on to the items in the default slot.
  //
  private update_items (): void
  {
    const s = this.renderRoot.querySelector<HTMLSlotElement> ('nav slot');

    if (s === null)
      return;

    for (const e of s.assignedElements ())
    {
      if (e instanceof navigation_rail_item)
      {
        e.expanded = this.expanded;
        e.modal = this.modal;
      }
    }
  }
}

// Destination of the navigation rail. The icon names the Material Symbols
// icon and the label is the visible name, which also names the link to href.
// The selected item is the current page and shows this with the active
// indicator, the filled icon, and the emphasized label.
//
// When collapsed, the indicator is a 56 by 32dp pill around the icon and the
// label (in label medium) is under it. When expanded, the indicator is a
// 56dp high pill around the icon and the label (in label large) beside it.
// The icon stays at the same distance from the start in both layouts.
//
// The item changes its layout with the rail's spring. While expanding, the
// indicator changes size, the label below fades out in the first half of the
// motion, and the label beside fades in during the second half. Collapsing
// plays this in reverse.
//
// The whole item takes the pointer and the keyboard focus while the
// indicator shows the state layer and the focus ring. On selection, the
// indicator expands from the center of the icon with the default spatial
// spring.
//
export class navigation_rail_item extends LitElement
{
  static override shadowRootOptions =
    {...LitElement.shadowRootOptions, delegatesFocus: true};

  static override properties =
  {
    icon:     {type: String},
    label:    {type: String},
    href:     {type: String},
    selected: {type: Boolean, reflect: true},
    expanded: {type: Boolean, reflect: true},
    modal:    {type: Boolean, reflect: true}
  };

  static override styles = css`
    :host
    {
      display: block;
      --_duration: var(--md-sys-motion-spring-default-spatial-duration);
      --_easing: var(--md-sys-motion-spring-default-spatial-easing);
      interpolate-size: allow-keywords;
    }

    :host([modal])
    {
      --_duration: var(--md-sys-motion-spring-fast-spatial-duration);
      --_easing: var(--md-sys-motion-spring-fast-spatial-easing);
    }

    /*
     * The link spans the width of the rail and places the indicator 20dp
     * from the start, which puts the icon 36dp from the start in both
     * layouts.
     */
    a
    {
      display: flex;
      flex-direction: column;
      align-items: flex-start;
      padding: 6px 20px;
      color: var(--md-sys-color-on-surface-variant);
      text-decoration: none;
      outline: none;
      -webkit-tap-highlight-color: transparent;
      -webkit-user-drag: none;
      transition-property: padding;
      transition-duration: var(--_duration);
      transition-timing-function: var(--_easing);
    }

    :host([expanded]) a
    {
      padding-block: 0;
    }

    /*
     * The indicator clips the label beside the icon to its width. Note that
     * the clip margin leaves room for the focus ring (3dp, 2dp outside).
     */
    .indicator
    {
      position: relative;
      display: flex;
      flex: none;
      align-items: center;
      box-sizing: border-box;
      width: 56px;
      height: 32px;
      padding-inline: 16px;
      border-radius: var(--md-sys-shape-corner-full);
      overflow: clip;
      overflow-clip-margin: 5px;
      transition-property: width, height, color;
      transition-duration:
        var(--_duration),
        var(--_duration),
        var(--md-sys-motion-spring-default-effects-duration);
      transition-timing-function:
        var(--_easing),
        var(--_easing),
        var(--md-sys-motion-spring-default-effects-easing);
    }

    :host([expanded]) .indicator
    {
      width: max-content;
      height: 56px;
    }

    :host([selected]) .indicator
    {
      color: var(--md-sys-color-on-secondary-container);
    }

    /*
     * The active indicator fades in as it grows from the center of the icon,
     * which is 28dp from the start of the indicator in both layouts.
     */
    .indicator::before
    {
      content: '';
      position: absolute;
      inset: 0;
      border-radius: inherit;
      background: var(--md-sys-color-secondary-container);
      opacity: 0;
      clip-path: inset(0 calc(100% - 28px) 0 28px round 9999px);
      transition-property: opacity, clip-path;
      transition-duration:
        var(--md-sys-motion-spring-default-spatial-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-spatial-easing);
    }

    :host([selected]) .indicator::before
    {
      opacity: 1;
      clip-path: inset(0 0 0 0 round 9999px);
    }

    m3-icon
    {
      position: relative;
    }

    .label
    {
      white-space: nowrap;
      transition-property: opacity, color;
      transition-duration:
        var(--md-sys-motion-spring-fast-effects-duration),
        var(--md-sys-motion-spring-default-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-effects-easing),
        var(--md-sys-motion-spring-default-effects-easing);
    }

    /*
     * The label below is centered under the indicator, which it may be wider
     * than.
     */
    .below
    {
      display: flex;
      justify-content: center;
      width: 56px;
      height: 16px;
      margin-block-start: 4px;
      transition-property: opacity, color, height, margin;
      transition-duration:
        var(--md-sys-motion-spring-fast-effects-duration),
        var(--md-sys-motion-spring-default-effects-duration),
        var(--_duration), var(--_duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-effects-easing),
        var(--md-sys-motion-spring-default-effects-easing),
        var(--_easing), var(--_easing);
      transition-delay: calc(var(--_duration) / 2), 0s, 0s, 0s;
      font-family: var(--md-sys-typescale-label-medium-font);
      font-size: var(--md-sys-typescale-label-medium-size);
      line-height: var(--md-sys-typescale-label-medium-line-height);
      font-weight: var(--md-sys-typescale-label-medium-weight);
      letter-spacing: var(--md-sys-typescale-label-medium-tracking);
    }

    :host([selected]) .below
    {
      color: var(--md-sys-color-secondary);
      font-weight:
        var(--md-sys-typescale-emphasized-label-medium-weight);
    }

    :host([expanded]) .below
    {
      height: 0;
      margin-block-start: 0;
      opacity: 0;
      transition-delay: 0s;
    }

    .beside
    {
      position: relative;
      margin-inline-start: 8px;
      opacity: 0;
      font-family: var(--md-sys-typescale-label-large-font);
      font-size: var(--md-sys-typescale-label-large-size);
      line-height: var(--md-sys-typescale-label-large-line-height);
      font-weight: var(--md-sys-typescale-label-large-weight);
      letter-spacing: var(--md-sys-typescale-label-large-tracking);
    }

    :host([selected]) .beside
    {
      font-weight:
        var(--md-sys-typescale-emphasized-label-large-weight);
    }

    :host([expanded]) .beside
    {
      opacity: 1;
      transition-delay: calc(var(--_duration) / 2), 0s;
    }
  `;

  declare icon: string;
  declare label: string;
  declare href: string;
  declare selected: boolean;
  declare expanded: boolean;
  declare modal: boolean;

  constructor ()
  {
    super ();
    this.icon = '';
    this.label = '';
    this.href = '';
    this.selected = false;
    this.expanded = false;
    this.modal = false;
  }

  // Render both labels, hiding them from assistive technology since the link
  // carries the name.
  //
  override render (): TemplateResult
  {
    return html`
      <a href=${this.href}
         aria-label=${this.label}
         aria-current=${this.selected ? 'page' : nothing}>
        <span class="indicator">
          <m3-focus-ring></m3-focus-ring>
          <m3-ripple></m3-ripple>
          <m3-icon ?filled=${this.selected}>${this.icon}</m3-icon>
          <span class="label beside" aria-hidden="true">${this.label}</span>
        </span>
        <span class="label below" aria-hidden="true">${this.label}</span>
      </a>`;
  }

  // Attach the indicator's focus ring and state layer to the link, so that
  // they respond to the whole item.
  //
  protected override firstUpdated (): void
  {
    const a = this.renderRoot.querySelector ('a');

    if (a === null)
      return;

    this.renderRoot.querySelector ('m3-focus-ring')?.attach (a);
    this.renderRoot.querySelector ('m3-ripple')?.attach (a);
  }
}

customElements.define ('m3-navigation-rail', navigation_rail);
customElements.define ('m3-navigation-rail-item', navigation_rail_item);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-navigation-rail': navigation_rail;
    'm3-navigation-rail-item': navigation_rail_item;
  }
}
