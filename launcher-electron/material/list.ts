// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement, nothing } from 'lit';

import type { PropertyValues, TemplateResult } from 'lit';

import './focus-ring';
import './ripple';

// List, a column of items. For example:
//
// <m3-list label="Online">
//   <m3-list-item action>
//     <m3-avatar slot="start" name="Kestrel"></m3-avatar>
//     Kestrel
//     <span slot="supporting">Terminal, Search and Destroy</span>
//   </m3-list-item>
// </m3-list>
//
// To assistive technology the list is a list named by the label, if any.
//
// The list is standard unless segmented is set. A standard list has 8dp of
// padding above and below its items. In a segmented list the items are 2dp
// apart on the surface color and the first and last items round the outer
// corners of the list with the large corner.
//
export class list extends LitElement
{
  static override properties =
  {
    label:     {type: String},
    segmented: {type: Boolean, reflect: true}
  };

  static override styles = css`
    :host
    {
      display: flex;
      flex-direction: column;
      padding-block: 8px;
    }

    :host([segmented])
    {
      gap: 2px;
      padding-block: 0;
    }

    :host([segmented]) ::slotted(*)
    {
      --m3-list-item-container-color: var(--md-sys-color-surface);
    }
  `;

  declare label: string;
  declare segmented: boolean;

  constructor ()
  {
    super ();
    this.label = '';
    this.segmented = false;
  }

  override connectedCallback (): void
  {
    super.connectedCallback ();
    this.setAttribute ('role', 'list');
  }

  override render (): TemplateResult
  {
    return html`<slot @slotchange=${() => {this.update_items ();}}></slot>`;
  }

  protected override updated (changed: PropertyValues<this>): void
  {
    if (changed.has ('segmented'))
      this.update_items ();

    if (changed.has ('label'))
    {
      if (this.label !== '')
        this.setAttribute ('aria-label', this.label);
      else
        this.removeAttribute ('aria-label');
    }
  }

  // Set the segment of each item in the default slot, which is the item's
  // place in a segmented list (and empty in a standard list).
  //
  private update_items (): void
  {
    const s = this.renderRoot.querySelector ('slot');

    if (s === null)
      return;

    const is = s.assignedElements ().filter ((e) => e instanceof list_item);

    for (const [i, e] of is.entries ())
    {
      e.segment = !this.segmented ? ''
        : is.length === 1          ? 'only'
        : i === 0                  ? 'first'
        : i === is.length - 1      ? 'last'
        :                            'middle';
    }
  }
}

// List item, a row of a list. For example:
//
// <m3-list-item href="#/friends/kestrel">
//   <m3-avatar slot="start" name="Kestrel"></m3-avatar>
//   Kestrel
//   <span slot="supporting">Online</span>
//   <m3-icon-button slot="end" icon="person_add" label="Add">
//   </m3-icon-button>
// </m3-list-item>
//
// The text content is the headline. The start slot holds the leading
// element (an avatar, say), the supporting slot holds the supporting text,
// and the end slot holds the trailing text or controls. The item is 56dp high
// (one line) or, if there is supporting text, 72dp high (two lines), both
// adjusted by the density offset (see tokens/density.css).
//
// If there is an href, then the item is a link. If action is set, then the
// item is a button and its clicks reach the page as click events on the
// item. Either kind shows the state layer and the focus ring. A selected item
// takes the secondary container color, which fades in and out with the
// default effects spring.
//
// The item's corners change with its state using the fast spatial spring.
// They are extra small at rest, medium while hovered, and large while
// pressed, focused, or selected. In a segmented list the first and last
// items keep the large corner on the outer side of the list (see list).
//
// The container color is --m3-list-item-container-color, which a standard
// list leaves unset (transparent) and a segmented list sets to the surface
// color.
//
// The end slot sits beside the link or button, so its controls are targets
// of their own. Note that their clicks also bubble to the item, so a control
// there should stop the propagation of its click if the page handles clicks
// on the item.
//
export class list_item extends LitElement
{
  static override properties =
  {
    href:      {type: String, reflect: true},
    action:    {type: Boolean, reflect: true},
    selected:  {type: Boolean, reflect: true},
    disabled:  {type: Boolean, reflect: true},
    segment:   {type: String, reflect: true},
    start_:    {state: true},
    end_:      {state: true},
    two_line_: {state: true}
  };

