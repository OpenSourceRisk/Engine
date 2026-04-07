cmake_minimum_required(VERSION 3.15)
project(ORE CXX)
include(CTest)
include("cmake/commonSettings.cmake")

if(ORE_BUILD_QL_SEPARATELY)
    # Use installed QuantLib CMake package if we are not building it.
    find_package(QuantLib REQUIRED CONFIG)
else()
    add_subdirectory("QuantLib")
    install(TARGETS ql_library EXPORT ${ORE_PACKAGE_NAME}Targets)
endif()

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

# Installation
install(
  EXPORT ${ORE_PACKAGE_NAME}Targets
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${ORE_PACKAGE_NAME}"
  NAMESPACE ${ORE_PACKAGE_NAME}::
)

# Helpers for creating CMake config file to support `find_package(Ore CONFIG ...)`
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/cmake/${ORE_PACKAGE_NAME}ConfigVersion.cmake"
    VERSION ${ORE_PACKAGE_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${ORE_PACKAGE_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/cmake/${ORE_PACKAGE_NAME}Config.cmake"
    @ONLY
)

install(
    FILES
        "${PROJECT_BINARY_DIR}/cmake/${ORE_PACKAGE_NAME}Config.cmake"
        "${PROJECT_BINARY_DIR}/cmake/${ORE_PACKAGE_NAME}ConfigVersion.cmake"
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/${ORE_PACKAGE_NAME}"
)
