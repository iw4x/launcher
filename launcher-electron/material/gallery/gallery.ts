// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

// Component gallery, a development page that shows the library's tokens and
// components.
//
import '../index';
import './tokens';
import './components';

// Switch the page between the dark scheme (the default, see
// ../build/color.ts) and the light scheme. Then label the button with the
// other scheme.
//
function
toggle_scheme (b: HTMLButtonElement): void
{
  const r = document.documentElement;
  const light = r.dataset['colorScheme'] !== 'light';

  if (light)
    r.dataset['colorScheme'] = 'light';
  else
    delete r.dataset['colorScheme'];

  b.textContent = light ? 'Dark scheme' : 'Light scheme';
}

const b = document.querySelector<HTMLButtonElement> ('#scheme');

if (b !== null)
  b.addEventListener ('click', () => {toggle_scheme (b);});