  static override styles = css`
    :host
    {
      position: relative;
      display: block;
      --_top: var(--md-sys-shape-corner-extra-small);
      --_bottom: var(--md-sys-shape-corner-extra-small);
      border-start-start-radius: var(--_top);
      border-start-end-radius: var(--_top);
      border-end-start-radius: var(--_bottom);
      border-end-end-radius: var(--_bottom);
      background: var(--m3-list-item-container-color, none);
      color: var(--md-sys-color-on-surface);
      --_height: calc(56px + var(--m3-density-offset, 0px));
      transition-property: background-color, color, border-radius;
      transition-duration:
        var(--md-sys-motion-spring-default-effects-duration),
        var(--md-sys-motion-spring-default-effects-duration),
        var(--md-sys-motion-spring-fast-spatial-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-effects-easing),
        var(--md-sys-motion-spring-default-effects-easing),
        var(--md-sys-motion-spring-fast-spatial-easing);
    }

    :host(:is([segment='first'], [segment='only']))
    {
      --_top: var(--md-sys-shape-corner-large);
    }

    :host(:is([segment='last'], [segment='only']))
    {
      --_bottom: var(--md-sys-shape-corner-large);
    }

    :host(:is([href]:not([href='']), [action]):not([disabled]):hover)
    {
      --_top: var(--md-sys-shape-corner-medium);
      --_bottom: var(--md-sys-shape-corner-medium);
    }

    :host(:is([href]:not([href='']), [action]):not([disabled]):active),
    :host(:is([href]:not([href='']), [action]):not([disabled]):focus-within),
    :host([selected])
    {
      --_top: var(--md-sys-shape-corner-large);
      --_bottom: var(--md-sys-shape-corner-large);
    }

    /*
     * Note that the selected item also tells an avatar in it which surface
     * its presence dot sits on.
     */
    :host([selected])
    {
      background: var(--md-sys-color-secondary-container);
      color: var(--md-sys-color-on-secondary-container);
      --m3-avatar-surface: var(--md-sys-color-secondary-container);
    }

    :host([disabled])
    {
      color: rgb(from var(--md-sys-color-on-surface) r g b / 0.38);
    }

    .item
    {
      position: relative;
      display: flex;
      align-items: center;
      gap: 12px;
      box-sizing: border-box;
      min-height: var(--_height);
      padding-block: 10px;
      padding-inline: 16px var(--_end-width, 16px);
      border-radius: inherit;
      color: inherit;
      text-decoration: none;
      outline: none;
      -webkit-tap-highlight-color: transparent;
      -webkit-user-drag: none;
    }

    .item.two-line
    {
      --_height: calc(72px + var(--m3-density-offset, 0px));
    }

    button.item
    {
      width: 100%;
      border: none;
      background: none;
      font: inherit;
      text-align: start;
      cursor: pointer;
    }

    button.item:disabled
    {
      cursor: default;
    }

    .text
    {
      display: flex;
      flex: 1;
      flex-direction: column;
      min-width: 0;
    }

    .headline,
    .supporting
    {
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .headline
    {
      font-family: var(--md-sys-typescale-body-large-font);
      font-size: var(--md-sys-typescale-body-large-size);
      line-height: var(--md-sys-typescale-body-large-line-height);
      font-weight: var(--md-sys-typescale-body-large-weight);
      letter-spacing: var(--md-sys-typescale-body-large-tracking);
    }

    .supporting
    {
      color: var(--md-sys-color-on-surface-variant);
      font-family: var(--md-sys-typescale-body-medium-font);
      font-size: var(--md-sys-typescale-body-medium-size);
      line-height: var(--md-sys-typescale-body-medium-line-height);
      font-weight: var(--md-sys-typescale-body-medium-weight);
      letter-spacing: var(--md-sys-typescale-body-medium-tracking);
    }

    :host([selected]) .supporting
    {
      color: inherit;
    }

    :host([disabled]) .supporting
    {
      color: inherit;
    }

    :host([disabled]) .start,
    :host([disabled]) .end
    {
      opacity: 0.38;
    }

    .start
    {
      display: flex;
      flex: none;
    }

    /*
     * The end slot sits over the item's end padding, outside of the link or
     * button, since HTML doesn't allow interactive content inside them.
     */
    .end
    {
      position: absolute;
      inset-block: 0;
      inset-inline-end: 16px;
      display: flex;
      align-items: center;
      gap: 8px;
      color: var(--md-sys-color-on-surface-variant);
      font-family: var(--md-sys-typescale-label-small-font);
      font-size: var(--md-sys-typescale-label-small-size);
      line-height: var(--md-sys-typescale-label-small-line-height);
      font-weight: var(--md-sys-typescale-label-small-weight);
      letter-spacing: var(--md-sys-typescale-label-small-tracking);
    }

    .start:not(.filled),
    .end:not(.filled),
    .supporting:not(.filled)
    {
      display: none;
    }
  `;

