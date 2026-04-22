function(add_json_test name)
    add_executable(json_${name} json_${name}.cpp)
    target_link_libraries(json_${name} PRIVATE arcal arcal_externalizer_json)
    add_test(NAME JSON-${name} COMMAND json_${name})
    set_tests_properties(JSON-${name} PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:arcal_externalizer_json>:$ENV{LD_LIBRARY_PATH}")
endfunction()

add_json_test(write_fields)
add_json_test(write_readonly)
add_json_test(read_roundtrip)
add_json_test(read_errors)
