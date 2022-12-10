cmake_minimum_required(VERSION 3.8)


cmake_policy(SET CMP0091 NEW)


project(ORE CXX)

option(BUILD_DOC "Build documentation" ON)

include(CTest)

include("cmake/commonSettings.cmake")

set(USE_GLOBAL_ORE_BUILD ON)

add_subdirectory("QuantLib")
add_subdirectory("QuantExt")
add_subdirectory("OREData")
add_subdirectory("OREAnalytics")
add_subdirectory("App")

# add examples testsuite
add_test(NAME examples WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Examples COMMAND nosetests run_examples_testsuite.py)
