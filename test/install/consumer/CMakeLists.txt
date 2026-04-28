cmake_minimum_required(VERSION 3.21)
project(arcal_install_consumer_smoke LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ARCAL_CONSUMER_PACKAGE "arcal" CACHE STRING "Installed ARCAL package name")
set(ARCAL_CONSUMER_TARGET "arcal::arcal" CACHE STRING "Installed ARCAL CMake target")

find_package(${ARCAL_CONSUMER_PACKAGE} CONFIG REQUIRED)

add_executable(arcal_consumer_smoke main.cpp)
target_link_libraries(arcal_consumer_smoke PRIVATE ${ARCAL_CONSUMER_TARGET})

add_test(NAME arcal_consumer_smoke COMMAND arcal_consumer_smoke)
