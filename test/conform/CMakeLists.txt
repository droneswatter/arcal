# Compile-only conformance suite — validates public API surface against OMSC-SPC-008 RevK.
# These files are built as an OBJECT library; successful compilation IS the test.
# No DDS, no linker, no test executable required.

add_library(arcal_conform OBJECT
    conf_accessorType.cpp
    conf_accessor.cpp
    conf_exception.cpp
    conf_status_listener.cpp
    conf_asb.cpp
    conf_enum.cpp
    conf_lifetime.cpp
    conf_externalizer.cpp
    conf_generated_global.cpp
    conf_generated_lifecycle_target.cpp
    conf_lists.cpp
    conf_uuid.cpp
    conf_utils.cpp
)

target_include_directories(arcal_conform PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# conf_asb.cpp needs the AbstractServiceBusConnection header which forward-declares
# the free functions — no link needed, just the include path above.

add_executable(arcal_conform_client_link conform_client_link.cpp)
target_link_libraries(arcal_conform_client_link PRIVATE arcal)
target_include_directories(arcal_conform_client_link PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_custom_target(arcal_conformance ALL
    DEPENDS arcal_conform arcal_conform_client_link
)

add_test(NAME CONFORM-client-link COMMAND arcal_conform_client_link)
set_tests_properties(CONFORM-client-link PROPERTIES
    ENVIRONMENT "ARCAL_CONFIG=NONE"
)
