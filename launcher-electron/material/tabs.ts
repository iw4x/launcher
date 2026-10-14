// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement } from 'lit';

import type { PropertyValues, TemplateResult } from 'lit';

import './focus-ring';
import './ripple';

// Primary tabs that switch between the sections of a view. For example:
//
// <m3-tabs label="Home" selected="general">
//   <m3-tab name="general" id="general-tab">General</m3-tab>
//   <m3-tab name="news" id="news-tab">News</m3-tab>
// </m3-tabs>
//
// The selected tab is the one whose name is in selected. When the user
// selects another tab, the element updates selected and dispatches the
// change event. The view then shows the section that goes with the selected
// tab in an element with the tabpanel role labeled by the tab's id.
//
// Note that we set the roles and states of the ARIA tabs pattern on the
// elements themselves since they are in the view's tree, where the panels
// can refer to the tabs. Following the pattern's automatic activation, only
// the selected tab is in the tab sequence and the arrow keys (as well as
// Home and End) move the selection.
//
// The tabs sit on a divider in the outline variant role. The active
// indicator is a 3dp primary bar under the label of the selected tab, which
// moves to the newly selected tab with the default spatial spring.
//
export class tabs extends LitElement
{
  static override properties =
  {
    label:    {type: String},
    selected: {type: String, reflect: true}
  };

  static override styles = css`
    :host
    {
      position: relative;
      display: flex;
      border-block-end: 1px solid var(--md-sys-color-outline-variant);
    }

    .indicator
    {
      position: absolute;
      inset-block-end: -1px;
      inset-inline-start: 0;
      width: var(--_width, 0);
      height: 3px;
      border-start-start-radius: 3px;
      border-start-end-radius: 3px;
      background: var(--md-sys-color-primary);
      transform: translateX(var(--_x, 0));
    }

    .indicator.placed
    {
      transition-property: transform, width;
      transition-duration:
        var(--md-sys-motion-spring-default-spatial-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-spatial-easing);
    }
  `;

  declare label: string;
  declare selected: string;

  private readonly resize_ = new ResizeObserver (() => {this.place ();});
  private placed_ = false;

  private readonly on_click = (e: Event): void =>
  {
    const t = this.tab_of (e);

    if (t !== null)
      this.select (t);
  };

  private readonly on_keydown = (e: KeyboardEvent): void =>
  {
    const ts = this.tabs ();
    const i = ts.findIndex ((t) => t.name === this.selected);
    let n = -1;

    switch (e.key)
    {
      case 'ArrowRight': n = (i + 1) % ts.length;             break;
      case 'ArrowLeft':  n = (i - 1 + ts.length) % ts.length; break;
      case 'Home':       n = 0;                               break;
      case 'End':        n = ts.length - 1;                   break;
      default:           return;
    }

    e.preventDefault ();

    const t = ts[n];

    if (t !== undefined)
    {
      this.select (t);
      t.focus ();
    }
  };

  constructor ()
  {
    super ();
    this.label = '';
    this.selected = '';
  }

  override connectedCallback (): void
  {
    super.connectedCallback ();

    this.setAttribute ('role', 'tablist');
    this.addEventListener ('click', this.on_click);
    this.addEventListener ('keydown', this.on_keydown);
    this.resize_.observe (this);
  }

  override disconnectedCallback (): void
  {
    super.disconnectedCallback ();

    this.removeEventListener ('click', this.on_click);
    this.removeEventListener ('keydown', this.on_keydown);
    this.resize_.disconnect ();
  }

  override render (): TemplateResult
  {
    return html`
      <slot @slotchange=${() => {this.update_tabs ();}}></slot>
      <div class="indicator"></div>`;
  }

  protected override updated (changed: PropertyValues<this>): void
  {
    if (changed.has ('label'))
      this.setAttribute ('aria-label', this.label);

    if (changed.has ('selected'))
      this.update_tabs ();
  }

  // Return the tabs in their order.
  //
  private tabs (): tab[]
  {
    return [...this.children].filter ((c): c is tab => c instanceof tab);
  }

  // Return our tab that the event happened in or null if there is none.
  //
  private tab_of (e: Event): tab | null
  {
    for (const t of e.composedPath ())
    {
      if (t instanceof tab && t.parentElement === this)
        return t;
    }

    return null;
  }

