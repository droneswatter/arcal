set(ARCAL_INSTALL_SMOKE_ROOT "${CMAKE_BINARY_DIR}/install-smoke")

function(arcal_add_install_smoke_test name script)
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
            "-DCTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/${script}"
    )
    set_tests_properties(${name} PROPERTIES
        LABELS "install"
        TIMEOUT 120
    )
endfunction()

arcal_add_install_smoke_test(
    INSTALL-consumer-cmake
    run_install_consumer_smoke.cmake
)

if(TARGET arlacal-server AND TARGET lacal_owp_smoke_test)
    add_test(
        NAME INSTALL-lacal-server
        COMMAND ${CMAKE_COMMAND}
            "-DPROJECT_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DINSTALL_PREFIX=${ARCAL_INSTALL_SMOKE_ROOT}/INSTALL-lacal-server/prefix"
            "-DWORK_DIR=${ARCAL_INSTALL_SMOKE_ROOT}/INSTALL-lacal-server"
            "-DTEST_CONFIG=$<CONFIG>"
            "-DLACAL_OWP_SMOKE_TEST=$<TARGET_FILE:lacal_owp_smoke_test>"
            "-DDEPENDENCY_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_lacal_install_smoke.cmake"
    )
    set_tests_properties(INSTALL-lacal-server PROPERTIES
        LABELS "install"
        TIMEOUT 30
    )
endif()
