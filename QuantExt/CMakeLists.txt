cmake_minimum_required(VERSION 3.8)

project(QuantExt CXX)

enable_testing()

include("../cmake/writeAll.cmake")
include("../cmake/writeTestSuiteMain.cmake")
include("../cmake/commonSettings.cmake")

get_library_name("QuantExt" QLE_LIB_NAME)
get_library_name("QuantLib" QL_LIB_NAME)
configure_msvc_runtime()

#find_package (Boost COMPONENTS unit_test_framework REQUIRED)
find_package (Boost COMPONENTS unit_test_framework date_time system thread REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${QUANTLIB_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_link_directory_if_exists("${QUANTLIB_SOURCE_DIR}/build/ql")

add_subdirectory("qle")
add_subdirectory("test")
