# Vendored QR Code generator

`qrcodegen.c` and `qrcodegen.h` are taken verbatim from Project Nayuki's QR
Code generator library (MIT licensed) and are **not** modified. Do not
reformat or refactor them; re-vendor from upstream instead.

## Why it is here

Steam's QR login hands the client a `https://s.team/q/...` challenge URL that
the user scans with the Steam mobile app. Rendering it in the terminal needs a
QR encoder, and there is no build2 package for one, so it is vendored the
same way BLAKE3 and the LZMA decoder are.

## Upstream

<https://github.com/nayuki/QR-Code-generator>
