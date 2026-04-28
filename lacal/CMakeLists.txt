find_package(Threads REQUIRED)
find_package(Boost REQUIRED)

function(arcal_add_lacal_server variant_target)
    set(json_plugin "${ARCAL_JSON_PLUGIN_${variant_target}}")
    set(generated_target "${ARCAL_GENERATED_TARGET_${variant_target}}")
    set(output_name "${ARCAL_LACAL_OUTPUT_${variant_target}}")
    if(NOT output_name)
        set(output_name "${variant_target}-lacal-server")
    endif()

    set(server_target "${output_name}")
    string(MAKE_C_IDENTIFIER "${server_target}" server_target)

    add_executable(${server_target}
        src/ClientSession.cpp
        src/main.cpp
        src/Protocol.cpp
        src/Server.cpp
        src/TopicMonitor.cpp
    )

    add_dependencies(${server_target} ${generated_target} arcal_idl)

    target_include_directories(${server_target} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/dds
    )

    target_link_libraries(${server_target} PRIVATE
        ${variant_target}
        ${json_plugin}
        Boost::headers
        CycloneDDS::ddsc
        nlohmann_json::nlohmann_json
        Threads::Threads
    )

    set_target_properties(${server_target} PROPERTIES
        OUTPUT_NAME "${output_name}"
        BUILD_RPATH
            "${CMAKE_BINARY_DIR};${CMAKE_BINARY_DIR}/externalizer/json;$<TARGET_FILE_DIR:${json_plugin}>"
        INSTALL_RPATH
            "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )

    install(TARGETS ${server_target} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    set(ARCAL_LACAL_TARGET_${variant_target} "${server_target}" PARENT_SCOPE)
endfunction()

foreach(_variant IN LISTS ARCAL_CAL_VARIANTS)
    arcal_add_lacal_server(${_variant})
endforeach()

if(ARCAL_BUILD_TESTS OR ARCAL_BUILD_E2E_TESTS)
    add_executable(lacal_protocol_test
        src/Protocol.cpp
        test/protocol_test.cpp
    )

    target_include_directories(lacal_protocol_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(lacal_protocol_test PRIVATE
        nlohmann_json::nlohmann_json
    )
    if(TARGET arcal_test_suite_all)
        add_dependencies(arcal_test_suite_all lacal_protocol_test)
    endif()

    add_test(NAME LACAL-protocol COMMAND lacal_protocol_test)
    set_tests_properties(LACAL-protocol PROPERTIES
        TIMEOUT 5
    )

    add_executable(lacal_owp_smoke_test
        test/owp_smoke_test.cpp
    )

    target_link_libraries(lacal_owp_smoke_test PRIVATE
        Boost::headers
        Threads::Threads
    )
    if(TARGET arcal_test_suite_all)
        add_dependencies(arcal_test_suite_all lacal_owp_smoke_test)
    endif()

    foreach(_variant IN LISTS ARCAL_CAL_VARIANTS)
        if(NOT _variant STREQUAL ARCAL_CERT_CAL_LIB)
            continue()
        endif()
        set(_server_target "${ARCAL_LACAL_TARGET_${_variant}}")
        set(_test_name "LACAL-owp-smoke")
        if(NOT _variant STREQUAL "arcal")
            set(_test_name "LACAL-owp-smoke-${_variant}")
        endif()
        add_test(
            NAME ${_test_name}
            COMMAND lacal_owp_smoke_test $<TARGET_FILE:${_server_target}>)

        set_tests_properties(${_test_name} PROPERTIES
            TIMEOUT 10
            ENVIRONMENT
                "CYCLONEDDS_URI=file://${PROJECT_SOURCE_DIR}/test/e2e/cyclonedds_localhost.xml")
    endforeach()
endif()
