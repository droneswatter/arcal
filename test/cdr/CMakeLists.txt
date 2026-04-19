add_executable(cdr_roundtrip cdr_roundtrip.cpp)
target_link_libraries(cdr_roundtrip PRIVATE arcal arcal_externalizer_cdr)
add_test(NAME CDR-roundtrip COMMAND cdr_roundtrip)
