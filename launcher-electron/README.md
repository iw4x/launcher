# launcher-electron - Desktop application for the IW4x launcher.

The `launcher-electron` package is the desktop application through which
players use the launcher. It draws the window and interface, and hands every
request to the [`launcher`](../launcher/README.md) package, which it loads as
a Node.js native addon.

## Usage

`launcher-electron` is in early development and has no releases yet. Build it
from source as the Development section of the repository
[README.md](../README.md) describes.

## Language exception

The `launcher-electron` package holds the launcher's TypeScript code, under
an exception to the IW4x rule that projects are written in C or C++. Electron
runs application code in JavaScript, so the code of each Electron process is
TypeScript compiled to JavaScript. The behavior that the application presents
lives in the `launcher` C++ package, and the exception covers this package
alone.

Owner: William Roy

Review date: 2027-04-14
