// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { builtinModules } from 'node:module';

import { defineConfig } from 'vite';

// Electron and the Node.js built-in modules come from the runtime (Node.js 24
// for Electron 44).
//
export default defineConfig (
{
  build:
  {
    outDir: 'dist/main',
    emptyOutDir: true,
    target: 'node24',
    minify: false,
    lib:
    {
      entry: 'electron/main/main.ts',
      formats: ['es'],
      fileName: () => 'main.js'
    },
    rolldownOptions:
    {
      external: ['electron',
                 ...builtinModules,
                 ...builtinModules.map ((m) => `node:${m}`)]
    }
  }
});
