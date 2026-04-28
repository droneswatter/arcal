function(add_json_test name)
    add_executable(json_${name} json_${name}.cpp)
    target_link_libraries(json_${name} PRIVATE
        ${ARCAL_STANDALONE_TEST_CAL_LIB}
        ${ARCAL_STANDALONE_TEST_JSON_PLUGIN}
    )
    if(TARGET arcal_test_suite_all)
        add_dependencies(arcal_test_suite_all json_${name})
    endif()
    add_test(NAME JSON-${name} COMMAND json_${name})
    set_tests_properties(JSON-${name} PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:${ARCAL_STANDALONE_TEST_JSON_PLUGIN}>:$ENV{LD_LIBRARY_PATH}")
endfunction()

add_json_test(write_fields)
add_json_test(write_readonly)
add_json_test(read_roundtrip)
add_json_test(read_errors)
