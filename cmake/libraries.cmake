function(netsurf_ce_library name)
    cmake_parse_arguments(PARSE_ARGV 1 library "" "" "SOURCES;PUBLIC_INCLUDES;PRIVATE_INCLUDES;DEFINITIONS;LINK")
    add_library(${name} STATIC ${library_SOURCES})
    target_include_directories(${name} PUBLIC ${library_PUBLIC_INCLUDES} PRIVATE ${library_PRIVATE_INCLUDES})
    target_compile_definitions(${name} PRIVATE "_ALIGNED=__attribute__((aligned))" ${library_DEFINITIONS})
    target_compile_options(${name} PRIVATE -std=gnu99 -w)
    target_link_libraries(${name} PUBLIC ce_posix ${library_LINK})
endfunction()

set(source_root "${NETSURF_CE_SOURCES}")

file(GLOB zlib_sources "${source_root}/zlib/*.c")
netsurf_ce_library(zlib
    SOURCES ${zlib_sources}
    PUBLIC_INCLUDES "${source_root}/zlib"
    DEFINITIONS HAVE_UNISTD_H HAVE_STDARG_H)
target_compile_options(zlib PRIVATE -U_WIN32_WCE)

file(GLOB_RECURSE bearssl_sources "${source_root}/bearssl/src/*.c")
netsurf_ce_library(bearssl
    SOURCES ${bearssl_sources}
    PUBLIC_INCLUDES "${source_root}/bearssl/inc"
    PRIVATE_INCLUDES "${source_root}/bearssl/src"
    DEFINITIONS BR_USE_WIN32_RAND=0 BR_USE_URANDOM=0 BR_USE_GETENTROPY=0 BR_USE_WIN32_TIME=0 BR_USE_UNIX_TIME=0)

netsurf_ce_library(wapcaplet
    SOURCES "${source_root}/libwapcaplet/src/libwapcaplet.c"
    PUBLIC_INCLUDES "${source_root}/libwapcaplet/include")

file(GLOB parserutils_sources
    "${source_root}/libparserutils/src/charset/*.c"
    "${source_root}/libparserutils/src/charset/codecs/*.c"
    "${source_root}/libparserutils/src/charset/encodings/*.c"
    "${source_root}/libparserutils/src/input/*.c"
    "${source_root}/libparserutils/src/utils/*.c")
netsurf_ce_library(parserutils
    SOURCES ${parserutils_sources}
    PUBLIC_INCLUDES "${source_root}/libparserutils/include"
    PRIVATE_INCLUDES "${source_root}/libparserutils/src" "${NETSURF_CE_GENERATED}/libparserutils")

file(GLOB hubbub_sources
    "${source_root}/libhubbub/src/*.c"
    "${source_root}/libhubbub/src/charset/*.c"
    "${source_root}/libhubbub/src/tokeniser/*.c"
    "${source_root}/libhubbub/src/treebuilder/*.c"
    "${source_root}/libhubbub/src/utils/*.c")
netsurf_ce_library(hubbub
    SOURCES ${hubbub_sources}
    PUBLIC_INCLUDES "${source_root}/libhubbub/include"
    PRIVATE_INCLUDES "${source_root}/libhubbub/src" "${NETSURF_CE_GENERATED}/libhubbub"
    LINK parserutils)

file(GLOB dom_sources
    "${source_root}/libdom/src/core/*.c"
    "${source_root}/libdom/src/events/*.c"
    "${source_root}/libdom/src/html/*.c"
    "${source_root}/libdom/src/utils/*.c"
    "${source_root}/libdom/bindings/hubbub/*.c")
netsurf_ce_library(dom
    SOURCES ${dom_sources}
    PUBLIC_INCLUDES "${source_root}/libdom/include" "${NETSURF_CE_GENERATED}/libdom/include"
    PRIVATE_INCLUDES "${source_root}/libdom/src" "${source_root}/libdom"
    LINK hubbub wapcaplet)

