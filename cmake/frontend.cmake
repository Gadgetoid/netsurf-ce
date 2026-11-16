set(framebuffer_root "${netsurf_root}/frontends/framebuffer")
set(project_root "${CMAKE_CURRENT_LIST_DIR}/..")

file(GLOB framebuffer_images "${NETSURF_CE_GENERATED}/netsurf/image-*.c")
add_library(framebuffer STATIC
    "${framebuffer_root}/gui.c" "${framebuffer_root}/framebuffer.c" "${framebuffer_root}/schedule.c"
    "${framebuffer_root}/bitmap.c" "${framebuffer_root}/fetch.c" "${framebuffer_root}/findfile.c"
    "${framebuffer_root}/corewindow.c" "${framebuffer_root}/local_history.c" "${framebuffer_root}/clipboard.c"
    "${framebuffer_root}/font_freetype.c"
    "${framebuffer_root}/fbtk/fbtk.c" "${framebuffer_root}/fbtk/event.c" "${framebuffer_root}/fbtk/fill.c"
    "${framebuffer_root}/fbtk/bitmap.c" "${framebuffer_root}/fbtk/user.c" "${framebuffer_root}/fbtk/window.c"
    "${framebuffer_root}/fbtk/text.c" "${framebuffer_root}/fbtk/scroll.c" "${framebuffer_root}/fbtk/osk.c"
    ${framebuffer_images})
target_compile_definitions(framebuffer PRIVATE
    small
    "NETSURF_FB_RESPATH=\"\${NETSURFRES}\""
    "NETSURF_FB_FONTPATH=\"\""
    FB_USE_FREETYPE
    "NETSURF_FB_FONT_SANS_SERIF=\"DejaVuSans.ttf\""
    "NETSURF_FB_FONT_SANS_SERIF_BOLD=\"DejaVuSans-Bold.ttf\""
    "NETSURF_FB_FONT_SANS_SERIF_ITALIC=\"DejaVuSans-Oblique.ttf\""
    "NETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD=\"DejaVuSans-BoldOblique.ttf\""
    "NETSURF_FB_FONT_SERIF=\"DejaVuSerif.ttf\""
    "NETSURF_FB_FONT_SERIF_BOLD=\"DejaVuSerif-Bold.ttf\""
    "NETSURF_FB_FONT_MONOSPACE=\"DejaVuSansMono.ttf\""
    "NETSURF_FB_FONT_MONOSPACE_BOLD=\"DejaVuSansMono-Bold.ttf\""
    "NETSURF_FB_FONT_CURSIVE=\"DejaVuSerif-Italic.ttf\""
    "NETSURF_FB_FONT_FANTASY=\"DejaVuSans.ttf\"")
set_source_files_properties("${framebuffer_root}/gui.c" PROPERTIES COMPILE_DEFINITIONS main=netsurf_framebuffer_main)
target_compile_options(framebuffer PRIVATE -w)
target_link_libraries(framebuffer PUBLIC netsurf_core nsfb freetype)

set(platform_definitions UNICODE _UNICODE UNDER_CE WINCE _WINDOWS GRIMACE_ALL_DECLARATIONS)
set(platform_options -std=gnu99 -Wall -Werror -Wno-unused-parameter)
file(GLOB platform_sources "${project_root}/src/platform/*.c")
list(FILTER platform_sources EXCLUDE REGEX "/(main|profile)\\.c$")
if(NETSURF_CE_PROFILE)
    list(APPEND platform_sources "${project_root}/src/platform/profile.c")
    list(APPEND platform_definitions NETSURF_CE_PROFILE)
endif()
add_library(platform STATIC ${platform_sources} "${NETSURF_CE_GENERATED}/platform/trust-anchors.c")
target_include_directories(platform PRIVATE "${project_root}/src")
set_source_files_properties("${NETSURF_CE_GENERATED}/platform/trust-anchors.c" PROPERTIES COMPILE_OPTIONS -w)
target_compile_definitions(platform PRIVATE ${platform_definitions})
target_compile_options(platform PRIVATE ${platform_options})
target_link_libraries(platform PUBLIC bearssl zlib ce_posix)

file(GLOB glue_sources "${project_root}/src/netsurf/*.c")
add_library(glue STATIC ${glue_sources})
target_include_directories(glue PRIVATE "${project_root}/src" "${NETSURF_CE_SOURCES}/libnsfb/src")
target_compile_options(glue PRIVATE -Wall -Werror -Wno-unused-parameter)
target_link_libraries(glue PUBLIC framebuffer platform stb_image)

grimace_add_executable(netsurf OUTPUT NetSurf.exe ICON "${netsurf_root}/frontends/windows/res/NetSurf.ico"
    "${project_root}/src/platform/main.c")
target_include_directories(netsurf SYSTEM BEFORE PRIVATE "${compat_root}/include")
target_compile_definitions(netsurf PRIVATE ${platform_definitions})
target_compile_options(netsurf PRIVATE ${platform_options})
target_link_options(netsurf PRIVATE
    --whole-archive $<TARGET_FILE:glue> $<TARGET_FILE:nsfb> $<TARGET_FILE:ce_posix> --no-whole-archive)
target_link_libraries(netsurf PRIVATE glue grimace::winsock)
