// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { css, html, LitElement, nothing } from 'lit';

import type { TemplateResult } from 'lit';

import { dialog } from '../dialog';
import { icon_button } from '../icon-button';
import { navigation_rail_item } from '../navigation-rail';
import { search_view } from '../search-view';
import { tabs } from '../tabs';
import { text_field } from '../text-field';

import { section_styles } from './tokens';

// Gallery sections of the components, one element per component. Each
// section shows the component in its variants and states.
//

// Sample of icons that the icon section shows outlined and filled.
//
const icon_names: readonly string[] =
[
  'download', 'folder_open', 'play_arrow', 'settings', 'update',
  'check_circle', 'error', 'arrow_back', 'arrow_forward',
  'remove', 'crop_square', 'filter_none', 'close'
];

class gallery_icon extends LitElement
{
  static override styles = [section_styles, css`
    .specimen
    {
      display: flex;
      align-items: center;
      gap: 12px;
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Icon</h2>
      <div class="grid">
        ${icon_names.map ((n) => html`
          <div class="specimen">
            <m3-icon>${n}</m3-icon>
            <m3-icon filled>${n}</m3-icon>
            <span class="name">${n}</span>
          </div>`)}
      </div>`;
  }
}

// Focus ring and state layer on plain buttons, enabled and disabled.
//
class gallery_interaction extends LitElement
{
  static override styles = [section_styles, css`
    button
    {
      position: relative;
      height: 80px;
      padding: 12px;
      border: none;
      border-radius: var(--md-sys-shape-corner-large);
      background: var(--md-sys-color-secondary-container);
      color: var(--md-sys-color-on-secondary-container);
      font: inherit;
      text-align: start;
      outline: none;
    }

    button:disabled
    {
      background: rgb(from var(--md-sys-color-on-surface) r g b / 0.1);
      color: rgb(from var(--md-sys-color-on-surface) r g b / 0.38);
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Focus ring and ripple</h2>
      <div class="grid">
        <button type="button">
          <m3-focus-ring></m3-focus-ring>
          <m3-ripple></m3-ripple>
          <span class="name">enabled</span>
        </button>
        <button type="button" disabled>
          <m3-focus-ring></m3-focus-ring>
          <m3-ripple></m3-ripple>
          <span class="name">disabled</span>
        </button>
      </div>`;
  }
}

// Avatar in each size, without presence and with each presence.
//
const avatar_presences: readonly string[] =
  ['', 'online', 'active', 'away', 'offline'];

const avatar_names: readonly string[] =
  ['Marlow Vega', 'Vega', 'Kestrel'];

class gallery_avatar extends LitElement
{
  static override styles = [section_styles, css`
    .specimen
    {
      display: flex;
      align-items: center;
      gap: 12px;
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Avatar</h2>
      <div class="grid">
        ${avatar_presences.map ((p, i) => html`
          <div>
            <div class="specimen">
              <m3-avatar size="small" name=${avatar_names[i % 3] ?? ''}
                         presence=${p}>
              </m3-avatar>
              <m3-avatar name=${avatar_names[(i + 1) % 3] ?? ''}
                         presence=${p}>
              </m3-avatar>
              <m3-avatar size="large" name=${avatar_names[(i + 2) % 3] ?? ''}
                         presence=${p}>
              </m3-avatar>
            </div>
            <span class="name">${p !== '' ? p : 'no presence'}</span>
          </div>`)}
      </div>`;
  }
}

// Small badge and large badge with a few values (one of them above the
// maximum), each before and after a label.
//
const badge_values: readonly (number | null)[] = [null, 3, 42, 1200];

class gallery_badge extends LitElement
{
  static override styles = [section_styles, css`
    .specimen
    {
      display: flex;
      align-items: center;
      gap: 8px;
      height: 24px;
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Badge</h2>
      <div class="grid">
        ${badge_values.map ((v) => html`
          <div>
            <div class="specimen">
              <m3-badge .value=${v}></m3-badge>
              <span>Requests</span>
              <m3-badge .value=${v}></m3-badge>
            </div>
            <span class="name">${v === null ? 'small' : `large, ${v}`}</span>
          </div>`)}
      </div>`;
  }
}

// Button variants.
//
const button_variants: readonly string[] =
  ['filled', 'tonal', 'outlined', 'text', 'elevated'];

