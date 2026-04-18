file(GLOB_RECURSE CDR_GENERATED_SOURCES
    ${PROJECT_SOURCE_DIR}/src/generated/*_cdr.cpp
    ${PROJECT_SOURCE_DIR}/src/generated/cdr_register_all.cpp
)

add_library(arcal_externalizer_cdr SHARED
    CdrExternalizer.cpp
    ExternalizerLoader.cpp
    ${CDR_GENERATED_SOURCES}
)

target_include_directories(arcal_externalizer_cdr PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(arcal_externalizer_cdr PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE ${ARCAL_UNITY_BATCH_SIZE}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

target_precompile_headers(arcal_externalizer_cdr PRIVATE
    CdrPrimitives.h
    CdrRegistry.h
    "${PROJECT_SOURCE_DIR}/include/uci/base/BoundedList.h"
    "${PROJECT_SOURCE_DIR}/include/uci/base/SimpleList.h"
)

# Generated CDR files are never debugged directly; line tables are enough for
# stack traces and cost a fraction of full DWARF (-g).
# -pipe avoids temp files between GCC internal stages (preprocess → compile → assemble).
target_compile_options(arcal_externalizer_cdr PRIVATE -g1 -pipe)

target_link_libraries(arcal_externalizer_cdr PRIVATE arcal)
