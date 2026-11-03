# launcher - Launcher and updater for IW4x.

The `iw4x-launcher` executable keeps the IW4x installation in its directory
up to date with the latest IW4x release and starts the selected IW4x client,
through Proton on Linux. It also installs shortcuts for both clients, in
Steam and on the desktop.

This file contains setup instructions and other details for developing
`launcher`. If you want to use `launcher` in your `build2`-based project,
then see the accompanying package [`README.md`](launcher/README.md) file.

## Usage

The launcher requires the Steam version of Call of Duty: Modern Warfare 2
(2009). Download the archive for your platform from the
[releases](https://github.com/iw4x/launcher/releases) page, extract it into
the game directory, and run `iw4x-launcher` from there.

## Development

The development setup for `launcher` uses the standard `bdep`-based
workflow. Note that development builds regenerate the files kept in
`launcher/launcher/pregenerated/`. For this `bdep` builds the `cli` and
`odb` compilers automatically and `xxd` comes from the system.

On Linux we build with GCC. For example:

```
git clone https://github.com/iw4x/launcher.git
cd launcher

bdep init -C @gcc cc config.cxx=g++
bdep update
bdep test
```

We can also cross-compile for Windows with MinGW-w64 GCC and run the tests
under Wine, with `WINEPATH` pointing to the directory that contains the
MinGW runtime DLLs (`/usr/x86_64-w64-mingw32/sys-root/mingw/bin` on Fedora).
For example:

```
bdep init -C @mingw cc config.cxx=x86_64-w64-mingw32-g++ \
  config.test.runner="env WINEPATH=/usr/x86_64-w64-mingw32/sys-root/mingw/bin wine"
bdep update @mingw
bdep test @mingw
```

On Windows we build with MinGW-w64 GCC from PowerShell. For example:

```
git clone https://github.com/iw4x/launcher.git
cd launcher

bdep init -C -@mingw cc config.cxx=g++
bdep update
bdep test
```

## Contributing

See [CONTRIBUTING](.github/CONTRIBUTING.md).

## License

`launcher` is licensed under the GNU General Public License, version 3 or
any later version.

See [LICENSE](LICENSE.md), [LEGAL](LEGAL), and [AUTHORS](AUTHORS).