// Each button variant in both sizes, with and without an icon, as well as
// disabled.
//
class gallery_button extends LitElement
{
  static override styles = [section_styles, css`
    .row
    {
      display: flex;
      align-items: center;
      flex-wrap: wrap;
      gap: 12px;
      margin-block-end: 16px;
    }

    .row .name
    {
      width: 96px;
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Button</h2>
      ${button_variants.map ((v) => html`
        <div class="row">
          <span class="name">${v}</span>
          <m3-button variant=${v}>Label</m3-button>
          <m3-button variant=${v} icon="play_arrow">Play</m3-button>
          <m3-button variant=${v} size="medium" icon="play_arrow">
            Play
          </m3-button>
          <m3-button variant=${v} disabled>Disabled</m3-button>
        </div>`)}`;
  }
}

// Card variants, each static and as a link.
//
const card_variants: readonly string[] = ['elevated', 'filled', 'outlined'];

class gallery_card extends LitElement
{
  static override styles = [section_styles, css`
    m3-card
    {
      height: 120px;
    }

    .body
    {
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      box-sizing: border-box;
      height: 100%;
      padding: 16px;
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Card</h2>
      <div class="grid">
        ${card_variants.flatMap ((v) => [false, true].map ((l) => html`
          <m3-card variant=${v} href=${l ? '#' : ''}
                   @click=${(e: Event) => {e.preventDefault ();}}>
            <div class="body">
              <span>${v}</span>
              <span class="name">${l ? 'link' : 'static'}</span>
            </div>
          </m3-card>`))}
      </div>`;
  }
}

// Checkbox unchecked and checked, enabled and disabled.
//
class gallery_checkbox extends LitElement
{
  static override styles = [section_styles, css`
    .column
    {
      display: flex;
      flex-direction: column;
      align-items: flex-start;
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Checkbox</h2>
      <div class="grid">
        <div class="column">
          <m3-checkbox>Remember me</m3-checkbox>
          <m3-checkbox checked>Show offline friends</m3-checkbox>
        </div>
        <div class="column">
          <m3-checkbox disabled>Disabled</m3-checkbox>
          <m3-checkbox disabled checked>Disabled</m3-checkbox>
        </div>
      </div>`;
  }
}

// Button that opens a dialog with a text field, next to the value that the
// dialog closed with.
//
class gallery_dialog extends LitElement
{
  static override properties =
  {
    result_: {state: true}
  };

  static override styles = [section_styles, css`
    .specimen
    {
      display: flex;
      align-items: center;
      gap: 16px;
    }
  `];

  declare private result_: string;

  constructor ()
  {
    super ();
    this.result_ = '';
  }

  override render (): TemplateResult
  {
    return html`
      <h2>Dialog</h2>
      <div class="specimen">
        <m3-button variant="tonal" @click=${this.on_open}>Open</m3-button>
        <span class="name">
          ${this.result_ !== '' ? `closed with ${this.result_}` : 'not closed'}
        </span>
      </div>
      <m3-dialog headline="Add friend" @close=${this.on_close}>
        <span>Enter the name of the player to send a request to.</span>
        <m3-text-field label="Name" autofocus></m3-text-field>
        <m3-button slot="actions" variant="text"
                   @click=${() => {this.dialog ().close ('cancel');}}>
          Cancel
        </m3-button>
        <m3-button slot="actions" variant="text"
                   @click=${() => {this.dialog ().close ('add');}}>
          Add
        </m3-button>
      </m3-dialog>`;
  }

  private dialog (): dialog
  {
    const d = this.renderRoot.querySelector ('m3-dialog');

    if (d === null)
      throw new Error ('no dialog');

    return d;
  }

  private on_open (): void
  {
    this.dialog ().show ();
  }

  private on_close (): void
  {
    const v = this.dialog ().return_value;
    this.result_ = v !== '' ? v : 'dismissal';
  }
}

// Horizontal divider with each inset between two lines of text as well as
// the vertical divider between the items of a row.
//
const divider_insets: readonly string[] = ['none', 'start', 'middle'];

