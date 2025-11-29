cmake_minimum_required(VERSION 3.15)

project(OREAnalytics CXX)

enable_testing()

get_library_name("OREAnalytics" OREA_LIB_NAME)
get_library_name("OREData" ORED_LIB_NAME)
get_library_name("QuantExt" QLE_LIB_NAME)
set_ql_library_name()

include_directories(${QUANTLIB_SOURCE_DIR})
include_directories(${QUANTEXT_SOURCE_DIR})
include_directories(${OREDATA_SOURCE_DIR})
include_directories(${ORETEST_SOURCE_DIR})
include_directories(${RAPIDXML_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_link_directory_if_exists("${QUANTLIB_SOURCE_DIR}/build/ql")
add_link_directory_if_exists("${QUANTEXT_SOURCE_DIR}/build/qle")
add_link_directory_if_exists("${OREDATA_SOURCE_DIR}/build/ored")
add_link_directory_if_exists("${CMAKE_BINARY_DIR}/QuantLib/ql")
add_link_directory_if_exists("${CMAKE_BINARY_DIR}/QuantExt/qle")
add_link_directory_if_exists("${CMAKE_BINARY_DIR}/OREData/ored")

add_subdirectory("orea")
if (ORE_BUILD_DOC AND Doxygen_FOUND)
    add_subdirectory("doc")
endif()
if (ORE_BUILD_TESTS)
    add_subdirectory("test")
endif()
