set(GIT_HASH "unknown")

find_package(Git QUIET)

if(GIT_FOUND)
    message(STATUS "Git was found, running git log to extract hash")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%h
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    message(STATUS "Git was not found, can not extract hash")
endif()

message(STATUS "Git hash is ${GIT_HASH}")

configure_file(${IN_FILE} ${OUT_FILE} @ONLY)
