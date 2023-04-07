cmake_minimum_required(VERSION 3.15)

project(QuantExt CXX)

enable_testing()

include("../cmake/writeAll.cmake")
include("../cmake/writeTestSuiteMain.cmake")
include("../cmake/commonSettings.cmake")

get_library_name("QuantExt" QLE_LIB_NAME)
set_ql_library_name()

if (ORE_BUILD_TESTS)
    SET(COMPONENTS_CONDITIONAL "unit_test_framework")
else()
    SET(COMPONENTS_CONDITIONAL "")
endif()
find_package (Boost REQUIRED COMPONENTS ${COMPONENTS_CONDITIONAL} date_time thread serialization system timer filesystem OPTIONAL_COMPONENTS chrono)
if (ORE_ENABLE_PARALLEL_UNIT_TEST_RUNNER AND UNIX AND NOT APPLE)
    find_library(RT_LIBRARY rt REQUIRED)
endif()

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${QUANTLIB_SOURCE_DIR})
include_directories(${ORETEST_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_link_directory_if_exists("${QUANTLIB_SOURCE_DIR}/build/ql")
add_link_directory_if_exists("${CMAKE_BINARY_DIR}/QuantLib/ql")

add_subdirectory("qle")
if (ORE_BUILD_DOC)
    add_subdirectory("doc")
endif()
if (ORE_BUILD_TESTS)
    add_subdirectory("test")
endif()
