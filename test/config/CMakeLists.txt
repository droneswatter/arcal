add_executable(arcal_config_identity cal_config_identity.cpp)
target_link_libraries(arcal_config_identity PRIVATE arcal)
target_include_directories(arcal_config_identity PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(NAME CONFIG-identity COMMAND arcal_config_identity "${CMAKE_CURRENT_BINARY_DIR}/cal_config_identity.json")
set_tests_properties(CONFIG-identity PROPERTIES
    ENVIRONMENT "CYCLONEDDS_URI=file://${CMAKE_SOURCE_DIR}/test/e2e/cyclonedds_localhost.xml"
)
