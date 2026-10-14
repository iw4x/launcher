// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { contextBridge, ipcRenderer } from 'electron';

import type { launcher_api } from './launcher.js';

const launcher: launcher_api =
{
  version: () => ipcRenderer.invoke ('launcher:version') as Promise<string>
};

contextBridge.exposeInMainWorld ('launcher', launcher);
