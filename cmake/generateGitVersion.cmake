if(DEFINED GIT_HASH AND NOT GIT_HASH STREQUAL "")
    message(STATUS "Git hash supplied as ${GIT_HASH}")
else()
    find_package(Git QUIET)

    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%h
            OUTPUT_VARIABLE GIT_HASH
            RESULT_VARIABLE RETURN_CODE
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT RETURN_CODE EQUAL 0)
            set(GIT_HASH "unknown")
        endif()
    else()
        set(GIT_HASH "unknown")
    endif()
endif()

message(STATUS "Git hash is ${GIT_HASH}")
configure_file(${IN_FILE} ${OUT_FILE} @ONLY)
