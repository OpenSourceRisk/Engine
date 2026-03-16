include_guard(GLOBAL)

# Set the policy around find_package Boost: https://cmake.org/cmake/help/latest/policy/CMP0167.html
if(DEFINED ENV{ORE_BOOST_DIR} OR DEFINED ENV{BOOST_DIR})
    if(POLICY CMP0167)
        cmake_policy(SET CMP0167 NEW)
        set(CMAKE_POLICY_DEFAULT_CMP0167 NEW)
    endif()
    # Don't need to explicity set Boost_DIR if BOOST_DIR is defined as it will pick it up.
    if(DEFINED ENV{ORE_BOOST_DIR})
        set(Boost_DIR "$ENV{ORE_BOOST_DIR}" CACHE STRING "Initialized from environment variable ORE_BOOST_DIR")
    endif()
elseif(DEFINED ENV{BOOST} OR DEFINED ENV{BOOST_LIB64})
    if(POLICY CMP0167)
        cmake_policy(SET CMP0167 OLD)
        set(CMAKE_POLICY_DEFAULT_CMP0167 OLD)
    endif()
    set(BOOST_INCLUDEDIR "$ENV{BOOST}" CACHE STRING "Initialized from environment variable BOOST")
    set(BOOST_LIBRARYDIR "$ENV{BOOST_LIB64}" CACHE STRING "Initialized from environment variable BOOST_LIB64")
else()
    message(DEBUG "Neither ORE_BOOST_DIR nor BOOST_DIR nor BOOST/BOOST_LIB64 environment variables are set. 
        Boost will be searched for in default locations and policy is not CMP0167 set.")
endif()

include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)

# Information to help with install.
set(ORE_VERSION_MAJOR 1)
set(ORE_VERSION_MINOR 8)
set(ORE_VERSION_PATCH 15)
set(ORE_VERSION_TWEAK 0)
set(ORE_PACKAGE_VERSION ${ORE_VERSION_MAJOR}.${ORE_VERSION_MINOR}.${ORE_VERSION_PATCH}.${ORE_VERSION_TWEAK})
set(ORE_PACKAGE_NAME "Ore")

# Used to define installation directories.
include(GNUInstallDirs)

include(${CMAKE_CURRENT_LIST_DIR}/writeAll.cmake)

option(ORE_BUILD_DOC "Build documentation" ON)
option(ORE_BUILD_EXAMPLES "Build examples" ON)
option(ORE_BUILD_TESTS "Build test suite" ON)
option(ORE_BUILD_APP "Build app" ON)
option(ORE_BUILD_SWIG "Build ORE Python" ON)
option(MSVC_LINK_DYNAMIC_RUNTIME "Link against dynamic runtime" ON)
option(MSVC_PARALLELBUILD "Use flag /MP" ON)
option(QL_USE_PCH "Use precompiled headers" OFF)
option(ORE_PYTHON_INTEGRATION "Build ORE with Python Integration" OFF)
option(ORE_USE_ZLIB "Use compression for boost::iostreams" OFF)
option(ORE_MULTITHREADING_CPU_AFFINITY "Set cpu affinitity in multithreaded calculations" OFF)
option(ORE_ENABLE_PARALLEL_UNIT_TEST_RUNNER "Enable the parallel unit test runner" OFF)
option(ORE_ENABLE_OPENCL "Enable OpenCL" OFF)
option(ORE_ENABLE_CUDA "Enable CUDA" OFF)
option(ORE_PREVENT_BOOST_AUTO_LINKING "Prevent Boost auto-linking" ON)

# Implies that we have built QuantLib (our fork thereof) separately and that we are importing it.
option(ORE_BUILD_QL_SEPARATELY "Enable when building ORE separately." OFF)

# This should resolve to a target on mono-repo builds and on the CI build.
set(QL_LIB_NAME "QuantLib::QuantLib")

# Set ORE library target names.
set(QLE_LIB_NAME "QuantExt")
set(ORED_LIB_NAME "OREData")
set(OREA_LIB_NAME "OREAnalytics")
set(RAPIDXML_NAME "RapidXml")
set(ORE_TEST_SUPPORT_NAME "TestSupport")

# define build type clang address sanitizer + undefined behaviour + LIBCPP assertions, but keep O2
set(CMAKE_CXX_FLAGS_CLANG_ASAN_O2 "-fsanitize=address,undefined -fno-omit-frame-pointer -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG -g -O2")

# add compiler flag, if not already present
macro(add_compiler_flag flag supportsFlag)
    check_cxx_compiler_flag(${flag} ${supportsFlag})
    if(${supportsFlag} AND NOT "${CMAKE_CXX_FLAGS}" MATCHES "${flag}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
    endif()
endmacro()

# add linker flag for shared libs and exe, if not already present
macro(add_linker_flag flag supportsFlag)
    check_linker_flag(CXX ${flag} ${supportsFlag})
    if(${supportsFlag} AND NOT "${CMAKE_SHARED_LINKER_FLAGS}" MATCHES "${flag}")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${flag}")
    endif()
    if(${supportsFlag} AND NOT "${CMAKE_EXE_LINKER_FLAGS}" MATCHES "${flag}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${flag}")
    endif()
endmacro()

# use CXX 20, disable gnu extensions
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS FALSE)

