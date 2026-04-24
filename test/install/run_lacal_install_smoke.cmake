if(NOT DEFINED PROJECT_BINARY_DIR)
    message(FATAL_ERROR "PROJECT_BINARY_DIR is required")
endif()
if(NOT DEFINED INSTALL_PREFIX)
    message(FATAL_ERROR "INSTALL_PREFIX is required")
endif()
if(NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "WORK_DIR is required")
endif()
if(NOT DEFINED LACAL_OWP_SMOKE_TEST)
    message(FATAL_ERROR "LACAL_OWP_SMOKE_TEST is required")
endif()

file(REMOVE_RECURSE "${INSTALL_PREFIX}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(_install_args --install "${PROJECT_BINARY_DIR}" --prefix "${INSTALL_PREFIX}")
if(TEST_CONFIG)
    list(APPEND _install_args --config "${TEST_CONFIG}")
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

set(_server_path "${INSTALL_PREFIX}/bin/arlacal-server")
set(_dds_config "${INSTALL_PREFIX}/share/arcal/examples/cyclonedds_localhost.xml")
if(NOT EXISTS "${_server_path}")
    message(FATAL_ERROR "installed arlacal-server not found: ${_server_path}")
endif()
if(NOT EXISTS "${_dds_config}")
    message(FATAL_ERROR "installed Cyclone DDS config not found: ${_dds_config}")
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

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "CYCLONEDDS_URI=file://${_dds_config}"
        "LD_LIBRARY_PATH=${_ld_library_path}"
        "${LACAL_OWP_SMOKE_TEST}" "${_server_path}"
    RESULT_VARIABLE _smoke_result
    OUTPUT_VARIABLE _smoke_output
    ERROR_VARIABLE _smoke_error
)
if(NOT _smoke_result EQUAL 0)
    message(FATAL_ERROR
        "installed arlacal-server smoke test failed with ${_smoke_result}\n"
        "${_smoke_output}\n${_smoke_error}")
endif()

message(STATUS "PASS INSTALL-lacal-server")
