# launcher - Launcher and updater for IW4x.

The `iw4x-launcher` executable keeps the IW4x installation in its directory
up to date with the latest IW4x release and starts the selected IW4x client,
through Proton on Linux.

Note that the `iw4x-launcher` executable in this package provides `build2`
metadata.

## Usage

To start using `launcher` in your project, add the following build-time
`depends` value to your `manifest`, adjusting the version constraint as
appropriate:

```
depends: * launcher ^1.1.8-
```

Then import the executable in your `buildfile`:

```
import! [metadata] launcher = launcher%exe{iw4x-launcher}
```

## Configuration variables

This package provides the following configuration variables:

```
[bool] config.launcher.develop        ?= false
[bool] config.launcher.static_runtime ?= true
```

If `config.launcher.develop` is `true`, then the build regenerates the files
kept in `launcher/pregenerated/`, which requires the `cli` and `odb`
compilers as well as `xxd`.

If `config.launcher.static_runtime` is `true`, then the Linux launcher links
the C++ runtime statically, so that it runs on hosts with an older one.
Disable it if your distribution doesn't install the static runtime, as is
the case on Fedora by default.