class gallery_divider extends LitElement
{
  static override styles = [section_styles, css`
    .specimen
    {
      padding-block: 8px;
      border-radius: var(--md-sys-shape-corner-medium);
      background: var(--md-sys-color-surface-container);
    }

    .specimen p
    {
      margin: 0;
      padding: 8px 16px;
    }

    .row
    {
      display: flex;
      align-items: center;
      gap: 16px;
      width: max-content;
      height: 56px;
      padding-inline: 16px;
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Divider</h2>
      <div class="grid">
        ${divider_insets.map ((i) => html`
          <div>
            <div class="specimen">
              <p>First</p>
              <m3-divider inset=${i}></m3-divider>
              <p>Second</p>
            </div>
            <span class="name">${i}</span>
          </div>`)}
        <div>
          <div class="specimen row">
            <span>First</span>
            <m3-divider vertical></m3-divider>
            <span>Second</span>
            <m3-divider vertical inset="middle"></m3-divider>
            <span>Third</span>
          </div>
          <span class="name">vertical, none and middle</span>
        </div>
      </div>`;
  }
}

// Filter chips with and without a leading icon plus a disabled chip in each
// state. Clicking a chip toggles it.
//
const filter_chips: readonly {label: string, icon: string}[] =
[
  {label: 'Servers', icon: 'dns'},
  {label: 'Players', icon: 'person'},
  {label: 'Mods',    icon: ''},
  {label: 'Maps',    icon: ''}
];

class gallery_filter_chip extends LitElement
{
  static override styles = [section_styles, css`
    .row
    {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      margin-block-end: 16px;
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Filter chip</h2>
      <div class="row">
        ${filter_chips.map ((c, i) => html`
          <m3-filter-chip icon=${c.icon} ?selected=${i === 0}>
            ${c.label}
          </m3-filter-chip>`)}
      </div>
      <div class="row">
        <m3-filter-chip disabled>Disabled</m3-filter-chip>
        <m3-filter-chip disabled selected>Disabled</m3-filter-chip>
      </div>`;
  }
}

// Icon button variants, each in both sizes, enabled and disabled.
//
const icon_button_variants: readonly string[] =
  ['standard', 'filled', 'tonal', 'outlined'];

class gallery_icon_button extends LitElement
{
  static override styles = [section_styles, css`
    .specimen
    {
      display: flex;
      align-items: center;
      gap: 8px;
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Icon button</h2>
      <div class="grid">
        ${icon_button_variants.map ((v) => html`
          <div>
            <div class="specimen">
              <m3-icon-button variant=${v} icon="home" label="Home">
              </m3-icon-button>
              <m3-icon-button variant=${v} size="extra-small" icon="home"
                              label="Home">
              </m3-icon-button>
              <m3-icon-button variant=${v} icon="home" label="Home" disabled>
              </m3-icon-button>
            </div>
            <span class="name">${v}</span>
          </div>`)}
      </div>`;
  }
}

// List with an item of each kind in the standard and segmented styles. The
// items are plain text, buttons with an avatar and supporting text (one of
// them with an end control), a link with an end label, and a disabled
// button. Choosing a button selects it in both lists.
//
class gallery_list extends LitElement
{
  static override properties =
  {
    selected_: {state: true}
  };

  static override styles = [section_styles, css`
    .specimens
    {
      display: flex;
      flex-wrap: wrap;
      align-items: flex-start;
      gap: 16px;
    }

    m3-list
    {
      flex: 1;
      max-width: 360px;
      border-radius: var(--md-sys-shape-corner-large);
      background: var(--md-sys-color-surface-container-low);
      --m3-avatar-surface: var(--md-sys-color-surface-container-low);
    }

    /*
     * The segments sit on a container of another tone, which shows between
     * them.
     */
    m3-list[segmented]
    {
      background: var(--md-sys-color-surface-container);
      --m3-avatar-surface: var(--md-sys-color-surface);
    }
  `];

  declare private selected_: string;

  constructor ()
  {
    super ();
    this.selected_ = 'kestrel';
  }

