# Run this with CMake in "script mode" (-P flag) at build time

# in case Git is not available, we default to "unknown"
set(GIT_HASH "unknown")

# find Git and if available set GIT_HASH variable
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND git log -1 --pretty=format:%h
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

message(STATUS "Git hash is ${GIT_HASH}")

# generate file version.hpp based on version.hpp.in
configure_file(
    ${IN_FILE}
    ${OUT_FILE}
    @ONLY
)