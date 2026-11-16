cmake_minimum_required(VERSION 3.20)
project(netsurf-ce LANGUAGES C CXX ASM)

set(NETSURF_CE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/sources" CACHE PATH "Upstream sources fetched by tools/fetch-sources.mjs")
set(NETSURF_CE_GENERATED "${CMAKE_BINARY_DIR}/generated")

find_program(NODE_EXECUTABLE node REQUIRED)
add_compile_options(-mcpu=arm926ej-s "-ffile-prefix-map=${CMAKE_CURRENT_LIST_DIR}=." "-ffile-prefix-map=${NETSURF_CE_SOURCES}=sources" "-ffile-prefix-map=${CMAKE_BINARY_DIR}=build")
include(Grimace)

option(NETSURF_CE_PROFILE "Sample the UI thread to profile.bin" OFF)
if(NETSURF_CE_PROFILE)
    add_compile_options(-g)
endif()

execute_process(
    COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/generate.mjs" "${NETSURF_CE_SOURCES}" "${NETSURF_CE_GENERATED}"
    COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/tools/generate.mjs" "${CMAKE_CURRENT_LIST_DIR}/sources.json")

include(cmake/compat.cmake)
include(cmake/libraries.cmake)
include(cmake/netsurf.cmake)
include(cmake/frontend.cmake)
