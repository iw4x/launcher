// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { argbFromHex,
         Hct,
         hexFromArgb,
         MaterialDynamicColors,
         SchemeTonalSpot } from '@material/material-color-utilities';

// Source color of the schemes, which is the IW4x accent (lime 400 of the
// website).
//
export const source_color = '#a3e635';

// Return the tonal spot scheme of the source color as of the 2025 (M3
// Expressive) color specification, with the standard contrast level.
//
function
scheme (dark: boolean): SchemeTonalSpot
{
  return new SchemeTonalSpot (Hct.fromInt (argbFromHex (source_color)),
                              dark,
                              0 /* contrast_level */,
                              '2025');
}

// Return the declarations of the scheme, one custom property per color role.
//
// Note that the library's list of all colors omits the shadow and scrim
// roles, so we add them.
//
function
declarations (dark: boolean): string
{
  const s = scheme (dark);
  const m = new MaterialDynamicColors ();

  const r: string[] = [];

  for (const c of [...m.allColors, m.shadow (), m.scrim ()])
  {
    const n = c.name.replaceAll ('_', '-');
    r.push (`  --md-sys-color-${n}: ${hexFromArgb (c.getArgb (s))};`);
  }

  return r.join ('\n');
}

// Return the color tokens stylesheet. The dark scheme is the default and the
// light scheme applies if the root element has data-color-scheme='light'.
//
export function
color_css (): string
{
  return `:root
{
  color-scheme: dark;
${declarations (true)}
}

:root[data-color-scheme='light']
{
  color-scheme: light;
${declarations (false)}
}
`;
}

// Return the module with the colors that the main process needs since it
// cannot read the stylesheet. Currently, this is the surface role of the dark
// scheme for the window background (see electron/main/main.ts).
//
export function
color_ts (): string
{
  const s = scheme (true);
  const m = new MaterialDynamicColors ();

  const surface = hexFromArgb (m.surface ().getArgb (s));

  return `export const surface = '${surface}';\n`;
}
