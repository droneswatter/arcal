function(add_json_test name)
    add_executable(json_${name} json_${name}.cpp)
    target_link_libraries(json_${name} PRIVATE arcal arcal_externalizer_json)
    add_test(NAME JSON-${name} COMMAND json_${name})
endfunction()

add_json_test(write_fields)
add_json_test(write_readonly)
