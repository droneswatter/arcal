add_executable(e2e_pub pub_action_command.cpp)
target_link_libraries(e2e_pub PRIVATE arcal arcal_externalizer_cdr)
target_include_directories(e2e_pub PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_executable(e2e_sub sub_action_command.cpp)
target_link_libraries(e2e_sub PRIVATE arcal arcal_externalizer_cdr)
target_include_directories(e2e_sub PRIVATE ${CMAKE_SOURCE_DIR}/src)

configure_file(run_e2e_action_command.sh.in run_e2e_action_command.sh @ONLY)
file(CHMOD ${CMAKE_CURRENT_BINARY_DIR}/run_e2e_action_command.sh
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)

add_test(
    NAME    E2E-ActionCommand-PubSub
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/run_e2e_action_command.sh
)
set_tests_properties(E2E-ActionCommand-PubSub PROPERTIES TIMEOUT 8)

# ---------------------------------------------------------------------------

add_executable(e2e_topic_isolation topic_isolation.cpp)
target_link_libraries(e2e_topic_isolation PRIVATE arcal arcal_externalizer_cdr)
target_include_directories(e2e_topic_isolation PRIVATE ${CMAKE_SOURCE_DIR}/src)

configure_file(run_e2e_topic_isolation.sh.in run_e2e_topic_isolation.sh @ONLY)
file(CHMOD ${CMAKE_CURRENT_BINARY_DIR}/run_e2e_topic_isolation.sh
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)

add_test(
    NAME    E2E-TopicIsolation
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/run_e2e_topic_isolation.sh
)
set_tests_properties(E2E-TopicIsolation PROPERTIES TIMEOUT 8)

add_executable(e2e_pub_rich pub_rich.cpp)
target_link_libraries(e2e_pub_rich PRIVATE arcal arcal_externalizer_cdr)
target_include_directories(e2e_pub_rich PRIVATE ${CMAKE_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
# Single-process E2E tests — use CTest ENVIRONMENT for CYCLONEDDS_URI

set(_E2E_ENV
    "CYCLONEDDS_URI=file://${CMAKE_CURRENT_SOURCE_DIR}/cyclonedds_localhost.xml")

foreach(_test IN ITEMS
    content_fidelity
    multi_message
    listener_lifecycle
    two_writers_one_reader
    two_readers_one_writer
    readnowait_empty
    read_timeout)

    add_executable(e2e_${_test} ${_test}.cpp)
    target_link_libraries(e2e_${_test} PRIVATE arcal arcal_externalizer_cdr)
    target_include_directories(e2e_${_test} PRIVATE ${CMAKE_SOURCE_DIR}/src)

    string(REPLACE "_" "-" _test_name "${_test}")
    add_test(NAME E2E-${_test_name} COMMAND e2e_${_test})
    set_tests_properties(E2E-${_test_name} PROPERTIES
        TIMEOUT     8
        ENVIRONMENT "${_E2E_ENV}")
endforeach()
