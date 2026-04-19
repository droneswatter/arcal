file(GLOB_RECURSE JSON_GENERATED_SOURCES
    ${PROJECT_SOURCE_DIR}/src/generated/json/*_json.cpp
    ${PROJECT_SOURCE_DIR}/src/generated/json/json_register_all.cpp
)

add_library(arcal_externalizer_json SHARED
    JsonExternalizer.cpp
    ExternalizerLoader.cpp
    ${JSON_GENERATED_SOURCES}
)

target_include_directories(arcal_externalizer_json PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(arcal_externalizer_json PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE ${ARCAL_UNITY_BATCH_SIZE}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

target_precompile_headers(arcal_externalizer_json PRIVATE
    JsonPrimitives.h
    JsonRegistry.h
    "${PROJECT_SOURCE_DIR}/include/uci/base/BoundedList.h"
    "${PROJECT_SOURCE_DIR}/include/uci/base/SimpleList.h"
)

target_compile_options(arcal_externalizer_json PRIVATE -g1 -pipe)
target_link_libraries(arcal_externalizer_json PRIVATE arcal)
