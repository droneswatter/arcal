set(_ARCAL_DDS_TEST_ENV
    "CYCLONEDDS_URI=file://${CMAKE_SOURCE_DIR}/test/e2e/cyclonedds_localhost.xml")

function(add_cert_test cert_id)
    add_executable(cert_${cert_id} ${cert_id}.cpp)
    target_link_libraries(cert_${cert_id} PRIVATE arcal)
    add_test(NAME CERT-${cert_id} COMMAND cert_${cert_id})
    set_tests_properties(CERT-${cert_id} PROPERTIES
        TIMEOUT     8
        ENVIRONMENT "${_ARCAL_DDS_TEST_ENV}")
endfunction()

# For tests that create DDS topics/readers/writers and need the CDR externalizer.
function(add_cert_test_ext cert_id)
    add_executable(cert_${cert_id} ${cert_id}.cpp)
    target_link_libraries(cert_${cert_id} PRIVATE arcal arcal_externalizer_cdr)
    add_test(NAME CERT-${cert_id} COMMAND cert_${cert_id})
    set_tests_properties(CERT-${cert_id} PROPERTIES
        TIMEOUT     8
        ENVIRONMENT "${_ARCAL_DDS_TEST_ENV}")
endfunction()

# CERT CAL-016015: Multiple CAL Clients in single address space get distinct UUIDs
add_cert_test(CAL-016015)

# CERT CAL-016366: Status listener is called immediately on registration with current state
add_cert_test(CAL-016366)

# CERT CAL-005201: Mechanism to obtain an initialized CAL instance
add_cert_test(CAL-005201)

# CERT CAL-005202: Single CAL instance per unique Service + ASB Identifier combination
add_cert_test(CAL-005202)

# CERT CAL-005203: Mechanism to retrieve System/Service/Subsystem/Component/Capability UUIDs
add_cert_test(CAL-005203)

# CERT CAL-005204: Error reported to CAL Client when initialization fails
add_cert_test(CAL-005204)

# CERT CAL-005208: Topic associated with one and only one CAL Message type
add_cert_test_ext(CAL-005208)

# CERT CAL-005209: Client Topics mapped to applicable CAL Topics
add_cert_test_ext(CAL-005209)

# CERT CAL-005210: Independent QoS settings per Client Topic
add_cert_test_ext(CAL-005210)
