cmake_minimum_required(VERSION 3.21)
project(arcal VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

set(ARCAL_UNITY_BATCH_SIZE "25" CACHE STRING "Number of source files per unity build batch")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

find_package(CycloneDDS   REQUIRED)
find_package(CycloneDDS-CXX REQUIRED)

# ---------------------------------------------------------------------------
# IDL Compiler — arcal_payload.idl → C/C++ type support
# ---------------------------------------------------------------------------

set(IDL_FILE    ${CMAKE_SOURCE_DIR}/src/dds/arcal_payload.idl)
set(IDL_INPUT   arcal_payload.idl)
set(IDL_OUT_C   ${CMAKE_SOURCE_DIR}/src/dds/arcal_payload.c)
set(IDL_OUT_CXX ${CMAKE_SOURCE_DIR}/src/dds/arcal_payload.cpp)
set(IDL_OUT_H   ${CMAKE_SOURCE_DIR}/src/dds/arcal_payload.h)
set(IDL_OUT_HPP ${CMAKE_SOURCE_DIR}/src/dds/arcal_payload.hpp)

# idlc ships with CycloneDDS; locate it via the imported target or find_program.
# get_target_property() is a hard error on a non-existent target, so guard first.
if(TARGET CycloneDDS::idlc)
    get_target_property(_CYCLONE_LOC CycloneDDS::idlc IMPORTED_LOCATION)
endif()
if(NOT _CYCLONE_LOC)
    find_program(_CYCLONE_LOC idlc
        HINTS
            ${CMAKE_PREFIX_PATH}/bin
            ${CMAKE_PREFIX_PATH}/tools/cyclonedds
        REQUIRED)
endif()

add_custom_command(
    OUTPUT  ${IDL_OUT_C} ${IDL_OUT_CXX}
            ${IDL_OUT_H}
            ${IDL_OUT_HPP}
    COMMAND ${_CYCLONE_LOC}        ${IDL_INPUT}
    COMMAND ${_CYCLONE_LOC} -l cxx ${IDL_INPUT}
    COMMAND ${CMAKE_COMMAND}
            "-DIDLC_SCRUB_FILES=${IDL_OUT_C};${IDL_OUT_CXX};${IDL_OUT_H};${IDL_OUT_HPP}"
            -P ${CMAKE_SOURCE_DIR}/cmake/ScrubIdlcSourcePaths.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src/dds
    DEPENDS ${IDL_FILE}
    COMMENT "Compiling arcal_payload.idl..."
    VERBATIM
)

add_custom_target(arcal_idl DEPENDS ${IDL_OUT_C} ${IDL_OUT_CXX})

# ---------------------------------------------------------------------------
# Schema Compiler — XSD → C++ headers
# ---------------------------------------------------------------------------

set(SCHEMA_DIR  ${CMAKE_SOURCE_DIR}/schema/OAC-STD-UCI_V2.5)
set(GENERATED_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE XSD_FILES ${SCHEMA_DIR}/*.xsd)

# Stamp file: schema compiler re-runs only when XSDs change
set(SCHEMA_STAMP ${CMAKE_BINARY_DIR}/schema_compiler.stamp)

add_custom_command(
    OUTPUT  ${SCHEMA_STAMP}
    COMMAND uv run python3
            ${CMAKE_SOURCE_DIR}/tools/schema_compiler/compiler.py
            --schema ${SCHEMA_DIR}
            --out    ${GENERATED_INCLUDE_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${SCHEMA_STAMP}
    DEPENDS ${XSD_FILES}
            ${CMAKE_SOURCE_DIR}/tools/schema_compiler/compiler.py
    COMMENT "Running arcal schema compiler..."
)

add_custom_target(arcal_generated DEPENDS ${SCHEMA_STAMP})

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------

add_library(arcal SHARED
    src/UUID.cpp
    src/dds/CdrBridge.cpp
    src/dds/DdsAbstractServiceBusConnection.cpp
    src/dds/DdsReaderCore.cpp
    src/dds/DdsWriterCore.cpp
    src/dds/arcal_payload.c
    src/dds/arcal_payload.cpp
    src/generated/factories_all.cpp
    src/generated/accessor_factory_all.cpp
    src/ExternalizerLoader.cpp
)

add_dependencies(arcal arcal_generated arcal_idl)

target_include_directories(arcal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
)

set_target_properties(arcal PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE ${ARCAL_UNITY_BATCH_SIZE}
)

target_link_libraries(arcal PUBLIC CycloneDDS-CXX::ddscxx PRIVATE ${CMAKE_DL_LIBS})

# Use mold linker if available — significantly faster than ld/lld for large targets.
find_program(MOLD_LINKER mold)
if(MOLD_LINKER)
    add_link_options(-fuse-ld=mold)
endif()

# ---------------------------------------------------------------------------
# CDR Externalizer plugin
# ---------------------------------------------------------------------------

add_subdirectory(externalizer/cdr)
add_subdirectory(externalizer/json)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

option(ARCAL_BUILD_TESTS     "Build CERT test suite"  ON)
option(ARCAL_BUILD_E2E_TESTS "Build E2E smoke tests"  ON)

if(ARCAL_BUILD_TESTS OR ARCAL_BUILD_E2E_TESTS)
    enable_testing()
endif()

if(ARCAL_BUILD_TESTS)
    add_subdirectory(test/conform)
    add_subdirectory(test/cert)
    add_subdirectory(test/json)
    add_subdirectory(test/cdr)
endif()

if(ARCAL_BUILD_E2E_TESTS)
    add_subdirectory(test/e2e)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

install(TARGETS arcal arcal_externalizer_cdr arcal_externalizer_json
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)
