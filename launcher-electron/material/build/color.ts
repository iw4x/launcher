// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { argbFromHex,
         Hct,
         hexFromArgb,
         MaterialDynamicColors,
         SchemeTonalSpot } from '@material/material-color-utilities';

// Source color of the scheme, the IW4x accent (lime 400 of the website).
//
export const source_color = '#a3e635';

// Return the dark tonal spot scheme of the source color as of the 2025 (M3
// Expressive) color specification, with the standard contrast level.
//
function
dark_scheme (): SchemeTonalSpot
{
  return new SchemeTonalSpot (Hct.fromInt (argbFromHex (source_color)),
                              true /* is_dark */,
                              0 /* contrast_level */,
                              '2025');
}

// Return the color tokens stylesheet, one custom property per color role.
//
// Note that the palette key colors are inputs of the scheme (and not roles)
// so we leave them out. The library's list of all colors omits the shadow
// and scrim roles, so we add them.
//
export function
color_css (): string
{
  const s = dark_scheme ();
  const m = new MaterialDynamicColors ();

  let r = ':root\n{\n  color-scheme: dark;\n';

  for (const c of [...m.allColors, m.shadow (), m.scrim ()])
  {
    if (c.name.endsWith ('_palette_key_color'))
      continue;

    const n = c.name.replaceAll ('_', '-');
    r += `  --md-sys-color-${n}: ${hexFromArgb (c.getArgb (s))};\n`;
  }

  return r + '}\n';
}

// Return the module with the colors that the main process needs, since it
// cannot read the stylesheet. Currently this is the surface role, which the
// window shows until the renderer paints.
//
export function
color_ts (): string
{
  const s = dark_scheme ();
  const m = new MaterialDynamicColors ();

  const surface = hexFromArgb (m.surface ().getArgb (s));

  return `export const surface = '${surface}';\n`;
}