  // Select the tab and dispatch the change event unless the tab is already
  // selected.
  //
  private select (t: tab): void
  {
    if (t.name === this.selected)
      return;

    this.selected = t.name;
    this.dispatchEvent (new Event ('change', {bubbles: true}));
  }

  // Pass the selection on to the tabs and move the indicator under the
  // selected tab.
  //
  private update_tabs (): void
  {
    for (const t of this.tabs ())
    {
      t.selected = t.name === this.selected;
      this.resize_.observe (t);
    }

    this.place ();
  }

  // Place the indicator under the label of the selected tab, which is
  // narrower than the tab itself.
  //
  // Note that the indicator appears in place the first time and moves from
  // then on.
  //
  private place (): void
  {
    const i = this.renderRoot.querySelector<HTMLElement> ('.indicator');
    const t = this.tabs ().find ((x) => x.selected);

    if (i === null)
      return;

    i.classList.toggle ('placed', this.placed_);
    this.placed_ = t !== undefined;

    if (t === undefined)
    {
      i.style.setProperty ('--_width', '0');
      return;
    }

    const r = t.content_rect ();
    const o = this.getBoundingClientRect ();

    i.style.setProperty ('--_x', `${r.left - o.left}px`);
    i.style.setProperty ('--_width', `${r.width}px`);
  }
}

// Tab of the primary tabs, identified by its name. The text content is the
// label (in title small). The label of the selected tab is in the primary
// role and the others are in the on-surface-variant role. The primary color
// fades in with the default effects spring and out with the fast effects
// spring.
//
export class tab extends LitElement
{
  static override properties =
  {
    name:     {type: String, reflect: true},
    selected: {type: Boolean, reflect: true}
  };

  static override styles = css`
    :host
    {
      position: relative;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      box-sizing: border-box;
      height: 48px;
      padding-inline: 16px;
      color: var(--md-sys-color-on-surface-variant);
      font-family: var(--md-sys-typescale-title-small-font);
      font-size: var(--md-sys-typescale-title-small-size);
      line-height: var(--md-sys-typescale-title-small-line-height);
      font-weight: var(--md-sys-typescale-title-small-weight);
      letter-spacing: var(--md-sys-typescale-title-small-tracking);
      white-space: nowrap;
      cursor: pointer;
      outline: none;
      user-select: none;
      -webkit-tap-highlight-color: transparent;
      transition-property: color;
      transition-duration:
        var(--md-sys-motion-spring-fast-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-fast-effects-easing);
    }

    :host([selected])
    {
      color: var(--md-sys-color-primary);
      transition-duration:
        var(--md-sys-motion-spring-default-effects-duration);
      transition-timing-function:
        var(--md-sys-motion-spring-default-effects-easing);
    }

    /*
     * The focus ring hugs the tab and leaves its bottom 4dp, where the active
     * indicator lies, clear.
     */
    m3-focus-ring
    {
      inset-block: 4px 9px;
      inset-inline: 4px;
      border-radius: var(--md-sys-shape-corner-small);
    }
  `;

  declare name: string;
  declare selected: boolean;

  constructor ()
  {
    super ();
    this.name = '';
    this.selected = false;
  }

  override connectedCallback (): void
  {
    super.connectedCallback ();
    this.setAttribute ('role', 'tab');
  }

  override render (): TemplateResult
  {
    return html`
      <m3-focus-ring></m3-focus-ring>
      <m3-ripple></m3-ripple>
      <span class="content"><slot></slot></span>`;
  }

  // Attach the focus ring and the state layer to the tab itself since it
  // takes the focus and the pointer.
  //
  protected override firstUpdated (): void
  {
    this.renderRoot.querySelector ('m3-focus-ring')?.attach (this);
    this.renderRoot.querySelector ('m3-ripple')?.attach (this);
  }

  protected override updated (changed: PropertyValues<this>): void
  {
    if (changed.has ('selected'))
    {
      this.setAttribute ('aria-selected', String (this.selected));
      this.tabIndex = this.selected ? 0 : -1;
    }
  }

  // Return the rectangle of the label in client coordinates.
  //
  content_rect (): DOMRect
  {
    const c = this.renderRoot.querySelector ('.content') ?? this;
    return c.getBoundingClientRect ();
  }
}

customElements.define ('m3-tabs', tabs);
customElements.define ('m3-tab', tab);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-tabs': tabs;
    'm3-tab': tab;
  }
}
