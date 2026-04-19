foreach(file IN LISTS IDLC_SCRUB_FILES)
    if(EXISTS "${file}")
        file(READ "${file}" content)
        string(REGEX REPLACE
            "Source: [^\n\r]*arcal_payload\\.idl"
            "Source: arcal_payload.idl"
            content
            "${content}")
        file(WRITE "${file}" "${content}")
    endif()
endforeach()
