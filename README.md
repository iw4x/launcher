# launcher - Launcher and updater for IW4x.

`launcher` is the launcher and updater of IW4x. It runs on players' computers
and on headless servers. It is in early development, and in this version it
prints its usage and version.

The repository holds one package, [`launcher`](launcher/README.md), the
launcher executable.

## Usage

`launcher` has no releases yet. Build it from source as the Development
section describes.

## Development

The development setup uses the standard `bdep`-based workflow and needs a C++
compiler whose standard library provides `std::print()` from C++23. On Linux,
with GCC 14 or later:

```
git clone https://github.com/iw4x/launcher.git
cd launcher

bdep init -C @gcc cc config.cxx=g++ --create-host-config
bdep update
bdep test
```

The command line parser of `launcher` is generated from
`launcher/launcher/launcher.cli` by
[CLI](https://codesynthesis.com/projects/cli/). In this setup `bdep` builds
`cli` in the host configuration that `--create-host-config` creates, and each
build updates the copy of the generated parser kept in
`launcher/launcher/pregenerated/`. Consumers of the package build that copy,
so only the development build requires `cli`.

`launcher` is also built and tested for Windows. From Linux, add a MinGW-w64
GCC configuration that runs the tests under Wine. Wine has to find the MinGW
runtime DLLs, so the test runner adds their directory to `WINEPATH` (here, as
on Fedora):

```
bdep init -C @mingw cc config.cxx=x86_64-w64-mingw32-g++ \
  --create-host-config                                    \
  config.test.runner="env WINEPATH=/usr/x86_64-w64-mingw32/sys-root/mingw/bin wine"
bdep update @mingw
bdep test @mingw
```

On Windows, with MSVC, from the Visual Studio "x64 Native Tools Command
Prompt", which builds 64-bit binaries:

```
git clone https://github.com/iw4x/launcher.git
cd launcher

bdep init -C @msvc cc config.cxx=cl --create-host-config
bdep update
bdep test
```

In PowerShell, a leading `@` has a special meaning (splatting), so write the
configuration name as `-@msvc`, which `bdep` also accepts:

```
bdep init -C -@msvc cc config.cxx=cl --create-host-config
```

## Contributing

See [CONTRIBUTING](.github/CONTRIBUTING.md).

## License

launcher is licensed under the GNU General Public License, version 3,
subject to the additional permissions described in version 1.1 of the IW4x
Linking Exception.

See LICENSE.md, LICENSE-EXCEPTION.md, and AUTHORS.
