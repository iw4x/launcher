// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import type { launcher_api } from '../preload/launcher.js';

declare global
{
  interface Window
  {
    launcher: launcher_api;
  }
}