  override render (): TemplateResult
  {
    const items = (segmented: boolean): TemplateResult => html`
      <m3-list-item>One line of text</m3-list-item>
      ${segmented ? nothing : html`<m3-divider></m3-divider>`}
      <m3-list-item action ?selected=${this.selected_ === 'kestrel'}
                    @click=${() => {this.selected_ = 'kestrel';}}>
        <m3-avatar slot="start" name="Kestrel" presence="active">
        </m3-avatar>
        Kestrel
        <span slot="supporting">Terminal, Search and Destroy</span>
      </m3-list-item>
      <m3-list-item action ?selected=${this.selected_ === 'vega'}
                    @click=${() => {this.selected_ = 'vega';}}>
        <m3-avatar slot="start" name="Vega" presence="online"></m3-avatar>
        Vega
        <span slot="supporting">Online</span>
        <m3-icon-button slot="end" icon="chat" label="Chat"
                        @click=${(e: Event) => {e.stopPropagation ();}}>
        </m3-icon-button>
      </m3-list-item>
      <m3-list-item href="#"
                    @click=${(e: Event) => {e.preventDefault ();}}>
        <m3-icon slot="start">dns</m3-icon>
        A link with an end label
        <span slot="end">18/18</span>
      </m3-list-item>
      <m3-list-item action disabled>
        <m3-avatar slot="start" name="Marlow" presence="offline">
        </m3-avatar>
        Disabled
        <span slot="supporting">Last online 2 days ago</span>
      </m3-list-item>`;

    return html`
      <h2>List</h2>
      <div class="specimens">
        <m3-list label="Standard">${items (false)}</m3-list>
        <m3-list label="Segmented" segmented>${items (true)}</m3-list>
      </div>`;
  }
}

// Destinations of the rail specimens.
//
const rail_destinations: readonly {icon: string, label: string}[] =
[
  {icon: 'home',      label: 'Home'},
  {icon: 'search',    label: 'Search'},
  {icon: 'dns',       label: 'Servers'},
  {icon: 'grid_view', label: 'Library'}
];

// Navigation rail collapsed and expanded, each with a menu button in the
// header that toggles it. Choosing an item selects it in its rail.
//
class gallery_navigation_rail extends LitElement
{
  static override styles = [section_styles, css`
    .specimens
    {
      display: flex;
      gap: 16px;
    }

    m3-navigation-rail
    {
      height: 420px;
      border: 1px solid var(--md-sys-color-outline-variant);
      border-radius: var(--md-sys-shape-corner-medium);
      --m3-navigation-rail-container-color:
        var(--md-sys-color-surface-container-low);
    }

    /*
     * The icon of the menu button lines up with the icons of the items,
     * whose centers are 48dp from the start in both layouts.
     */
    m3-icon-button
    {
      margin-inline: 28px;
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Navigation rail</h2>
      <div class="specimens" @click=${this.select}>
        ${[false, true].map ((x) => html`
          <m3-navigation-rail label="Specimen" ?expanded=${x}>
            <m3-icon-button slot="header" icon=${x ? 'menu_open' : 'menu'}
                            label="Toggle" @click=${this.toggle}>
            </m3-icon-button>
            ${rail_destinations.map ((d, i) => html`
              <m3-navigation-rail-item icon=${d.icon} label=${d.label}
                                       href="#" ?selected=${i === 0}>
              </m3-navigation-rail-item>`)}
          </m3-navigation-rail>`)}
      </div>`;
  }

  // Expand or collapse the rail of the menu button and update the button's
  // icon to show what the next click does.
  //
  private toggle (e: Event): void
  {
    const b = e.currentTarget as icon_button;
    const r = b.closest ('m3-navigation-rail');

    if (r === null)
      return;

    r.expanded = !r.expanded;
    b.icon = r.expanded ? 'menu_open' : 'menu';
  }

  // Select the chosen item in its rail. Note that we prevent the navigation
  // so that the page stays where it is.
  //
  private select (e: Event): void
  {
    for (const t of e.composedPath ())
    {
      if (!(t instanceof navigation_rail_item))
        continue;

      e.preventDefault ();

      for (const x of t.parentElement?.children ?? [])
      {
        if (x instanceof navigation_rail_item)
          x.selected = x === t;
      }

      break;
    }
  }
}

// Button that opens a search view whose list shows the names that contain
// the query.
//
const search_names: readonly string[] =
  ['Terminal', 'Highrise', 'Rust', 'Favela', 'Quarry', 'Karachi'];

class gallery_search_view extends LitElement
{
  static override properties =
  {
    query_: {state: true}
  };

  static override styles = [section_styles, css`
    .chips
    {
      display: flex;
      gap: 8px;
      padding: 16px 16px 8px;
    }
  `];

