cmake_minimum_required(VERSION 3.20)
project(netsurf-ce LANGUAGES C CXX ASM)

if(NOT JORNADA_TOOLCHAIN)
    message(FATAL_ERROR "Set JORNADA_TOOLCHAIN to the jornada-72x-tools directory")
endif()
set(NETSURF_CE_SOURCES "${CMAKE_CURRENT_LIST_DIR}/sources" CACHE PATH "Upstream sources fetched by tools/fetch-sources.mjs")
set(NETSURF_CE_GENERATED "${CMAKE_BINARY_DIR}/generated")

find_program(NODE_EXECUTABLE node REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${JORNADA_TOOLCHAIN}/cmake")
add_compile_options(-mcpu=arm926ej-s "-ffile-prefix-map=${CMAKE_CURRENT_LIST_DIR}=." "-ffile-prefix-map=${NETSURF_CE_SOURCES}=sources" "-ffile-prefix-map=${CMAKE_BINARY_DIR}=build")
include(JornadaCE)

option(NETSURF_CE_PROFILE "Sample the UI thread to profile.bin" OFF)
if(NETSURF_CE_PROFILE)
    add_compile_options(-g)
endif()

execute_process(
    COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/generate.mjs" "${NETSURF_CE_SOURCES}" "${NETSURF_CE_GENERATED}"
    COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/tools/generate.mjs" "${CMAKE_CURRENT_LIST_DIR}/sources.json")

set(BRAIN_IMPORT_DIRECTORY "${CMAKE_BINARY_DIR}/brain-imports")
execute_process(
    COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/tools/prepare-imports.mjs"
        "${CMAKE_CURRENT_LIST_DIR}/ce-imports" "${BRAIN_IMPORT_DIRECTORY}"
    COMMAND_ERROR_IS_FATAL ANY)
set(coredll_stubs "${BRAIN_IMPORT_DIRECTORY}/coredll.S")
add_custom_command(OUTPUT "${coredll_stubs}"
    COMMAND "${Python3_EXECUTABLE}" "${JORNADA_TOOLCHAIN}/tools/mkimplib.py"
        "${BRAIN_IMPORT_DIRECTORY}/coredll.txt" "coredll.dll" "${coredll_stubs}"
    DEPENDS "${BRAIN_IMPORT_DIRECTORY}/coredll.txt"
    VERBATIM)
add_library(brain_import_coredll STATIC "${coredll_stubs}")

include(cmake/compat.cmake)
include(cmake/libraries.cmake)
include(cmake/netsurf.cmake)
include(cmake/frontend.cmake)