# If available, use PIC for shared libs and PIE for executables
if (NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
if (CMAKE_POSITION_INDEPENDENT_CODE)
    # cmake policy CMP0083: add PIE support if possible (need cmake 3.14)
    include(CheckPIESupported)
    check_pie_supported()
endif()

if(ORE_USE_ZLIB)
  find_package(ZLIB REQUIRED)
endif()

# On single-configuration builds, select a default build type that gives the same compilation flags as a default autotools build.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

if(MSVC)
    set(BUILD_SHARED_LIBS OFF)

    # build static libs always
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<BOOL:${MSVC_LINK_DYNAMIC_RUNTIME}>:DLL>"
    )

    # For info on MSVC compile options /we<NUM> see:
    # https://learn.microsoft.com/en-us/cpp/error-messages/compiler-errors-1/c-cpp-build-errors?view=msvc-180
    add_compile_options(
        /external:env:BOOST
        /external:W0
        /bigobj # This should really be scoped to targets that need it.
        /W3
        /we5038 /we4189 /we4700 /we5233 /we4508 /wd4834 /we26815
        "$<$<CONFIG:Release>:/GF;/Gy;/Ot;/GT>"
        "$<$<CONFIG:RelWithDebInfo>:/GF;/Gy;/Ot;/GT;/Oi>"
        $<$<BOOL:${MSVC_PARALLELBUILD}>:/MP>
    )

    add_compile_definitions(
        _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
        _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
        _SCL_SECURE_NO_DEPRECATE
        _CRT_SECURE_NO_DEPRECATE
        BOOST_ENABLE_ASSERT_HANDLER
    )

    add_link_options(
        /LARGEADDRESSAWARE
    )

else()
    if (NOT DEFINED BUILD_SHARED_LIBS)
        set(BUILD_SHARED_LIBS ON)
    endif()

    add_compile_definitions(
        # avoid a crash in valgrind that sometimes occurs if this flag is not defined
        BOOST_MATH_NO_LONG_DOUBLE_MATH_FUNCTIONS
        $<$<NOT:$<CONFIG:Debug>>:BOOST_UBLAS_NDEBUG>
        BOOST_ENABLE_ASSERT_HANDLER
    )

    # The add_compiler_flag and add_linker_flag call is generally unnecessary for these.
    # If there are unusual options or specific issues, we can add back the call to either one.
    # CMake supported compiler IDs are documented here:
    # https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_ID.html
    set(clangs_to_check "AppleClang,Clang")
    add_link_options(
        -pthread
        $<$<AND:$<BOOL:${APPLE}>,$<CXX_COMPILER_ID:${clangs_to_check}>>:-flat_namespace>
    )

    add_compile_options(
        -pthread
        -Wall
        -Werror=non-virtual-dtor
        -Werror=reorder
        -Werror=unused-but-set-variable
        -Werror=return-type
        -Werror=unused-function
        -Wno-unknown-pragmas
        $<$<CXX_COMPILER_ID:${clangs_to_check}>:--system-header-prefix=boost/>
        -Werror=unused-variable
        -Werror=uninitialized
        "$<$<AND:$<BOOL:${QL_USE_PCH}>,$<CXX_COMPILER_ID:${clangs_to_check}>>:SHELL:-Xclang -fno-pch-timestamp>"
        $<$<AND:$<BOOL:${QL_USE_PCH}>,$<CXX_COMPILER_ID:GNU>>:-fpch-preprocess>
        $<$<CXX_COMPILER_ID:${clangs_to_check}>:-Wsometimes-uninitialized>
        $<$<CXX_COMPILER_ID:GNU>:-Wmaybe-uninitialized>
        $<$<CXX_COMPILER_ID:GNU>:-Wno-error=maybe-uninitialized>
        $<$<CXX_COMPILER_ID:${clangs_to_check}>:-Wunused-lambda-capture>
        $<$<CXX_COMPILER_ID:${clangs_to_check}>:-Winconsistent-missing-override>
    )
endif()

# link against static boost libraries
if(NOT DEFINED Boost_USE_STATIC_LIBS)
    if(BUILD_SHARED_LIBS)
        set(Boost_USE_STATIC_LIBS OFF)
    else()
        set(Boost_USE_STATIC_LIBS ON)
    endif()
endif()

# Boost static runtime. ON for MSVC
if(NOT DEFINED Boost_USE_STATIC_RUNTIME)
    if(BUILD_SHARED_LIBS OR (MSVC AND MSVC_LINK_DYNAMIC_RUNTIME))
        set(Boost_USE_STATIC_RUNTIME OFF)
    else()
        set(Boost_USE_STATIC_RUNTIME ON)
    endif()
endif()

if(NOT Boost_USE_STATIC_LIBS)
    # link against dynamic boost libraries
    add_definitions(-DBOOST_ALL_DYN_LINK)
    add_definitions(-DBOOST_TEST_DYN_LINK)
