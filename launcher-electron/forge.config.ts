// Copyright (c) the IW4x authors (see the AUTHORS file).
// SPDX-License-Identifier: GPL-3.0-only WITH AdditionRef-IW4x-Exception-1.1

import { createRequire } from 'node:module';

import { FusesPlugin } from '@electron-forge/plugin-fuses';
import { FuseV1Options, FuseVersion } from '@electron/fuses';

import type { ForgeConfig } from '@electron-forge/shared-types';

// Electron download cache that the dependency installation step fills (see
// buildfile).
//
const cache = process.env['electron_config_cache'];

if (cache === undefined)
  throw new Error ('electron_config_cache environment variable is not set');

// Note that ../out/ is out/ in our output directory (see buildfile).
//
const config: ForgeConfig =
{
  outDir: '../out',

  packagerConfig:
  {
    name: 'IW4x Launcher',
    executableName: 'iw4x-launcher',
    appBundleId: 'io.iw4x.launcher',
    asar: true,

    // The packaging step has no network access, so take the Electron
    // archive from the cache and verify it against the checksums that the
    // electron package ships.
    //
    download:
    {
      cacheRoot: cache,
      checksums: createRequire (import.meta.url) (
        'electron/checksums.json') as Record<string, string>
    },

    // Only the bundles and package.json (which names the main process
    // entry) go into the application archive.
    //
    ignore: (p: string) => p !== '' &&
                           p !== '/package.json' &&
                           p !== '/dist' &&
                           !p.startsWith ('/dist/'),

    // Keep the addon outside of the application archive since Electron
    // would otherwise extract it into a temporary file to load it (see
    // electron/main/main.ts).
    //
    extraResource: ['launcher.node']
  },

  plugins:
  [
    // Restrict the packaged application to its own integrity-checked
    // archive. Note that it also rejects Node.js options from the
    // environment and the command line.
    //
    new FusesPlugin (
    {
      version: FuseVersion.V1,
      [FuseV1Options.RunAsNode]: false,
      [FuseV1Options.EnableCookieEncryption]: true,
      [FuseV1Options.EnableNodeOptionsEnvironmentVariable]: false,
      [FuseV1Options.EnableNodeCliInspectArguments]: false,
      [FuseV1Options.EnableEmbeddedAsarIntegrityValidation]: true,
      [FuseV1Options.OnlyLoadAppFromAsar]: true
    })
  ]
};

export default config;
