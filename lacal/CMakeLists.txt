find_package(Threads REQUIRED)
find_package(Boost REQUIRED)

add_executable(arlacal-server
    src/main.cpp
)

add_dependencies(arlacal-server arcal_generated arcal_idl)

target_include_directories(arlacal-server PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/dds
)

target_link_libraries(arlacal-server PRIVATE
    arcal
    arcal_externalizer_json
    Boost::headers
    CycloneDDS::ddsc
    nlohmann_json::nlohmann_json
    Threads::Threads
)

set_target_properties(arlacal-server PROPERTIES
    BUILD_RPATH
        "${CMAKE_BINARY_DIR};${CMAKE_BINARY_DIR}/externalizer/json"
    INSTALL_RPATH
        "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
)

install(TARGETS arlacal-server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(ARCAL_BUILD_TESTS OR ARCAL_BUILD_E2E_TESTS)
    add_executable(lacal_owp_smoke_test
        test/owp_smoke_test.cpp
    )

    target_link_libraries(lacal_owp_smoke_test PRIVATE
        Boost::headers
        Threads::Threads
    )

    add_test(
        NAME LACAL-owp-smoke
        COMMAND lacal_owp_smoke_test $<TARGET_FILE:arlacal-server>)

    set_tests_properties(LACAL-owp-smoke PROPERTIES
        TIMEOUT 10
        ENVIRONMENT
            "CYCLONEDDS_URI=file://${PROJECT_SOURCE_DIR}/test/e2e/cyclonedds_localhost.xml")
endif()
