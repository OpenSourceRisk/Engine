cmake_minimum_required(VERSION 3.15)


cmake_policy(SET CMP0091 NEW)


project(ORE CXX)

option(ORE_BUILD_DOC "Build documentation" ON)
option(ORE_BUILD_EXAMPLES "Build examples" ON)
option(ORE_BUILD_TESTS "Build test suite" ON)
option(ORE_BUILD_APP "Build app" ON)
option(ORE_USE_ZLIB "Use compression for boost::iostreams" OFF)

include(CTest)

include("cmake/commonSettings.cmake")

set(USE_GLOBAL_ORE_BUILD ON)

add_subdirectory("QuantLib")
add_subdirectory("QuantExt")
add_subdirectory("OREData")
add_subdirectory("OREAnalytics")
if (ORE_BUILD_APP)
    add_subdirectory("App")
endif()

# add examples testsuite
if (ORE_BUILD_EXAMPLES AND ORE_BUILD_TESTS)
add_test(NAME examples WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Examples COMMAND nosetests run_examples_testsuite.py)
endif()
