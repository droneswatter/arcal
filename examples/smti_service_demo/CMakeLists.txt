add_executable(arcal_smti_service_demo main.cpp)
target_link_libraries(arcal_smti_service_demo PRIVATE arcal)
target_include_directories(arcal_smti_service_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
