# IW4x Launcher

<img width="3194" height="1744" alt="image" src="https://github.com/user-attachments/assets/592124b1-5b55-4172-82e6-3f72e147df69" />

## Getting Started

IW4x launcher is built using the [build2](https://build2.org/build2-toolchain/doc/build2-toolchain-intro.xhtml#preface) build system. To get started, you'll need the staged toolchain, which you can grab as either source or binary packages. The pre-built binaries are available here:

* [Staged Toolchain](https://stage.build2.org/0/0.18.0-a.0/bindist/)
* [SHA256 Checksums](https://stage.build2.org/0/toolchain-bindist.sha256)

You'll also need a working GCC compiler. Use GCC if you're compiling on Linux, or MinGW if you're on Windows. If you need help setting these up, you can find installation instructions below:

* [GCC](https://gcc.gnu.org/)
* [MinGW-w64](https://www.mingw-w64.org/)

### Consumption

> [!NOTE]
> **Consumption** is just `build2`'s term for building the package without actively modifying the code. These steps are great if you want to test the latest development builds, but we don't recommend them for regular users just looking to install and play the game.

#### Windows

```powershell
# Create the build configuration:
bpkg create -d launcher cc          ^
  config.cxx=x86_64-w64-mingw32-g++ ^

cd launcher

# To build:
bpkg build launcher@https://github.com/iw4x/launcher.git#main

# To test:
bpkg test launcher

# To install:
bpkg install launcher

# To uninstall:
bpkg uninstall launcher

# To upgrade:
bpkg fetch
bpkg status launcher
bpkg uninstall launcher
bpkg build --upgrade --recursive launcher
bpkg install launcher
```

#### Linux

```bash
# Create the build configuration:
bpkg create -d launcher @gcc cc

cd iw4x

# To build:
bpkg build launcher@https://github.com/iw4x/launcher.git#main

# To test:
bpkg test launcher

# To install:
bpkg install launcher

# To uninstall:
bpkg uninstall launcher

# To upgrade:
bpkg fetch
bpkg status launcher
bpkg uninstall launcher
bpkg build --upgrade --recursive launcher
bpkg install launcher
```

If you want to dive deeper into how these commands work, check out the [build2 package consumption guide](https://build2.org/build2-toolchain/doc/build2-toolchain-intro.xhtml#guide-consume-pkg).

### Development

> [!NOTE]
> These instructions are meant for developers looking to modify and contribute to the IW4x launcher codebase. If you just want to compile and test the latest build, follow the **Consumption** steps above instead.

#### Linux

```bash
# Clone the repository:
git clone --recursive https://github.com/iw4x/launcher.git

cd launcher

# Create the build configuration:
bdep init -C @gcc        \
  config.cc.compiledb=./ \
  cc

# To build:
b

# To test:
b test
```

#### Windows

```powershell
# Clone the repository:
git clone --recursive https://github.com/iw4x/launcher.git

cd iw4x

# Create the build configuration:
bdep init -C -@mingw32 cc           ^
  config.cxx=x86_64-w64-mingw32-g++ ^
  config.cc.compiledb=./

# To build:
b

# To test:
b test
```

For more details on `bdep` and typical development workflows, take a look at the [build2 build system manual](https://build2.org/build2/doc/build2-build-system-manual.xhtml).

## License

IW4x launcher is licensed under the GNU General Public License v3.0 or later. See [LICENSE.md](LICENSE.md) for details.
