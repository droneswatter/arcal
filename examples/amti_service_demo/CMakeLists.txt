foreach(_variant IN LISTS ARCAL_CAL_VARIANTS)
    set(_prefix "${ARCAL_EXAMPLE_PREFIX_${_variant}}")
    if(NOT _prefix)
        set(_prefix "${_variant}")
    endif()
    set(_target "${_prefix}_amti_service_demo")

    add_executable(${_target} main.cpp)
    target_link_libraries(${_target} PRIVATE ${_variant})
    target_include_directories(${_target} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    install(TARGETS ${_target} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endforeach()
