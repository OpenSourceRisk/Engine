cmake_minimum_required(VERSION 3.15)

project(QuantExt CXX)

enable_testing()

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

find_package (Eigen3 3.3 NO_MODULE)
if (TARGET Eigen3::Eigen)
    if(MSVC AND EXISTS ${Eigen3_INCLUDE_DIRS})
        # for conan configuration
        include_directories(${Eigen3_INCLUDE_DIRS})
        add_compile_definitions(ORE_USE_EIGEN)
    else()
        get_target_property(Eigen3_include_dir Eigen3::Eigen INTERFACE_INCLUDE_DIRECTORIES)
        if(EXISTS ${Eigen3_include_dir})
            include_directories(${Eigen3_include_dir})
            add_compile_definitions(ORE_USE_EIGEN)
        elseif(NOT EXISTS ${Eigen3_include_dir} AND DEFINED ENV{Eigen3_INCLUDE_DIR})
            # cases in which returned directory does not exist but an environment variable is set
            include_directories($ENV{Eigen3_INCLUDE_DIR})
            add_compile_definitions(ORE_USE_EIGEN)
        else()
            message([WARNING] " -- Could not find Eigen3 include directories. Building without Eigen3. Please set 'Eigen3_INCLUDE_DIR' in environment variables.")
        endif()
    endif()
endif()

if (ORE_PYTHON_INTEGRATION)
    find_package(Python COMPONENTS Interpreter Development)
    include_directories(${Python_INCLUDE_DIRS})
endif()

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${QUANTLIB_SOURCE_DIR})
include_directories(${ORETEST_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_link_directory_if_exists("${QUANTLIB_SOURCE_DIR}/build/ql")
add_link_directory_if_exists("${CMAKE_BINARY_DIR}/QuantLib/ql")

generate_git_hash(ore_qle)

add_subdirectory("qle")
if (ORE_BUILD_DOC)
    add_subdirectory("doc")
endif()
if (ORE_BUILD_TESTS)
    add_subdirectory("test")
endif()
