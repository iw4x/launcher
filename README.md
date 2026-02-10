# IW4x Launcher

<img width="3194" height="1744" alt="image" src="https://github.com/user-attachments/assets/592124b1-5b55-4172-82e6-3f72e147df69" />

## Getting Started

If you're ready to start contributing, we recommend referring to [build2](https://build2.org/build2-toolchain/doc/build2-toolchain-intro.xhtml#preface) official documentation.

### Windows

```cmd
# Create the build configuration:
#
> bdep init -C @msvc cc ^
  config.cxx=cl

# To build:
#
> bdep update
```

### Linux

```bash
# Create the build configuration:
#
$ bdep init -C @gcc cc \
  config.cxx=g++

# To build:
#
$ bdep update
```

## License

IW4x Launcher is licensed under the [GPLv3](LICENSE.md).
