cmake_minimum_required(VERSION 3.15)

project(OREData CXX)

enable_testing()

include("../cmake/writeAll.cmake")
include("../cmake/commonSettings.cmake")

get_library_name("OREData" ORED_LIB_NAME)
get_library_name("QuantExt" QLE_LIB_NAME)
set_ql_library_name()

if (ORE_BUILD_TESTS)
    SET(COMPONENTS_CONDITIONAL "unit_test_framework")
else()
    SET(COMPONENTS_CONDITIONAL "")
endif()
find_package (Boost REQUIRED COMPONENTS ${COMPONENTS_CONDITIONAL} regex system date_time serialization filesystem timer OPTIONAL_COMPONENTS chrono)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${QUANTLIB_SOURCE_DIR})
include_directories(${QUANTEXT_SOURCE_DIR})
include_directories(${ORETEST_SOURCE_DIR})
include_directories(${RAPIDXML_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_link_directory_if_exists("${QUANTLIB_SOURCE_DIR}/build/ql")
add_link_directory_if_exists("${QUANTEXT_SOURCE_DIR}/build/qle")
add_link_directory_if_exists("${CMAKE_BINARY_DIR}/QuantLib/ql")
add_link_directory_if_exists("${CMAKE_BINARY_DIR}/QuantExt/qle")

add_subdirectory("ored")
if (ORE_BUILD_DOC)
    add_subdirectory("doc")
endif()
if (ORE_BUILD_TESTS)
    add_subdirectory("test")
endif()
