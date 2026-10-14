// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { defineConfig } from 'vite';

// Note that a sandboxed preload script is loaded as CommonJS and can only
// require a few modules (electron among them), thus the bundle.
//
export default defineConfig (
{
  build:
  {
    outDir: 'dist/preload',
    emptyOutDir: true,
    target: 'node24',
    minify: false,
    lib:
    {
      entry: 'electron/preload/preload.ts',
      formats: ['cjs'],
      fileName: () => 'preload.cjs'
    },
    rolldownOptions:
    {
      external: ['electron']
    }
  }
});