  declare private query_: string;

  constructor ()
  {
    super ();
    this.query_ = '';
  }

  override render (): TemplateResult
  {
    const q = this.query_.toLocaleLowerCase ();

    return html`
      <h2>Search view</h2>
      <m3-button variant="tonal" @click=${this.on_open}>Open</m3-button>
      <m3-search-view placeholder="Search maps" .value=${this.query_}
                      @input=${this.on_input}>
        <div class="chips">
          <m3-filter-chip selected>Maps</m3-filter-chip>
          <m3-filter-chip>Servers</m3-filter-chip>
        </div>
        <m3-list label="Results">
          ${search_names.filter ((n) => n.toLocaleLowerCase ().includes (q))
                        .map ((n) => html`
            <m3-list-item action>
              <m3-icon slot="start">map</m3-icon>
              ${n}
            </m3-list-item>`)}
        </m3-list>
      </m3-search-view>`;
  }

  private on_open (): void
  {
    this.renderRoot.querySelector ('m3-search-view')?.show ();
  }

  private on_input (e: Event): void
  {
    this.query_ = (e.currentTarget as search_view).value;
  }
}

// Button that opens a side sheet with a profile, a list, and actions.
//
const side_sheet_servers: readonly string[] =
  ['IW4x Official #1', 'Domination Nights', 'Rust Only 24/7'];

class gallery_side_sheet extends LitElement
{
  static override styles = [section_styles, css`
    .profile
    {
      display: flex;
      align-items: center;
      gap: 16px;
      padding-block-end: 16px;
      --m3-avatar-surface: var(--md-sys-color-surface-container-low);
    }

    m3-list
    {
      margin-inline: -24px;
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Side sheet</h2>
      <m3-button variant="tonal" @click=${this.on_open}>Open</m3-button>
      <m3-side-sheet headline="Kestrel">
        <div class="profile">
          <m3-avatar size="large" name="Kestrel" presence="active"></m3-avatar>
          <span>Terminal, Search and Destroy</span>
        </div>
        <m3-divider></m3-divider>
        <m3-list label="Recent">
          ${side_sheet_servers.map ((n) => html`
            <m3-list-item>
              <m3-icon slot="start">dns</m3-icon>
              ${n}
            </m3-list-item>`)}
        </m3-list>
        <m3-button slot="actions">Join</m3-button>
        <m3-button slot="actions" variant="outlined">Message</m3-button>
      </m3-side-sheet>`;
  }

  private on_open (): void
  {
    this.renderRoot.querySelector ('m3-side-sheet')?.show ();
  }
}

// Switch off and on, enabled and disabled, as rows of a settings list.
//
class gallery_switch extends LitElement
{
  static override styles = [section_styles, css`
    .column
    {
      display: flex;
      flex-direction: column;
      gap: 16px;
      max-width: 360px;
    }
  `];

  override render (): TemplateResult
  {
    return html`
      <h2>Switch</h2>
      <div class="column">
        <m3-switch selected>Friend activity</m3-switch>
        <m3-switch>New releases</m3-switch>
        <m3-switch disabled>Disabled</m3-switch>
        <m3-switch disabled selected>Disabled</m3-switch>
      </div>`;
  }
}

// Sections of the tabs specimen.
//
const tab_sections: readonly {name: string, label: string}[] =
[
  {name: 'general',  label: 'General'},
  {name: 'news',     label: 'News'},
  {name: 'discover', label: 'Discover'}
];

// Primary tabs over a panel that names the selected section.
//
class gallery_tabs extends LitElement
{
  static override properties = {selected_: {state: true}};

  static override styles = [section_styles, css`
    [role='tabpanel']
    {
      padding: 16px;
    }
  `];

  declare private selected_: string;

  constructor ()
  {
    super ();
    this.selected_ = 'general';
  }

  override render (): TemplateResult
  {
    const c = tab_sections.find ((t) => t.name === this.selected_);

    return html`
      <h2>Tabs</h2>
      <m3-tabs label="Specimen" selected=${this.selected_}
               @change=${this.on_change}>
        ${tab_sections.map ((t) => html`
          <m3-tab name=${t.name} id="tab-${t.name}">${t.label}</m3-tab>`)}
      </m3-tabs>
      <div role="tabpanel" aria-labelledby="tab-${this.selected_}">
        <span class="name">${c?.label ?? ''} section</span>
      </div>`;
  }

