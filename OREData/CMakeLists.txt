cmake_minimum_required(VERSION 3.8)

project(OREData CXX)

enable_testing()

include("../cmake/writeAll.cmake")
include("../cmake/writeTestSuiteMain.cmake")
include("../cmake/commonSettings.cmake")

find_package (Boost COMPONENTS unit_test_framework regex date_time REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${QUANTLIB_SOURCE_DIR})
include_directories(${QUANTEXT_SOURCE_DIR})
include_directories(${ORETEST_SOURCE_DIR})
include_directories(${RAPIDXML_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

link_directories("${QUANTLIB_SOURCE_DIR}/build/ql") 
link_directories("${QUANTEXT_SOURCE_DIR}/build/qle")

add_subdirectory("ored")
add_subdirectory("test")
