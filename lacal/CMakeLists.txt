find_package(Threads REQUIRED)
find_package(Boost REQUIRED)

add_executable(arlacal-server
    src/main.cpp
)

add_dependencies(arlacal-server arcal_generated arcal_idl)

target_include_directories(arlacal-server PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/dds
)

target_link_libraries(arlacal-server PRIVATE
    arcal
    arcal_externalizer_json
    Boost::headers
    CycloneDDS::ddsc
    nlohmann_json::nlohmann_json
    Threads::Threads
)

set_target_properties(arlacal-server PROPERTIES
    BUILD_RPATH
        "${CMAKE_BINARY_DIR};${CMAKE_BINARY_DIR}/externalizer/json"
)

install(TARGETS arlacal-server RUNTIME DESTINATION bin)