file(GLOB css_sources
    "${source_root}/libcss/src/*.c"
    "${source_root}/libcss/src/charset/*.c"
    "${source_root}/libcss/src/lex/*.c"
    "${source_root}/libcss/src/parse/*.c"
    "${source_root}/libcss/src/parse/properties/*.c"
    "${source_root}/libcss/src/select/*.c"
    "${source_root}/libcss/src/select/properties/*.c"
    "${source_root}/libcss/src/utils/*.c"
    "${NETSURF_CE_GENERATED}/libcss/*.c")
list(FILTER css_sources EXCLUDE REGEX "/css_property_parser_gen\\.c$")
netsurf_ce_library(css
    SOURCES ${css_sources}
    PUBLIC_INCLUDES "${source_root}/libcss/include"
    PRIVATE_INCLUDES "${source_root}/libcss/src" "${source_root}/libcss/src/parse/properties"
    LINK parserutils wapcaplet)

file(GLOB nsutils_sources "${source_root}/libnsutils/src/*.c")
netsurf_ce_library(nsutils
    SOURCES ${nsutils_sources}
    PUBLIC_INCLUDES "${source_root}/libnsutils/include")

netsurf_ce_library(nsgif
    SOURCES "${source_root}/libnsgif/src/gif.c" "${source_root}/libnsgif/src/lzw.c"
    PUBLIC_INCLUDES "${source_root}/libnsgif/include")

netsurf_ce_library(nsbmp
    SOURCES "${source_root}/libnsbmp/src/libnsbmp.c"
    PUBLIC_INCLUDES "${source_root}/libnsbmp/include")

set(nsfb_root "${source_root}/libnsfb/src")
netsurf_ce_library(nsfb
    SOURCES
        "${nsfb_root}/libnsfb.c" "${nsfb_root}/cursor.c" "${nsfb_root}/palette.c"
        "${nsfb_root}/plot/api.c" "${nsfb_root}/plot/util.c" "${nsfb_root}/plot/generic.c"
        "${nsfb_root}/plot/32bpp-xrgb8888.c" "${nsfb_root}/plot/32bpp-xbgr8888.c"
        "${nsfb_root}/plot/16bpp.c" "${nsfb_root}/plot/8bpp.c"
        "${nsfb_root}/surface/surface.c" "${nsfb_root}/surface/ram.c"
    PUBLIC_INCLUDES "${source_root}/libnsfb/include"
    PRIVATE_INCLUDES "${nsfb_root}")

set(stb_image_source "${NETSURF_CE_GENERATED}/stb_image.c")
file(CONFIGURE OUTPUT "${stb_image_source}" CONTENT "#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
")
netsurf_ce_library(stb_image
    SOURCES "${stb_image_source}"
    PUBLIC_INCLUDES "${source_root}/stb")
target_compile_definitions(stb_image PUBLIC STBI_NO_THREAD_LOCALS STBI_ONLY_PNG STBI_ONLY_JPEG STBI_NO_STDIO STBI_NO_LINEAR STBI_NO_HDR)

set(freetype_root "${source_root}/freetype/src")
netsurf_ce_library(freetype
    SOURCES
        "${freetype_root}/base/ftsystem.c" "${freetype_root}/base/ftinit.c" "${freetype_root}/base/ftdebug.c"
        "${freetype_root}/base/ftbase.c" "${freetype_root}/base/ftbbox.c" "${freetype_root}/base/ftbitmap.c"
        "${freetype_root}/base/ftglyph.c" "${freetype_root}/base/ftmm.c" "${freetype_root}/truetype/truetype.c" "${freetype_root}/sfnt/sfnt.c"
        "${freetype_root}/smooth/smooth.c" "${freetype_root}/autofit/autofit.c" "${freetype_root}/psnames/psnames.c"
        "${freetype_root}/cache/ftcache.c" "${freetype_root}/gzip/ftgzip.c"
    PUBLIC_INCLUDES "${source_root}/freetype/include"
    DEFINITIONS FT2_BUILD_LIBRARY "FT_CONFIG_MODULES_H=\"${CMAKE_CURRENT_LIST_DIR}/freetype-modules.h\"")
