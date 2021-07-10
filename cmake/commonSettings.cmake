include(CheckCXXCompilerFlag)

# add compiler flag, if not already present
macro(add_compiler_flag flag supportsFlag)
  check_cxx_compiler_flag(${flag} ${supportsFlag})
    if(${supportsFlag} AND NOT "${CMAKE_CXX_FLAGS}" MATCHES "${flag}")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
    endif()
endmacro()

# use CXX 11 and std::unique_ptr in QuantLib (instead of std::auto_ptr, which is deprecated in C++11)
set(CMAKE_CXX_STANDARD 11)
add_compiler_flag("-D QL_USE_STD_UNIQUE_PTR" supports_D_QL_USE_STD_UNIQUE_PTR)

# On single-configuration builds, select a default build type that gives the same compilation flags as a default autotools build.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

if(MSVC)
    # build static libs always
    set(BUILD_STATIC_LIBS ON)

    # link against static boost libraries
    set(Boost_USE_STATIC_LIBS ON)

    add_compiler_flag("-D_SCL_SECURE_NO_DEPRECATE" supports_D_SCL_SECURE_NO_DEPRECATE)
    add_compiler_flag("-D_CRT_SECURE_NO_DEPRECATE" supports_D_CRT_SECURE_NO_DEPRECATE)
    add_compiler_flag("-DBOOST_ENABLE_ASSERT_HANDLER" enableAssertionHandler)
    add_compiler_flag("/bigobj" supports_bigobj)
    add_compiler_flag("/W3" supports_w3)
else()
    # build shared libs always
    set(BUILD_SHARED_LIBS ON)

    # link against dynamic boost libraries
    add_definitions(-DBOOST_ALL_DYN_LINK)
    add_definitions(-DBOOST_TEST_DYN_LINK)

    # avoid a crash in valgrind that sometimes occurs if this flag is not defined
    add_definitions(-DBOOST_MATH_NO_LONG_DOUBLE_MATH_FUNCTIONS)

    # switch off blas debug for build types release
    if(NOT("${CMAKE_BUILD_TYPE}" STREQUAL "Debug"))
      add_definitions(-DBOOST_UBLAS_NDEBUG)
    endif()

    # customize compiler warnings
    add_compiler_flag("-Wall" supportsWall)
    add_compiler_flag("-Wnon-virtual-dtor" supportsNonVirtualDtor)
    add_compiler_flag("-Wsign-compare" supportsSignCompare)
    add_compiler_flag("-Wsometimes-uninitialized" supportsSometimesUninitialized)
    add_compiler_flag("-Wmaybe-uninitialized" supportsMaybeUninitialized)
    add_compiler_flag("-Wno-unknown-pragmas" supportsNoUnknownPragmas)
    add_compiler_flag("-DBOOST_ENABLE_ASSERT_HANDLER" enableAssertionHandler)
endif()

# set library locations
get_filename_component(QUANTLIB_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../QuantLib" ABSOLUTE)
get_filename_component(QUANTEXT_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../QuantExt" ABSOLUTE)
get_filename_component(OREDATA_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../OREData" ABSOLUTE)
get_filename_component(OREANALYTICS_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../OREAnalytics" ABSOLUTE)
get_filename_component(ORETEST_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../ORETest" ABSOLUTE)
get_filename_component(RAPIDXML_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../ThirdPartyLibs/rapidxml-1.13" ABSOLUTE)

# convenience function that adds a link directory dir, but only if it exists
function(add_link_directory_if_exists dir)
  if(EXISTS "${dir}")
    link_directories("${dir}")
  endif()
endfunction()

macro(get_library_name LIB_NAME OUTPUT_NAME)

    # modified version of quantlib.cmake / get_quantlib_library_name

    # message(STATUS "${LIB_NAME} Library name tokens:")

    # MSVC: Give built library different names following code in 'ql/autolink.hpp'
    if(MSVC)

        # - platform
        if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
            set(LIB_PLATFORM "-x64")
        endif()
        #message(STATUS " - Platform: ${LIB_PLATFORM}")

        # - thread linkage
        set(LIB_THREAD_OPT "-mt")  # _MT is defined for /MD and /MT runtimes (https://docs.microsoft.com/es-es/cpp/build/reference/md-mt-ld-use-run-time-library)
        #message(STATUS " - Thread opt: ${LIB_THREAD_OPT}")

        # - static/dynamic linkage
        if("${MSVC_RUNTIME}" STREQUAL "static")
            set(LIB_RT_OPT "-s")
            set(CMAKE_DEBUG_POSTFIX "gd")
        else()
            set(CMAKE_DEBUG_POSTFIX "-gd")
        endif()
        #message(STATUS " - Linkage opt: ${LIB_RT_OPT}")

        set(${OUTPUT_NAME} "${LIB_NAME}${LIB_PLATFORM}${LIB_THREAD_OPT}${LIB_RT_OPT}")
    else()
        set(${OUTPUT_NAME} ${LIB_NAME})
    endif()
    message(STATUS "${LIB_NAME} library name: ${${OUTPUT_NAME}}[${CMAKE_DEBUG_POSTFIX}]")
endmacro(get_library_name)

macro(configure_msvc_runtime)
    # Credit: https://stackoverflow.com/questions/10113017/setting-the-msvc-runtime-in-cmake
    if(MSVC)
        # Default to dynamically-linked runtime.
        if("${MSVC_RUNTIME}" STREQUAL "")
            set(MSVC_RUNTIME "dynamic")
        endif()

        # Set compiler options.
        set(variables
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_MINSIZEREL
            CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_MINSIZEREL
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_RELWITHDEBINFO
            )

        if(${MSVC_RUNTIME} STREQUAL "static")
            message(STATUS "MSVC -> forcing use of statically-linked runtime.")
            foreach(variable ${variables})
                if(${variable} MATCHES "/MD")
                    string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
                endif()
            endforeach()
        else()
            message(STATUS "MSVC -> forcing use of dynamically-linked runtime." )
            foreach(variable ${variables})
                if(${variable} MATCHES "/MT")
                    string(REGEX REPLACE "/MT" "/MD" ${variable} "${${variable}}")
                endif()
            endforeach()
        endif()

      if(${MSVC_RUNTIME} STREQUAL "static")
        if(USE_BOOST_DYNAMIC_LIBRARIES)
            message(FATAL_ERROR "Use of shared Boost libraries while compiling with static runtime seems not be a good idea.")
        endif()
        set(Boost_USE_STATIC_RUNTIME ON)
      endif()

    endif()

endmacro()
