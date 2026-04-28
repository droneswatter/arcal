set(ARCAL_INSTALL_SMOKE_ROOT "${CMAKE_BINARY_DIR}/install-smoke")

function(arcal_add_install_smoke_test name script)
    set(options)
    set(oneValueArgs PACKAGE_NAME TARGET_NAME TEST_TYPE)
    set(multiValueArgs)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT ARG_PACKAGE_NAME)
        set(ARG_PACKAGE_NAME arcal)
    endif()
    if(NOT ARG_TARGET_NAME)
        set(ARG_TARGET_NAME arcal::arcal)
    endif()
    add_test(
        NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            "-DPROJECT_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DINSTALL_PREFIX=${ARCAL_INSTALL_SMOKE_ROOT}/${name}/prefix"
            "-DWORK_DIR=${ARCAL_INSTALL_SMOKE_ROOT}/${name}"
            "-DTEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
            "-DTEST_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DTEST_CONFIG=$<CONFIG>"
            "-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DTEST_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
            "-DDEPENDENCY_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
            "-DARCAL_CONSUMER_PACKAGE=${ARG_PACKAGE_NAME}"
            "-DARCAL_CONSUMER_TARGET=${ARG_TARGET_NAME}"
            "-DTEST_TYPE=${ARG_TEST_TYPE}"
            "-DCTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/${script}"
    )
    set_tests_properties(${name} PROPERTIES
        LABELS "install"
        TIMEOUT 120
    )
endfunction()

foreach(_variant IN LISTS ARCAL_CAL_VARIANTS)
    if(NOT _variant STREQUAL ARCAL_CERT_CAL_LIB)
        continue()
    endif()
    if(_variant STREQUAL "arcal")
        arcal_add_install_smoke_test(
            INSTALL-consumer-cmake
            run_install_consumer_smoke.cmake
            PACKAGE_NAME arcal
            TARGET_NAME arcal::arcal
        )
    else()
        string(REPLACE "_" "-" _package "${_variant}")
        arcal_add_install_smoke_test(
            INSTALL-consumer-cmake-${_variant}
            run_install_consumer_smoke.cmake
            PACKAGE_NAME ${_package}
            TARGET_NAME ${_variant}::${_variant}
            TEST_TYPE ${_variant}
        )
    endif()
endforeach()

if(TARGET lacal_owp_smoke_test)
foreach(_variant IN LISTS ARCAL_CAL_VARIANTS)
    if(NOT _variant STREQUAL ARCAL_CERT_CAL_LIB)
        continue()
    endif()
    set(_server_target "${ARCAL_LACAL_TARGET_${_variant}}")
    if(NOT TARGET ${_server_target})
        continue()
    endif()
    set(_install_test_name "INSTALL-lacal-server")
    if(NOT _variant STREQUAL "arcal")
        set(_install_test_name "INSTALL-lacal-server-${_variant}")
    endif()
    add_test(
        NAME ${_install_test_name}
        COMMAND ${CMAKE_COMMAND}
            "-DPROJECT_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DINSTALL_PREFIX=${ARCAL_INSTALL_SMOKE_ROOT}/${_install_test_name}/prefix"
            "-DWORK_DIR=${ARCAL_INSTALL_SMOKE_ROOT}/${_install_test_name}"
            "-DTEST_CONFIG=$<CONFIG>"
            "-DLACAL_OWP_SMOKE_TEST=$<TARGET_FILE:lacal_owp_smoke_test>"
            "-DLACAL_SERVER_NAME=$<TARGET_FILE_NAME:${_server_target}>"
            "-DDEPENDENCY_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_lacal_install_smoke.cmake"
    )
    set_tests_properties(${_install_test_name} PROPERTIES
        LABELS "install"
        TIMEOUT 30
    )
endforeach()
endif()
