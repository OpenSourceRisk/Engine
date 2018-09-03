cmake_minimum_required(VERSION 3.8)

project(QuantExt CXX)

include("../cmake/writeAll.cmake")
include("../cmake/writeTestSuiteMain.cmake")
include("../cmake/commonSettings.cmake")

find_package (Boost COMPONENTS unit_test_framework REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${QUANTLIB_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

link_directories("${QUANTLIB_SOURCE_DIR}/build/ql")

add_subdirectory("qle")
add_subdirectory("test")
