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

// Numeric application version for the executable metadata (see buildfile).
//
const app_version = process.env['launcher_electron_app_version'];

if (app_version === undefined)
  throw new Error (
    'launcher_electron_app_version environment variable is not set');

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
    appVersion: app_version,
    buildVersion: app_version,
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

    // Only put the bundles and package.json (which names the main process
    // entry) into the application archive.
    //
    ignore: (p: string) => p !== '' &&
                           p !== '/package.json' &&
                           p !== '/dist' &&
                           !p.startsWith ('/dist/'),

    // Keep the addon outside of the application archive since Electron
    // would have to extract it into a temporary file to load it from there
    // (see electron/main/main.ts). Note that the third-party licenses go
    // next to the archive as well (see buildfile).
    //
    extraResource: ['launcher.node', 'licenses']
  },

  plugins:
  [
    // Restrict the packaged application to its own integrity-checked
    // archive and ignore the NODE_OPTIONS environment variable as well as the
    // inspector arguments on the command line. Note that we also disable
    // running as Node.js and encrypt cookies at rest.
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