  declare href: string;
  declare action: boolean;
  declare selected: boolean;
  declare disabled: boolean;
  declare segment: '' | 'only' | 'first' | 'middle' | 'last';
  declare private start_: boolean;
  declare private end_: boolean;
  declare private two_line_: boolean;

  // Keep the end padding as wide as the end slot's content plus its 16dp
  // inset and a 16dp gap, so that the text never runs under the content.
  //
  private readonly resize_ = new ResizeObserver ((es) =>
  {
    const w = es[0]?.borderBoxSize[0]?.inlineSize ?? 0;
    this.style.setProperty ('--_end-width', `${w + 16 + 16}px`);
  });

  constructor ()
  {
    super ();
    this.href = '';
    this.action = false;
    this.selected = false;
    this.disabled = false;
    this.segment = '';
    this.start_ = false;
    this.end_ = false;
    this.two_line_ = false;
  }

  override connectedCallback (): void
  {
    super.connectedCallback ();
    this.setAttribute ('role', 'listitem');
  }

  override disconnectedCallback (): void
  {
    super.disconnectedCallback ();
    this.resize_.disconnect ();
  }

  override render (): TemplateResult
  {
    const content = html`
      <span class=${this.start_ ? 'start filled' : 'start'}>
        <slot name="start" @slotchange=${this.on_slot}></slot>
      </span>
      <span class="text">
        <span class="headline"><slot></slot></span>
        <span class=${this.two_line_ ? 'supporting filled' : 'supporting'}>
          <slot name="supporting" @slotchange=${this.on_slot}></slot>
        </span>
      </span>`;

    const c = this.two_line_ ? 'item two-line' : 'item';
    const state = html`
      <m3-ripple></m3-ripple>
      <m3-focus-ring></m3-focus-ring>`;

    // Render the item as a link (if it has an href and is enabled), as a
    // button (if action is set), or as plain text. The end slot goes beside
    // it.
    //
    const item = this.href !== '' && !this.disabled
      ? html`<a class=${c} href=${this.href}>${content}${state}</a>`
      : this.action
        ? html`
          <button class=${c} type="button" ?disabled=${this.disabled}
                  aria-current=${this.selected ? 'true' : nothing}>
            ${content}${this.disabled ? nothing : state}
          </button>`
        : html`<div class=${c}>${content}</div>`;

    return html`
      ${item}
      <span class=${this.end_ ? 'end filled' : 'end'}>
        <slot name="end" @slotchange=${this.on_slot}></slot>
      </span>`;
  }

  // Update the layout according to the slots' content:
  //
  // - an empty slot takes no room
  // - supporting text makes the item two lines high
  // - the width of the end slot's content sets the end padding (see resize_)
  //
  private on_slot (): void
  {
    const filled = (n: string): boolean =>
    {
      const s = this.renderRoot.querySelector<HTMLSlotElement> (
        `slot[name="${n}"]`);

      return (s?.assignedNodes ({flatten: true}).length ?? 0) !== 0;
    };

    this.start_ = filled ('start');
    this.end_ = filled ('end');
    this.two_line_ = filled ('supporting');

    const e = this.renderRoot.querySelector ('.end');

    this.resize_.disconnect ();

    if (this.end_ && e !== null)
      this.resize_.observe (e);
    else
      this.style.removeProperty ('--_end-width');
  }
}
customElements.define ('m3-list', list);
customElements.define ('m3-list-item', list_item);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-list': list;
    'm3-list-item': list_item;
  }
}
