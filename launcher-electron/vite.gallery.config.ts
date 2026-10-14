// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { resolve } from 'node:path';

import { defineConfig } from 'vite';

// Bundle the component gallery the same way as the renderer (see
// vite.renderer.config.ts).
//
export default defineConfig (
{
  root: resolve (import.meta.dirname, 'material/gallery'),
  base: './',
  build:
  {
    outDir: resolve (import.meta.dirname, 'dist/gallery'),
    emptyOutDir: true,
    target: 'chrome152'
  }
});