  private on_change (e: Event): void
  {
    this.selected_ = (e.currentTarget as tabs).selected;
  }
}

// Text field variants.
//
const text_field_variants: readonly string[] = ['filled', 'outlined'];

// Each text field variant empty, filled in, with supporting text, in the
// error state, without a label (as a search field with a clear button), and
// disabled.
//
class gallery_text_field extends LitElement
{
  static override properties =
  {
    query_: {state: true}
  };

  static override styles = [section_styles, css`
    .grid
    {
      grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
      align-items: start;
      margin-block-end: 24px;
    }
  `];

  declare private query_: string;

  constructor ()
  {
    super ();
    this.query_ = 'Kestrel';
  }

  override render (): TemplateResult
  {
    return html`
      <h2>Text field</h2>
      ${text_field_variants.map ((v) => html`
        <div class="grid">
          <m3-text-field variant=${v} label="Name"></m3-text-field>
          <m3-text-field variant=${v} label="Name" value="Kestrel">
          </m3-text-field>
          <m3-text-field variant=${v} label="Email" type="email"
                         icon="mail" supporting="We never share it">
          </m3-text-field>
          <m3-text-field variant=${v} label="Name" value="K"
                         error="Use at least 3 characters">
          </m3-text-field>
          <m3-text-field variant=${v} type="search" icon="search"
                         placeholder="Search friends" .value=${this.query_}
                         @input=${this.on_input}>
            ${this.query_ !== ''
              ? html`
                <m3-icon-button slot="end" icon="close" label="Clear"
                                @click=${() => {this.query_ = '';}}>
                </m3-icon-button>`
              : ''}
          </m3-text-field>
          <m3-text-field variant=${v} label="Name" value="Kestrel" disabled>
          </m3-text-field>
        </div>`)}`;
  }

  private on_input (e: Event): void
  {
    this.query_ = (e.currentTarget as text_field).value;
  }
}

customElements.define ('m3-gallery-icon', gallery_icon);
customElements.define ('m3-gallery-interaction', gallery_interaction);
customElements.define ('m3-gallery-avatar', gallery_avatar);
customElements.define ('m3-gallery-badge', gallery_badge);
customElements.define ('m3-gallery-button', gallery_button);
customElements.define ('m3-gallery-card', gallery_card);
customElements.define ('m3-gallery-checkbox', gallery_checkbox);
customElements.define ('m3-gallery-dialog', gallery_dialog);
customElements.define ('m3-gallery-divider', gallery_divider);
customElements.define ('m3-gallery-filter-chip', gallery_filter_chip);
customElements.define ('m3-gallery-icon-button', gallery_icon_button);
customElements.define ('m3-gallery-list', gallery_list);
customElements.define ('m3-gallery-navigation-rail', gallery_navigation_rail);
customElements.define ('m3-gallery-search-view', gallery_search_view);
customElements.define ('m3-gallery-side-sheet', gallery_side_sheet);
customElements.define ('m3-gallery-switch', gallery_switch);
customElements.define ('m3-gallery-tabs', gallery_tabs);
customElements.define ('m3-gallery-text-field', gallery_text_field);

declare global
{
  interface HTMLElementTagNameMap
  {
    'm3-gallery-icon': gallery_icon;
    'm3-gallery-interaction': gallery_interaction;
    'm3-gallery-avatar': gallery_avatar;
    'm3-gallery-badge': gallery_badge;
    'm3-gallery-button': gallery_button;
    'm3-gallery-card': gallery_card;
    'm3-gallery-checkbox': gallery_checkbox;
    'm3-gallery-dialog': gallery_dialog;
    'm3-gallery-divider': gallery_divider;
    'm3-gallery-filter-chip': gallery_filter_chip;
    'm3-gallery-icon-button': gallery_icon_button;
    'm3-gallery-list': gallery_list;
    'm3-gallery-navigation-rail': gallery_navigation_rail;
    'm3-gallery-search-view': gallery_search_view;
    'm3-gallery-side-sheet': gallery_side_sheet;
    'm3-gallery-switch': gallery_switch;
    'm3-gallery-tabs': gallery_tabs;
    'm3-gallery-text-field': gallery_text_field;
  }
}
