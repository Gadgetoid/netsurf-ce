# netsurf-ce

[NetSurf](https://www.netsurf-browser.org/) for Windows CE 6.0 on the SHARP Brain (PW-SH1, PW-SH2 and similar). Built on NetSurf's framebuffer frontend, with a Windows CE display surface and an HTTP(S) fetcher over BearSSL.

Releases are a zip to copy to the microSD card: see [card/ReadMe.txt](card/ReadMe.txt).

## Building

Needs [jornada-72x-tools](https://github.com/Gadgetoid/jornada-72x-tools) with its LLVM, Node.js, Python 3, Perl, gperf, pkg-config, libpng and zlib.

```sh
make package JORNADA_TOOLCHAIN=/path/to/jornada-72x-tools   # build/netsurf-ce-VERSION.zip
make install JORNADA_TOOLCHAIN=... BRAIN_DEVICE=host:port   # copies to the card over debugmgr
```

## Layout

- `sources.json`: upstream repositories and commits. `tools/fetch-sources.mjs` fetches them into `sources/` and applies `patches/`.
- `tools/generate.mjs`: generated sources (libcss properties, hubbub entities, framebuffer fonts and icons, Messages).
- `compat/`: the POSIX and C library parts that CE's coredll lacks.
- `src/platform/`: the Windows side: entry point, window and input, HTTP and TLS on worker threads.
- `src/netsurf/`: the NetSurf side: libnsfb surface, `http`/`https` fetcher, PNG and JPEG decoding with stb_image.
- `card/`: files shipped in the zip.

## Not supported

JavaScript, SVG, TLS 1.3, the public suffix list (cookie scoping by registrable domain), and text outside ASCII from the keyboard.

## Licence

GPL version 2, as NetSurf. Each library's licence ships in the zip's `licences` folder.
