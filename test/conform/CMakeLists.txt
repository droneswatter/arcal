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
)

target_include_directories(arcal_conform PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# conf_asb.cpp needs the AbstractServiceBusConnection header which forward-declares
# the free functions — no link needed, just the include path above.

add_executable(arcal_conform_client_link conform_client_link.cpp)
target_link_libraries(arcal_conform_client_link PRIVATE
    arcal
    arcal_externalizer_cdr
)
target_include_directories(arcal_conform_client_link PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
