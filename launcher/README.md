# launcher - Launcher and updater for IW4x.

The `launcher` executable is the launcher and updater of IW4x. It runs on
players' computers and on headless servers. It is in early development, and
in this version it prints its usage and version.

The package also builds `libs{launcher-node}`, a Node.js native addon through
which a desktop interface written for Electron calls into the launcher.

## Usage

`launcher` has no releases yet. Build it from source as the Development
section of the repository [README.md](../README.md) describes, then run:

```
launcher --help
```
