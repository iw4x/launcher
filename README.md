# launcher - Launcher and updater for IW4x.

`launcher` is the launcher and updater of IW4x. It runs on players' computers
and on headless servers. It is in early development, and in this version it
prints its usage and version.

The repository holds two packages: [`launcher`](launcher/README.md), the
launcher executable, and [`launcher-electron`](launcher-electron/README.md),
the desktop application that loads it as a Node.js native addon.

## Usage

`launcher` has no releases yet. Build it from source as the Development
section describes.

## Development

The development setup for `launcher` uses the standard `bdep`-based workflow
and needs a C++23 compiler that provides `std::print()`, for example, GCC 14:

```
git clone https://github.com/iw4x/launcher.git
cd launcher

bdep init -C @gcc cc config.cxx=g++ --create-host-config
bdep update
bdep test
```

To also build for Windows with MinGW-w64 GCC, add a configuration that runs
the tests under Wine (which finds the MinGW runtime DLLs through `WINEPATH`,
here as on Fedora):

```
bdep init -C @mingw cc config.cxx=x86_64-w64-mingw32-g++ \
  --create-host-config                                    \
  config.test.runner="env WINEPATH=/usr/x86_64-w64-mingw32/sys-root/mingw/bin wine"
bdep update @mingw
bdep test @mingw
```

On Windows with MSVC, use the "x64 Native Tools Command Prompt" (in
PowerShell, write the configuration name as `-@msvc`):

```
bdep init -C @msvc cc config.cxx=cl --create-host-config
bdep update
bdep test
```

Note that `launcher-electron` builds in a [Podman](https://podman.io/)
container and needs network access to install its npm dependencies. The
packaged application is written to `launcher-electron/out/` in the build
configuration.

## Contributing

See [CONTRIBUTING](.github/CONTRIBUTING.md).

## License

launcher is licensed under the GNU General Public License, version 3,
subject to the additional permissions described in version 1.1 of the IW4x
Linking Exception.

See LICENSE.md, LICENSE-EXCEPTION.md, and AUTHORS.
