// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { resolve } from 'node:path';

import { defineConfig } from 'vite';

// The window loads the renderer from the file system, thus the relative
// asset references. Note that Electron 44 embeds Chromium 152.
//
export default defineConfig (
{
  root: resolve (import.meta.dirname, 'electron/renderer'),
  base: './',
  build:
  {
    outDir: resolve (import.meta.dirname, 'dist/renderer'),
    emptyOutDir: true,
    target: 'chrome152'
  }
});
