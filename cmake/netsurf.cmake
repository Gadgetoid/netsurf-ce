set(netsurf_root "${NETSURF_CE_SOURCES}/netsurf")

function(netsurf_ce_prefix output prefix)
    list(TRANSFORM ARGN PREPEND "${netsurf_root}/${prefix}")
    set(${output} ${ARGN} PARENT_SCOPE)
endfunction()

netsurf_ce_prefix(netsurf_content content/
    content.c content_factory.c fetch.c hlcache.c llcache.c mimesniff.c textsearch.c urldb.c no_backing_store.c
    fetchers/data.c fetchers/resource.c
    fetchers/file/dirlist.c fetchers/file/file.c
    handlers/css/css.c handlers/css/dump.c handlers/css/internal.c handlers/css/hints.c handlers/css/select.c
    handlers/javascript/fetcher.c handlers/javascript/none/none.c
    handlers/text/textplain.c
    handlers/image/image.c handlers/image/image_cache.c handlers/image/bmp.c handlers/image/gif.c handlers/image/ico.c)
file(GLOB netsurf_about "${netsurf_root}/content/fetchers/about/*.c")
file(GLOB netsurf_html "${netsurf_root}/content/handlers/html/*.c")
file(GLOB netsurf_http "${netsurf_root}/utils/http/*.c")

netsurf_ce_prefix(netsurf_utils utils/
    bloom.c corestrings.c file.c filepath.c hashmap.c hashtable.c idna.c libdom.c log.c messages.c nscolour.c
    nsoption.c punycode.c ssl_certs.c talloc.c time.c url.c useragent.c utf8.c utils.c
    nsurl/nsurl.c nsurl/parse.c)

netsurf_ce_prefix(netsurf_desktop desktop/
    cookie_manager.c knockout.c hotlist.c mouse.c plot_style.c print.c search.c searchweb.c scrollbar.c
    textarea.c version.c system_colour.c local_history.c global_history.c treeview.c page-info.c
    bitmap.c browser.c browser_window.c browser_history.c download.c frames.c netsurf.c cw_helper.c
    save_complete.c save_text.c selection.c textinput.c gui_factory.c save_pdf.c font_haru.c)

add_library(netsurf_core STATIC
    ${netsurf_content} ${netsurf_about} ${netsurf_html} ${netsurf_http} ${netsurf_utils} ${netsurf_desktop})
target_include_directories(netsurf_core PUBLIC
    "${netsurf_root}" "${netsurf_root}/include" "${netsurf_root}/content/handlers" "${netsurf_root}/frontends"
    "${NETSURF_CE_GENERATED}/netsurf")
target_compile_definitions(netsurf_core PUBLIC
    nsframebuffer
    "NETSURF_UA_FORMAT_STRING=\"Mozilla/5.0 (%s) NetSurf/%d.%d\""
    "NETSURF_HOMEPAGE=\"about:welcome\""
    NETSURF_LOG_LEVEL=WARNING
    "NETSURF_BUILTIN_LOG_FILTER=\"(level:WARNING)\""
    "NETSURF_BUILTIN_VERBOSE_FILTER=\"(level:VERBOSE)\""
    STMTEXPR=1
    WITH_BMP WITH_GIF WITH_PNG WITH_JPEG)
target_compile_options(netsurf_core PUBLIC -std=gnu99 PRIVATE -w)
target_link_libraries(netsurf_core PUBLIC css dom hubbub parserutils wapcaplet nsutils nsgif nsbmp zlib ce_posix)
set_source_files_properties("${netsurf_root}/content/fetch.c" PROPERTIES COMPILE_DEFINITIONS WITH_CURL)
