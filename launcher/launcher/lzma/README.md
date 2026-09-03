# Vendored LZMA decoder

The files in this directory are taken verbatim from Igor Pavlov's LZMA SDK
(public domain) and are **not** modified. Do not reformat or refactor them;
re-vendor from upstream instead.

    LzmaDec.c   2018-02-28  LZMA1 range decoder
    LzmaDec.h
    7zTypes.h
    Precomp.h
    Compiler.h

## Why it is here

Steam's depot chunks are compressed with either zstd or "VZip", the latter
being raw LZMA1 with a five-byte property header wrapped in Valve's own
container (see `steam/steam-compression.hxx`). zstd is available as a build2
package, but LZMA is not, so the decoder is vendored the same way BLAKE3 is.

Only the decoder is vendored: the launcher never compresses anything in this
format.

## Upstream

<https://www.7-zip.org/sdk.html>