endif()

if(ORE_BOOST_AUTO_LINK_SYSTEM)
    add_compile_definitions(BOOST_AUTO_LINK_SYSTEM)
endif()

# Avoid using Boost auto-linking unless it was explicitly asked for.
if(ORE_PREVENT_BOOST_AUTO_LINKING)
    add_compile_definitions(BOOST_ALL_NO_LIB)
endif()

set(Boost_NO_WARN_NEW_VERSIONS ON)

# Find Boost components.
list(APPEND BOOST_COMPONENT_LIST filesystem serialization timer log iostreams thread)
find_package(Boost REQUIRED COMPONENTS ${BOOST_COMPONENT_LIST})

if (MSVC)
    if(Boost_VERSION_STRING LESS 1.84.0)
        add_compile_definitions(_WINVER=0x0601)
        add_compile_definitions(_WIN32_WINNT=0x0601)
        add_compile_definitions(BOOST_USE_WINAPI_VERSION=0x0601)
    endif()
endif()

# workaround when building with boost 1.81, see https://github.com/boostorg/phoenix/issues/111
add_definitions(-DBOOST_PHOENIX_STL_TUPLE_H_)

# parallel unit test runner
if (ORE_ENABLE_PARALLEL_UNIT_TEST_RUNNER)
    if(UNIX AND NOT APPLE)
        find_library(RT_LIBRARY rt REQUIRED)
    endif()
endif()

# In QuantLib, this is guarded by QL_TAGGED_LAYOUT.
# We may want to later add ORE_TAGGED_LAYOUT to have this control also.
if (MSVC)
    if (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
        set(DEBUG_POSTFIX "-x64")
        set(RELEASE_POSTFIX "-x64")
    endif()
    set(DEBUG_POSTFIX ${DEBUG_POSTFIX}-mt)
    set(RELEASE_POSTFIX ${RELEASE_POSTFIX}-mt)
    if (MSVC_LINK_DYNAMIC_RUNTIME)
        set(DEBUG_POSTFIX ${DEBUG_POSTFIX}-gd)
    else()
        set(DEBUG_POSTFIX ${DEBUG_POSTFIX}-sgd)
        set(RELEASE_POSTFIX ${RELEASE_POSTFIX}-s)
    endif()
    set(CMAKE_DEBUG_POSTFIX ${DEBUG_POSTFIX})
    set(CMAKE_RELEASE_POSTFIX ${RELEASE_POSTFIX})
    set(CMAKE_RELWITHDEBINFO_POSTFIX ${RELEASE_POSTFIX})
    set(CMAKE_MINSIZEREL_POSTFIX ${RELEASE_POSTFIX})
endif()

if(ORE_BUILD_DOC)
    find_package(Doxygen)
    if(NOT Doxygen_FOUND)
        message("Doxygen needs to be installed to generate the doxygen documentation.")
    endif()
endif()

function(generate_doxy_docs doxy_filename)
    # Set the Doxygen input and output files.
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/${doxy_filename}.doxy)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target("doc_${doxy_filename}" ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation for ${doxy_filename} with Doxygen."
        VERBATIM
    )
endfunction()

# Add RT library in certain cases.
function(add_rt_library target_name)
    if(DEFINED RT_LIBRARY AND NOT "${RT_LIBRARY}" MATCHES ".*NOTFOUND$")
        target_link_libraries(${target_name} PRIVATE ${RT_LIBRARY})
    endif()
endfunction()

# Used in multiple places so add as function here.
function(msvc_wpo_options target_name)
    set(configs ${ARGN})
    if(MSVC AND MSVC_WHOLE_PROGRAM_OPTIMIZATION)
        foreach(cfg IN LISTS configs)
            target_compile_options(${target_name} PRIVATE
                "$<$<CONFIG:${cfg}>:/GL>"
            )
            target_link_options(${target_name} PRIVATE
                "$<$<CONFIG:${cfg}>:/INCREMENTAL:NO;/LTCG;/OPT:REF;/OPT:ICF>"
            )
        endforeach()
    endif()
endfunction()

# Remove compile options from a target if they are present.
function(remove_compile_options target_name)
    # If no options are given, do nothing.
    if(NOT ARGN)
        return()
    endif()

    # If the target has no compile flags, do nothing.
    get_target_property(current_flags ${target_name} COMPILE_OPTIONS)
    if(NOT current_flags)
        return()
    endif()

    # Attempt to remove each flag from the target's compile options, if it is present.
    set(sth_removed FALSE)
    foreach(flag IN LISTS ARGN)
        if("${current_flags}" MATCHES "${flag}")
            list(REMOVE_ITEM current_flags ${flag})
            set(sth_removed TRUE)
            message(DEBUG "Removed compile option ${flag} from target ${target_name}.")
        endif()
    endforeach()

    # If we removed something, update the target's compile options.
    if(sth_removed)
        set_target_properties(${target_name} PROPERTIES COMPILE_OPTIONS "${current_flags}")
    endif()
endfunction()
