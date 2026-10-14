// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

// Launcher interface that the preload script exposes to the renderer as
// window.launcher. Each function sends a request to the main process over
// the IPC channel of the same name (prefixed with launcher:).
//
export interface launcher_api
{
  // Return the launcher version.
  //
  version (): Promise<string>;
}
