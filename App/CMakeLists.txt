cmake_minimum_required(VERSION 3.8)

project(App CXX)

include("../cmake/writeAll.cmake")
include("../cmake/writeTestSuiteMain.cmake")
include("../cmake/commonSettings.cmake")

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${QUANTLIB_SOURCE_DIR})
include_directories(${QUANTEXT_SOURCE_DIR})
include_directories(${OREDATA_SOURCE_DIR})
include_directories(${OREANALYTICS_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_link_directory_if_exists("${QUANTLIB_SOURCE_DIR}/build/ql")
add_link_directory_if_exists("${QUANTEXT_SOURCE_DIR}/build/qle")
add_link_directory_if_exists("${OREDATA_SOURCE_DIR}/build/ored")
add_link_directory_if_exists("${OREANALYTICS_SOURCE_DIR}/build/orea")

find_package (Boost COMPONENTS unit_test_framework regex date_time serialization filesystem REQUIRED)

add_executable(ore ore.cpp)
target_link_libraries(ore OREAnalytics OREData QuantExt QuantLib ${Boost_LIBRARIES})
