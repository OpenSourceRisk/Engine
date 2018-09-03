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

link_directories("${QUANTLIB_SOURCE_DIR}/build/ql")
link_directories("${QUANTEXT_SOURCE_DIR}/build/qle")
link_directories("${OREDATA_SOURCE_DIR}/build/ored")
link_directories("${OREANALYTICS_SOURCE_DIR}/build/orea")

find_package (Boost COMPONENTS unit_test_framework regex date_time serialization filesystem REQUIRED)

add_executable(ore ore.cpp)
target_link_libraries(ore OREAnalytics OREData QuantExt QuantLib ${Boost_LIBRARIES})
