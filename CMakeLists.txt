cmake_minimum_required(VERSION 3.21)
project(arcal VERSION 0.1.0 LANGUAGES CXX C)

file(WRITE "${CMAKE_BINARY_DIR}/.gitignore" "*\n")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

set(ARCAL_UNITY_BATCH_SIZE "100" CACHE STRING "Number of source files per unity build batch")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

find_package(CycloneDDS   REQUIRED)
find_package(CycloneDDS-CXX REQUIRED)
find_package(nlohmann_json 3.12.0 REQUIRED)

option(ARCAL_BUILD_TESTS         "Build CERT test suite"  ON)
option(ARCAL_BUILD_E2E_TESTS     "Build E2E smoke tests"  ON)
option(ARCAL_BUILD_INSTALL_TESTS "Build install smoke tests" ON)
option(ARCAL_BUILD_EXAMPLES      "Build optional example applications" OFF)
option(ARCAL_USE_MOLD_LINKER     "Use mold for linking when available" ON)
set(ARCAL_SUBSET_CONFIGS "" CACHE STRING "Semicolon-separated subset CAL config JSON files to build as additional libraries")

if(ARCAL_USE_MOLD_LINKER AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    find_program(ARCAL_MOLD_LINKER mold)
    if(ARCAL_MOLD_LINKER)
        add_link_options(-fuse-ld=mold)
    endif()
endif()

if(ARCAL_BUILD_TESTS OR ARCAL_BUILD_E2E_TESTS OR ARCAL_BUILD_INSTALL_TESTS)
    enable_testing()
endif()

if(ARCAL_BUILD_TESTS OR ARCAL_BUILD_E2E_TESTS)
    add_custom_target(arcal_test_suite_all)
endif()

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

function(arcal_add_subset subset_config_path)
    get_filename_component(subset_config_abs "${subset_config_path}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    if(NOT EXISTS "${subset_config_abs}")
        message(FATAL_ERROR "ARCAL subset config not found: ${subset_config_path}")
    endif()

    file(READ "${subset_config_abs}" subset_config_json)
    string(JSON subset_suffix ERROR_VARIABLE subset_suffix_error GET "${subset_config_json}" cal_name_suffix)
    if(subset_suffix_error)
        message(FATAL_ERROR "Failed to read cal_name_suffix from ${subset_config_abs}: ${subset_suffix_error}")
    endif()

    set(subset_name "arcal-${subset_suffix}")
    string(MAKE_C_IDENTIFIER "${subset_name}" subset_target_base)
    set(subset_root "${CMAKE_BINARY_DIR}/subsets/${subset_name}")
    set(subset_include_dir "${subset_root}/include")
    set(subset_generated_dir "${subset_root}/src/generated")
    set(subset_json_dir "${subset_generated_dir}/json")
    set(subset_stamp "${subset_root}/schema_compiler.stamp")
    set(subset_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${subset_name}")

    file(MAKE_DIRECTORY "${subset_include_dir}")

    if(NOT EXISTS "${subset_stamp}"
       OR "${subset_config_abs}" IS_NEWER_THAN "${subset_stamp}")
        message(STATUS "Generating subset sources for ${subset_name}...")
        execute_process(
            COMMAND uv run python3
                    ${CMAKE_SOURCE_DIR}/tools/schema_compiler/compiler.py
                    --schema ${SCHEMA_DIR}
                    --out ${subset_include_dir}
                    --subset-config ${subset_config_abs}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE subset_generate_result
        )
        if(NOT subset_generate_result EQUAL 0)
            message(FATAL_ERROR "Subset schema generation failed for ${subset_name}")
        endif()
        file(TOUCH "${subset_stamp}")
    endif()

    file(GLOB_RECURSE subset_cdr_generated_sources
        ${subset_generated_dir}/*_cdr.cpp
        ${subset_generated_dir}/cdr_register_all.cpp
    )
    file(GLOB subset_core_generated_sources
        ${subset_generated_dir}/type_impl/*Impl.cpp
        ${subset_generated_dir}/accessor_factory_all.cpp
    )
    file(GLOB_RECURSE subset_json_generated_sources
        ${subset_json_dir}/*_json.cpp
        ${subset_json_dir}/json_register_all.cpp
    )

    if(NOT subset_cdr_generated_sources)
        message(FATAL_ERROR "Subset ${subset_name} produced no CDR generated sources")
    endif()
    if(NOT subset_core_generated_sources)
        message(FATAL_ERROR "Subset ${subset_name} produced no core generated sources")
    endif()
    if(NOT subset_json_generated_sources)
        message(FATAL_ERROR "Subset ${subset_name} produced no JSON generated sources")
    endif()

    add_custom_command(
        OUTPUT ${subset_stamp}
        COMMAND uv run python3
                ${CMAKE_SOURCE_DIR}/tools/schema_compiler/compiler.py
                --schema ${SCHEMA_DIR}
                --out ${subset_include_dir}
                --subset-config ${subset_config_abs}
        COMMAND ${CMAKE_COMMAND} -E touch ${subset_stamp}
        DEPENDS ${XSD_FILES}
                ${CMAKE_SOURCE_DIR}/tools/schema_compiler/compiler.py
                ${subset_config_abs}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running subset schema compiler for ${subset_name}..."
        VERBATIM
    )

    add_custom_target(${subset_target_base}_generated DEPENDS ${subset_stamp})

    set_source_files_properties(
        ${subset_generated_dir}/cdr_register_all.cpp
        ${subset_generated_dir}/accessor_factory_all.cpp
        ${subset_json_dir}/json_register_all.cpp
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )

    add_library(${subset_target_base}_cdr_objects OBJECT
        externalizer/cdr/CdrExternalizer.cpp
        externalizer/cdr/ExternalizerLoader.cpp
        ${subset_cdr_generated_sources}
    )

    add_dependencies(${subset_target_base}_cdr_objects ${subset_target_base}_generated)

    target_include_directories(${subset_target_base}_cdr_objects PRIVATE
        ${subset_include_dir}
        ${subset_root}/src
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/externalizer/cdr
    )

    set_target_properties(${subset_target_base}_cdr_objects PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        UNITY_BUILD ON
        UNITY_BUILD_BATCH_SIZE ${ARCAL_UNITY_BATCH_SIZE}
    )

    target_precompile_headers(${subset_target_base}_cdr_objects PRIVATE
        "${CMAKE_SOURCE_DIR}/externalizer/cdr/CdrPrimitives.h"
        "${CMAKE_SOURCE_DIR}/externalizer/cdr/CdrRegistry.h"
        "${CMAKE_SOURCE_DIR}/include/uci/base/BoundedList.h"
        "${CMAKE_SOURCE_DIR}/include/uci/base/SimpleList.h"
    )

    target_compile_options(${subset_target_base}_cdr_objects PRIVATE -g1 -pipe)

    add_library(${subset_target_base} SHARED
        src/config/CalConfig.cpp
        src/UUID.cpp
        src/dds/CdrBridge.cpp
        src/dds/DdsAbstractServiceBusConnection.cpp
        src/dds/DdsReaderCore.cpp
        src/dds/DdsWriterCore.cpp
        src/dds/arcal_payload.c
        src/dds/arcal_payload.cpp
        ${subset_core_generated_sources}
        src/ExternalizerLoader.cpp
        $<TARGET_OBJECTS:${subset_target_base}_cdr_objects>
    )

    add_dependencies(${subset_target_base} ${subset_target_base}_generated arcal_idl)

    target_compile_definitions(${subset_target_base} PRIVATE
        ARCAL_JSON_PLUGIN_SONAME=\"lib${subset_name}_externalizer_json.so\"
    )

    target_include_directories(${subset_target_base} PUBLIC
        $<BUILD_INTERFACE:${subset_include_dir}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${subset_name}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        $<BUILD_INTERFACE:${subset_root}/src>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    )

    set_target_properties(${subset_target_base} PROPERTIES
        OUTPUT_NAME ${subset_name}
        UNITY_BUILD ON
        UNITY_BUILD_BATCH_SIZE ${ARCAL_UNITY_BATCH_SIZE}
        INSTALL_RPATH "$ORIGIN"
    )

    target_link_libraries(${subset_target_base} PUBLIC CycloneDDS-CXX::ddscxx PRIVATE nlohmann_json::nlohmann_json ${CMAKE_DL_LIBS})

    add_library(${subset_target_base}_externalizer_json SHARED
        externalizer/json/JsonExternalizer.cpp
        externalizer/json/ExternalizerLoader.cpp
        ${subset_json_generated_sources}
    )

    target_include_directories(${subset_target_base}_externalizer_json PUBLIC
        $<BUILD_INTERFACE:${subset_include_dir}>
        $<BUILD_INTERFACE:${subset_root}/src>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/externalizer/json>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${subset_name}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/arcal/externalizer/json>
    )

    set_target_properties(${subset_target_base}_externalizer_json PROPERTIES
        OUTPUT_NAME ${subset_name}_externalizer_json
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        UNITY_BUILD ON
        UNITY_BUILD_BATCH_SIZE ${ARCAL_UNITY_BATCH_SIZE}
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        INSTALL_RPATH "$ORIGIN"
    )

    target_precompile_headers(${subset_target_base}_externalizer_json PRIVATE
        "${CMAKE_SOURCE_DIR}/externalizer/json/JsonPrimitives.h"
        "${CMAKE_SOURCE_DIR}/externalizer/json/JsonParse.h"
        "${CMAKE_SOURCE_DIR}/externalizer/json/JsonRegistry.h"
        "${CMAKE_SOURCE_DIR}/include/uci/base/BoundedList.h"
        "${CMAKE_SOURCE_DIR}/include/uci/base/SimpleList.h"
    )

    target_compile_options(${subset_target_base}_externalizer_json PRIVATE -g1 -pipe)
    target_link_libraries(${subset_target_base}_externalizer_json PRIVATE ${subset_target_base} nlohmann_json::nlohmann_json)

    install(TARGETS ${subset_target_base} ${subset_target_base}_externalizer_json
        EXPORT ${subset_target_base}Targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${subset_target_base}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${subset_target_base}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${subset_target_base}
    )

    install(
        DIRECTORY ${subset_include_dir}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${subset_name}
        COMPONENT ${subset_target_base}
    )

    install(
        DIRECTORY
            ${CMAKE_SOURCE_DIR}/include/uci/base
            ${CMAKE_SOURCE_DIR}/include/uci/utils
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/uci
        COMPONENT ${subset_target_base}
    )

    install(
        DIRECTORY ${CMAKE_SOURCE_DIR}/include/xs
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT ${subset_target_base}
    )

    install(
        DIRECTORY ${CMAKE_SOURCE_DIR}/externalizer/json/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/arcal/externalizer/json
        COMPONENT ${subset_target_base}
        FILES_MATCHING
            PATTERN "*.h"
    )

    set(ARCAL_SUBSET_TARGETS_FILE "${subset_name}Targets.cmake")
    configure_package_config_file(
        ${CMAKE_SOURCE_DIR}/cmake/arcalSubsetConfig.cmake.in
        ${CMAKE_BINARY_DIR}/${subset_name}Config.cmake
        INSTALL_DESTINATION ${subset_package_dir}
    )

    write_basic_package_version_file(
        ${CMAKE_BINARY_DIR}/${subset_name}ConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(
        EXPORT ${subset_target_base}Targets
        FILE ${subset_name}Targets.cmake
        NAMESPACE ${subset_target_base}::
        DESTINATION ${subset_package_dir}
        COMPONENT ${subset_target_base}
    )

    install(
        FILES
            ${CMAKE_BINARY_DIR}/${subset_name}Config.cmake
            ${CMAKE_BINARY_DIR}/${subset_name}ConfigVersion.cmake
        DESTINATION ${subset_package_dir}
        COMPONENT ${subset_target_base}
    )

    install(
        FILES
            ${CMAKE_SOURCE_DIR}/README.md
            ${subset_config_abs}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${subset_name}
        COMPONENT ${subset_target_base}
    )

    set(_arcal_variants ${ARCAL_CAL_VARIANTS})
    list(APPEND _arcal_variants ${subset_target_base})
    set(ARCAL_CAL_VARIANTS "${_arcal_variants}" PARENT_SCOPE)
    set(ARCAL_JSON_PLUGIN_${subset_target_base} "${subset_target_base}_externalizer_json" PARENT_SCOPE)
    set(ARCAL_GENERATED_TARGET_${subset_target_base} "${subset_target_base}_generated" PARENT_SCOPE)
    set(ARCAL_LACAL_OUTPUT_${subset_target_base} "arlacal-server-${subset_suffix}" PARENT_SCOPE)
    set(ARCAL_EXAMPLE_PREFIX_${subset_target_base} "${subset_target_base}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------

file(GLOB_RECURSE CDR_GENERATED_SOURCES
    ${CMAKE_SOURCE_DIR}/src/generated/*_cdr.cpp
    ${CMAKE_SOURCE_DIR}/src/generated/cdr_register_all.cpp
)

file(GLOB ARCAL_CORE_GENERATED_SOURCES
    ${CMAKE_SOURCE_DIR}/src/generated/type_impl/*Impl.cpp
    ${CMAKE_SOURCE_DIR}/src/generated/accessor_factory_all.cpp
)

set(ARCAL_HAVE_FULL_GENERATED OFF)
if(CDR_GENERATED_SOURCES AND ARCAL_CORE_GENERATED_SOURCES)
    set(ARCAL_HAVE_FULL_GENERATED ON)
endif()

if(ARCAL_HAVE_FULL_GENERATED)
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/src/generated/cdr_register_all.cpp
        ${CMAKE_SOURCE_DIR}/src/generated/accessor_factory_all.cpp
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )

    add_library(arcal_cdr_objects OBJECT
        externalizer/cdr/CdrExternalizer.cpp
        externalizer/cdr/ExternalizerLoader.cpp
        ${CDR_GENERATED_SOURCES}
    )

    add_dependencies(arcal_cdr_objects arcal_generated)

    target_include_directories(arcal_cdr_objects PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/externalizer/cdr
    )

    set_target_properties(arcal_cdr_objects PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        UNITY_BUILD ON
        UNITY_BUILD_BATCH_SIZE ${ARCAL_UNITY_BATCH_SIZE}
    )

    target_precompile_headers(arcal_cdr_objects PRIVATE
        "${CMAKE_SOURCE_DIR}/externalizer/cdr/CdrPrimitives.h"
        "${CMAKE_SOURCE_DIR}/externalizer/cdr/CdrRegistry.h"
        "${CMAKE_SOURCE_DIR}/include/uci/base/BoundedList.h"
        "${CMAKE_SOURCE_DIR}/include/uci/base/SimpleList.h"
    )

    # Generated CDR files are never debugged directly; line tables are enough for
    # stack traces and cost a fraction of full DWARF (-g). -pipe avoids temp files
    # between compiler stages.
    target_compile_options(arcal_cdr_objects PRIVATE -g1 -pipe)

    add_library(arcal_core_generated_objects OBJECT
        ${ARCAL_CORE_GENERATED_SOURCES}
    )

    add_dependencies(arcal_core_generated_objects arcal_generated)

    target_include_directories(arcal_core_generated_objects PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )

    set_target_properties(arcal_core_generated_objects PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        UNITY_BUILD ON
        UNITY_BUILD_BATCH_SIZE ${ARCAL_UNITY_BATCH_SIZE}
    )

    target_precompile_headers(arcal_core_generated_objects PRIVATE
        "${CMAKE_SOURCE_DIR}/include/arcal/AccessorFactory.h"
        "${CMAKE_SOURCE_DIR}/include/uci/base/BoundedListImpl.h"
        "${CMAKE_SOURCE_DIR}/include/uci/base/SimpleListImpl.h"
    )

    target_link_libraries(arcal_core_generated_objects PRIVATE CycloneDDS-CXX::ddscxx)

    add_library(arcal SHARED
        src/config/CalConfig.cpp
        src/UUID.cpp
        src/dds/CdrBridge.cpp
        src/dds/DdsAbstractServiceBusConnection.cpp
        src/dds/DdsReaderCore.cpp
        src/dds/DdsWriterCore.cpp
        src/dds/arcal_payload.c
        src/dds/arcal_payload.cpp
        src/ExternalizerLoader.cpp
        $<TARGET_OBJECTS:arcal_core_generated_objects>
        $<TARGET_OBJECTS:arcal_cdr_objects>
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
        INSTALL_RPATH "$ORIGIN"
    )

    target_link_libraries(arcal PUBLIC CycloneDDS-CXX::ddscxx PRIVATE nlohmann_json::nlohmann_json ${CMAKE_DL_LIBS})

    set(ARCAL_CAL_VARIANTS arcal)
    set(ARCAL_JSON_PLUGIN_arcal arcal_externalizer_json)
    set(ARCAL_GENERATED_TARGET_arcal arcal_generated)
    set(ARCAL_LACAL_OUTPUT_arcal arlacal-server)
    set(ARCAL_EXAMPLE_PREFIX_arcal arcal)
else()
    message(STATUS "Full generated source tree not present; skipping main arcal targets.")
    set(ARCAL_CAL_VARIANTS "")
endif()

# ---------------------------------------------------------------------------
# Externalizer plugins
# ---------------------------------------------------------------------------

if(ARCAL_HAVE_FULL_GENERATED)
    add_subdirectory(externalizer/json)
endif()

foreach(subset_config IN LISTS ARCAL_SUBSET_CONFIGS)
    arcal_add_subset("${subset_config}")
endforeach()

if(NOT DEFINED ARCAL_CERT_CAL_LIB OR ARCAL_CERT_CAL_LIB STREQUAL "")
    set(ARCAL_CERT_CAL_LIB "arcal" CACHE STRING "")
endif()

# ---------------------------------------------------------------------------
# Language-Agnostic CAL server
# ---------------------------------------------------------------------------

option(ARCAL_BUILD_LACAL "Build ARCAL Language-Agnostic CAL WebSocket server" ON)

if(ARCAL_BUILD_LACAL)
    add_subdirectory(lacal)
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------

if(ARCAL_BUILD_EXAMPLES)
    add_subdirectory(examples/amti_service_demo)
    add_subdirectory(examples/smti_service_demo)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

set(ARCAL_STANDALONE_TEST_CAL_LIB "${ARCAL_CERT_CAL_LIB}")
if(ARCAL_STANDALONE_TEST_CAL_LIB STREQUAL "arcal")
    set(ARCAL_STANDALONE_TEST_JSON_PLUGIN "arcal_externalizer_json")
else()
    set(ARCAL_STANDALONE_TEST_JSON_PLUGIN "${ARCAL_STANDALONE_TEST_CAL_LIB}_externalizer_json")
endif()

if(ARCAL_BUILD_TESTS)
    if(NOT TARGET ${ARCAL_STANDALONE_TEST_CAL_LIB})
        message(FATAL_ERROR
            "Standalone test CAL target '${ARCAL_STANDALONE_TEST_CAL_LIB}' is not defined.")
    endif()
    if(NOT TARGET ${ARCAL_STANDALONE_TEST_JSON_PLUGIN})
        message(FATAL_ERROR
            "Standalone test JSON plugin target '${ARCAL_STANDALONE_TEST_JSON_PLUGIN}' is not defined.")
    endif()
    add_subdirectory(test/conform)
    add_subdirectory(test/config)
    add_subdirectory(test/json)
    add_subdirectory(test/cdr)
endif()

if(ARCAL_BUILD_TESTS OR ARCAL_BUILD_E2E_TESTS)
    set(ARCAL_CERT_BUILD_CERT       ${ARCAL_BUILD_TESTS}     CACHE BOOL   "" FORCE)
    set(ARCAL_CERT_BUILD_E2E        ${ARCAL_BUILD_E2E_TESTS} CACHE BOOL   "" FORCE)
    set(ARCAL_CERT_TEST_ENV
        "CYCLONEDDS_URI=file://${CMAKE_SOURCE_DIR}/test/e2e/cyclonedds_localhost.xml;ARCAL_CONFIG=NONE"
        CACHE STRING "" FORCE)
    add_subdirectory(test/arcal-cert)
endif()

if(ARCAL_BUILD_INSTALL_TESTS)
    add_subdirectory(test/install)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

if(ARCAL_HAVE_FULL_GENERATED)
    install(TARGETS arcal arcal_externalizer_json
        EXPORT arcalTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(
        DIRECTORY externalizer/json/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/arcal/externalizer/json
        FILES_MATCHING
            PATTERN "*.h"
    )

    configure_package_config_file(
        ${CMAKE_SOURCE_DIR}/cmake/arcalConfig.cmake.in
        ${CMAKE_BINARY_DIR}/arcalConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arcal
    )

    write_basic_package_version_file(
        ${CMAKE_BINARY_DIR}/arcalConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(
        EXPORT arcalTargets
        FILE arcalTargets.cmake
        NAMESPACE arcal::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arcal
    )

    install(
        FILES
            ${CMAKE_BINARY_DIR}/arcalConfig.cmake
            ${CMAKE_BINARY_DIR}/arcalConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/arcal
    )

    install(
        FILES
            ${CMAKE_SOURCE_DIR}/README.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/arcal
    )

    install(
        FILES ${CMAKE_SOURCE_DIR}/lacal/README.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/arcal
        RENAME README_ARLACAL.md
    )

    install(
        FILES ${CMAKE_SOURCE_DIR}/test/e2e/cyclonedds_localhost.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/arcal/examples
    )

    install(
        DIRECTORY examples/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/arcal/examples
        FILES_MATCHING
            PATTERN "*.cpp"
            PATTERN "*.md"
            PATTERN "*.json"
            PATTERN "CMakeLists.txt"
    )
endif()
