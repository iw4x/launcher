// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { app, BrowserWindow, ipcMain } from 'electron';
import { createRequire } from 'node:module';
import { join } from 'node:path';

import { surface } from '../../material/generated/color.js';

import type { launcher_api } from '../preload/launcher.js';

// Launcher addon interface (see launcher/node/addon.cxx).
//
interface addon
{
  version (): string;
}

// Note that the packaging step places the addon into the resources
// directory, next to the application archive (see forge.config.ts).
//
const launcher = createRequire (import.meta.url) (
  join (process.resourcesPath, 'launcher.node')) as addon;

// Chromium's Wayland platform has no Vulkan support, which WebGPU uses by
// default on Linux. So have WebGPU use OpenGL ES there, the same as the
// compositor.
//
if (process.platform === 'linux' &&
    process.env['WAYLAND_DISPLAY'] !== undefined)
  app.commandLine.appendSwitch ('use-webgpu-adapter', 'opengles');

// Keep the window hidden until the renderer has painted, which avoids
// showing a blank frame on startup. Note that the window background is the
// surface role (see material/build/color.ts), which it shows when resized
// faster than the renderer paints.
//
// The window is frameless with the renderer drawing its title bar (see
// renderer.css).
//
function
create_window (): void
{
  const w = new BrowserWindow (
  {
    width: 1280,
    height: 800,
    minWidth: 960,
    minHeight: 600,
    frame: false,
    show: false,
    backgroundColor: surface,
    webPreferences:
    {
      preload: join (import.meta.dirname, '../preload/preload.cjs'),
      contextIsolation: true,
      sandbox: true,
      nodeIntegration: false
    }
  });

  w.once ('ready-to-show', () => {w.show ();});
  void w.loadFile (join (import.meta.dirname, '../renderer/index.html'));
}

// Only run one instance. On a second start, bring the window of the running
// instance forward and exit.
//
if (!app.requestSingleInstanceLock ())
{
  app.quit ();
}
else
{
  app.on ('second-instance', () =>
  {
    const w = BrowserWindow.getAllWindows ()[0];

    if (w !== undefined)
    {
      if (w.isMinimized ())
        w.restore ();

      w.focus ();
    }
  });

  ipcMain.handle ('launcher:version',
                  (): ReturnType<launcher_api['version']> =>
                    Promise.resolve (launcher.version ()));

  app.on ('window-all-closed', () => {app.quit ();});

  void app.whenReady ().then (create_window);
}
