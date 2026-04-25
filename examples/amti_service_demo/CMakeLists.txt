add_executable(arcal_amti_service_demo main.cpp)
target_link_libraries(arcal_amti_service_demo PRIVATE arcal)
target_include_directories(arcal_amti_service_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
