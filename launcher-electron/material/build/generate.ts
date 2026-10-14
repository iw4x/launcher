// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

import { color_css, color_ts } from './color';
import { motion_css } from './motion';

// Usage: generate <dir>
//
// Write the tokens that we compute at build time (color and motion) into
// <dir>, from where the library imports them (see ../index.ts).
//
function
main (argv: readonly string[]): number
{
  const d = argv[2];

  if (d === undefined)
  {
    console.error ('error: output directory expected\n' +
                   '  info: run \'generate <dir>\'');
    return 1;
  }

  mkdirSync (d, {recursive: true});
  writeFileSync (join (d, 'color.css'), color_css ());
  writeFileSync (join (d, 'color.ts'), color_ts ());
  writeFileSync (join (d, 'motion.css'), motion_css ());

  return 0;
}

process.exitCode = main (process.argv);
