cmake_minimum_required(VERSION 3.8)

project(OREAnalytics CXX)

include("../cmake/writeAll.cmake")
include("../cmake/writeTestSuiteMain.cmake")
include("../cmake/commonSettings.cmake")

find_package (Boost COMPONENTS unit_test_framework regex date_time serialization filesystem REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${QUANTLIB_SOURCE_DIR})
include_directories(${QUANTEXT_SOURCE_DIR})
include_directories(${OREDATA_SOURCE_DIR})
include_directories(${ORETEST_SOURCE_DIR})
include_directories(${RAPIDXML_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# specify library search path for all-in builds ...
link_directories("${CMAKE_BINARY_DIR}/QuantLib/ql")
link_directories("${CMAKE_BINARY_DIR}/QuantExt/qle")
link_directories("${CMAKE_BINARY_DIR}/OREData/ored")
# ... and for local builds
link_directories("${QUANTLIB_SOURCE_DIR}/build/ql")
link_directories("${QUANTEXT_SOURCE_DIR}/build/qle")
link_directories("${OREDATA_SOURCE_DIR}/build/ored")

add_subdirectory("orea")
add_subdirectory("test")
