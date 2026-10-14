// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

// Material 3 Expressive library of the launcher. Importing it applies the
// system tokens to the document as custom properties named after the
// specification and defines the components as custom elements with the m3-
// prefix.
//
// Note that the generated/ stylesheets are computed at build time by
// build/generate.ts and only exist in the build's stage directory.
//
import '@fontsource-variable/inter';
import '@material-symbols/font-400/rounded.css';

import './generated/color.css';
import './tokens/typescale.css';
import './tokens/shape.css';
import './tokens/elevation.css';
import './tokens/state.css';
import './tokens/density.css';
import './generated/motion.css';

export { avatar } from './avatar';
export { badge } from './badge';
export { button } from './button';
export { card } from './card';
export { checkbox } from './checkbox';
export { dialog } from './dialog';
export { divider } from './divider';
export { filter_chip } from './filter-chip';
export { focus_ring } from './focus-ring';
export { icon } from './icon';
export { icon_button } from './icon-button';
export { list, list_item } from './list';
export { navigation_rail, navigation_rail_item } from './navigation-rail';
export { ripple } from './ripple';
export { search_view } from './search-view';
export { side_sheet } from './side-sheet';
export { switch_ } from './switch';
export { tab, tabs } from './tabs';
export { text_field } from './text-field';
