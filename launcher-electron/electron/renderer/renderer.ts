// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import '../../material/index';

// Show the launcher version on the page.
//
async function
show_version (): Promise<void>
{
  const e = document.querySelector<HTMLElement> ('#version');

  if (e !== null)
    e.textContent = `launcher ${await window.launcher.version ()}`;
}

void show_version ();
