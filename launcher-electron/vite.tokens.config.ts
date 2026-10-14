// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { builtinModules } from 'node:module';

import { defineConfig } from 'vite';

// Bundle the token generator together with its dependencies into
// tools/generate.js. Note that the material color utilities import their own
// modules in a form that only a bundler resolves, thus the bundle.
//
export default defineConfig (
{
  build:
  {
    outDir: 'tools',
    emptyOutDir: true,
    target: 'node24',
    minify: false,
    lib:
    {
      entry: 'material/build/generate.ts',
      formats: ['es'],
      fileName: () => 'generate.js'
    },
    rolldownOptions:
    {
      external: [...builtinModules,
                 ...builtinModules.map ((m) => `node:${m}`)]
    }
  }
});
