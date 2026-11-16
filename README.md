# netsurf-ce

[NetSurf](https://www.netsurf-browser.org/) for Windows CE 6.0 on the SHARP Brain (PW-SH1, PW-SH2 and similar). Built on NetSurf's framebuffer frontend, with a Windows CE display surface and an HTTP(S) fetcher over BearSSL.

Releases are a zip to copy to the microSD card: see [card/ReadMe.txt](card/ReadMe.txt).

## Building

Needs [grimace](https://github.com/grimace-sdk/grimace) with [llvm-builds](https://github.com/grimace-sdk/llvm-builds) v6 or later, CMake, Ninja, Node.js, Python 3, Perl, gperf, pkg-config, libpng and zlib. `tools/ce-toolchain.sh DIRECTORY` fetches the pinned grimace and LLVM.

```sh
make package GRIMACE=/path/to/grimace GRIMACE_LLVM_ROOT=/path/to/llvm   # build/netsurf-ce-VERSION.zip
make check-imports GRIMACE=...                                         # checks the imports against the PW-SH2 and PW-SH4
make install GRIMACE=... BRAIN_DEVICE=host:port                        # copies to the card with ce-debug
```

`GRIMACE_LLVM_ROOT` can be left out when grimace finds the LLVM itself. `CE_DEBUG` names the ce-debug client when it isn't on `PATH`.

## Layout

- `sources.json`: upstream repositories and commits. `tools/fetch-sources.mjs` fetches them into `sources/` and applies `patches/`.
- `tools/generate.mjs`: generated sources (libcss properties, hubbub entities, framebuffer fonts and icons, Messages).
- `compat/`: the POSIX and C library parts that coredll and grimace's runtime lack, a C99 printf that logs stdout and stderr to NetSurf.log, and UTF-8 file paths.
- `src/platform/`: the Windows side: entry point, window and input, HTTP and TLS on worker threads.
- `src/netsurf/`: the NetSurf side: libnsfb surface, `http`/`https` fetcher, PNG and JPEG decoding with stb_image.
- `card/`: files shipped in the zip.

## Not supported

JavaScript, SVG, TLS 1.3, the public suffix list (cookie scoping by registrable domain), and text outside ASCII from the keyboard.

## Licence

GPL version 2, as NetSurf. Each library's licence ships in the zip's `licences` folder.
