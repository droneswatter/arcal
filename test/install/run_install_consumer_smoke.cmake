if(NOT DEFINED PROJECT_BINARY_DIR)
    message(FATAL_ERROR "PROJECT_BINARY_DIR is required")
endif()
if(NOT DEFINED INSTALL_PREFIX)
    message(FATAL_ERROR "INSTALL_PREFIX is required")
endif()
if(NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "WORK_DIR is required")
endif()
if(NOT DEFINED TEST_SOURCE_DIR)
    message(FATAL_ERROR "TEST_SOURCE_DIR is required")
endif()
if(NOT DEFINED ARCAL_CONSUMER_PACKAGE)
    set(ARCAL_CONSUMER_PACKAGE arcal)
endif()
if(NOT DEFINED ARCAL_CONSUMER_TARGET)
    set(ARCAL_CONSUMER_TARGET arcal::arcal)
endif()

set(CONSUMER_SOURCE_DIR "${TEST_SOURCE_DIR}/consumer")
set(CONSUMER_BINARY_DIR "${WORK_DIR}/consumer-build")

file(REMOVE_RECURSE "${INSTALL_PREFIX}" "${CONSUMER_BINARY_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(_install_args --install "${PROJECT_BINARY_DIR}" --prefix "${INSTALL_PREFIX}")
if(TEST_CONFIG)
    list(APPEND _install_args --config "${TEST_CONFIG}")
endif()
if(TEST_TYPE)
    list(APPEND _install_args --component "${TEST_TYPE}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" ${_install_args}
    RESULT_VARIABLE _install_result
    OUTPUT_VARIABLE _install_output
    ERROR_VARIABLE _install_error
)
if(NOT _install_result EQUAL 0)
    message(FATAL_ERROR
        "cmake --install failed with ${_install_result}\n"
        "${_install_output}\n${_install_error}")
endif()

set(_prefix_path "${INSTALL_PREFIX}")
if(DEPENDENCY_PREFIX_PATH)
    list(APPEND _prefix_path ${DEPENDENCY_PREFIX_PATH})
endif()

set(_configure_args
    -S "${CONSUMER_SOURCE_DIR}"
    -B "${CONSUMER_BINARY_DIR}"
    "-DCMAKE_PREFIX_PATH=${_prefix_path}"
    "-DARCAL_CONSUMER_PACKAGE=${ARCAL_CONSUMER_PACKAGE}"
    "-DARCAL_CONSUMER_TARGET=${ARCAL_CONSUMER_TARGET}"
)
if(TEST_GENERATOR)
    list(APPEND _configure_args -G "${TEST_GENERATOR}")
endif()
if(TEST_BUILD_TYPE)
    list(APPEND _configure_args "-DCMAKE_BUILD_TYPE=${TEST_BUILD_TYPE}")
endif()
if(TEST_CXX_COMPILER)
    list(APPEND _configure_args "-DCMAKE_CXX_COMPILER=${TEST_CXX_COMPILER}")
endif()
if(TEST_TOOLCHAIN_FILE)
    list(APPEND _configure_args "-DCMAKE_TOOLCHAIN_FILE=${TEST_TOOLCHAIN_FILE}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" ${_configure_args}
    RESULT_VARIABLE _configure_result
    OUTPUT_VARIABLE _configure_output
    ERROR_VARIABLE _configure_error
)
if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR
        "consumer configure failed with ${_configure_result}\n"
        "${_configure_output}\n${_configure_error}")
endif()

set(_build_args --build "${CONSUMER_BINARY_DIR}")
if(TEST_CONFIG)
    list(APPEND _build_args --config "${TEST_CONFIG}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" ${_build_args}
    RESULT_VARIABLE _build_result
    OUTPUT_VARIABLE _build_output
    ERROR_VARIABLE _build_error
)
if(NOT _build_result EQUAL 0)
    message(FATAL_ERROR
        "consumer build failed with ${_build_result}\n"
        "${_build_output}\n${_build_error}")
endif()

set(_runtime_paths
    "${INSTALL_PREFIX}/lib"
    "${INSTALL_PREFIX}/lib64"
)
foreach(_dependency_prefix IN LISTS DEPENDENCY_PREFIX_PATH)
    list(APPEND _runtime_paths
        "${_dependency_prefix}/lib"
        "${_dependency_prefix}/lib64"
    )
endforeach()
if(DEFINED ENV{LD_LIBRARY_PATH} AND NOT "$ENV{LD_LIBRARY_PATH}" STREQUAL "")
    list(APPEND _runtime_paths "$ENV{LD_LIBRARY_PATH}")
endif()
list(JOIN _runtime_paths ":" _ld_library_path)

set(_ctest_args --test-dir "${CONSUMER_BINARY_DIR}" --output-on-failure)
if(TEST_CONFIG)
    list(APPEND _ctest_args -C "${TEST_CONFIG}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "LD_LIBRARY_PATH=${_ld_library_path}"
        "${CTEST_COMMAND}" ${_ctest_args}
    RESULT_VARIABLE _ctest_result
    OUTPUT_VARIABLE _ctest_output
    ERROR_VARIABLE _ctest_error
)
if(NOT _ctest_result EQUAL 0)
    message(FATAL_ERROR
        "consumer smoke test failed with ${_ctest_result}\n"
        "${_ctest_output}\n${_ctest_error}")
endif()

message(STATUS "PASS INSTALL-consumer-cmake")
