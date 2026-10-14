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

// Whether the bundle includes the component gallery (see
// vite.main.config.ts).
//
declare const launcher_gallery: boolean;

// Load the addon. Note that the packaging step places it into the resources
// directory, next to the application archive (see forge.config.ts).
//
const launcher = createRequire (import.meta.url) (
  join (process.resourcesPath, 'launcher.node')) as addon;

// Create the main window and load the launcher or, if the bundle includes
// the component gallery and we were started with --gallery, the gallery.
//
// The window is frameless and the renderer draws the title bar (see
// renderer.css). We keep the window hidden until the renderer has painted so
// that we don't show a blank frame on startup. Note also that the window
// background is the surface role (see material/build/color.ts) so that any
// area the renderer hasn't painted yet matches the page.
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

  const page = launcher_gallery && process.argv.includes ('--gallery')
    ? '../gallery/index.html'
    : '../renderer/index.html';

  void w.loadFile (join (import.meta.dirname, page));
}

// Only run one instance. If we are the second, then exit and let the
// running instance bring its window forward.
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

  // Serve the launcher interface (see preload/launcher.ts).
  //
  ipcMain.handle ('launcher:version',
                  (): ReturnType<launcher_api['version']> =>
                    Promise.resolve (launcher.version ()));

  app.on ('window-all-closed', () => {app.quit ();});

  void app.whenReady ().then (create_window);
}
