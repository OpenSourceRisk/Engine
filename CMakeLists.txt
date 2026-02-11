cmake_minimum_required(VERSION 3.15)
project(ORE CXX)
include(CTest)

# Path for local cmake modules
if(NOT "${CMAKE_CURRENT_LIST_DIR}/cmake" IN_LIST CMAKE_MODULE_PATH)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
endif()

set(USE_GLOBAL_ORE_BUILD ON)

include(commonSettings)

# This should not be needed. We set USE_GLOBAL_ORE_BUILD to ON above and I can't see anywhere 
# it is set to OFF. However leave this here because if USE_GLOBAL_ORE_BUILD is OFF then 
# QuantLib library is just a name and not a target and hence we need these.
if(NOT USE_GLOBAL_ORE_BUILD)
    include_directories(${QUANTLIB_SOURCE_DIR})
    add_link_directory_if_exists("${QUANTLIB_SOURCE_DIR}/build/ql")
    add_link_directory_if_exists("${CMAKE_BINARY_DIR}/QuantLib/ql")
endif()

add_subdirectory("QuantLib")
add_subdirectory("QuantExt")
add_subdirectory("OREData")
add_subdirectory("OREAnalytics")
add_subdirectory("ORETest")
add_subdirectory("ThirdPartyLibs/rapidxml-1.13")
if (ORE_BUILD_APP)
    add_subdirectory("App")
endif()
if (ORE_BUILD_SWIG)
    add_subdirectory("ORE-SWIG")
endif()

# add examples testsuite
if (ORE_BUILD_EXAMPLES AND ORE_BUILD_TESTS)
add_test(NAME examples WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Examples COMMAND pytest run_examples_testsuite.py)
endif()
