set(GIT_HASH "unknown")

find_package(Git QUIET)

if(GIT_FOUND)
  execute_process(COMMAND git log -1 --pretty=format:%h OUTPUT_VARIABLE GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()

message(STATUS "Git hash is ${GIT_HASH}")

configure_file(${IN_FILE} ${OUT_FILE} @ONLY)