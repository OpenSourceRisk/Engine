cmake_minimum_required(VERSION 3.8)

project(OREAnalyticsJava CXX)

# write the library here
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# workaround for ninja which requires the file to exist before the build starts
#execute_process(
#  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
#  COMMAND ${CMAKE_COMMAND} -E make_directory lib
#)

execute_process(
  WORKING_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
  COMMAND ${CMAKE_COMMAND} -E touch OREAnalyticsJava.jar
)

include(${ORE}/cmake/commonSettings.cmake)

# Append /bigobj to compiler flags on Windows for this target
if (MSVC)
   # FIXME: Add the other compiler flags from QuantLib-SWIG/Python/setup.py (/GR /FD /Zm250 /EHsc)?
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
   message(STATUS CMAKE_CXX_FLAGS "=" "${CMAKE_CXX_FLAGS}")

   foreach (cfg "DEBUG" "RELEASE" "MINSIZEREL" "RELWITHDEBINFO")
      foreach (flag "CXX" "C")
         set (CMAKE_${flag}_FLAGS_${cfg} "${CMAKE_${flag}_FLAGS_${cfg}} /bigobj") 
	 message(STATUS CMAKE_${flag}_FLAGS_${cfg} "=" "${CMAKE_${flag}_FLAGS_${cfg}}")
      endforeach ()
   endforeach ()
endif()

set(ORE_JAVA_LIB_NAME "OREAnalyticsJava") # no extensions
get_library_name("OREAnalytics" OREA_LIB_NAME)
get_library_name("OREData" ORED_LIB_NAME)
get_library_name("QuantExt" QLE_LIB_NAME)
get_library_name("QuantLib" QL_LIB_NAME)
configure_msvc_runtime()

include_directories(${ORE}/QuantLib)
include_directories(${ORE}/QuantExt)
include_directories(${ORE}/OREData)
include_directories(${ORE}/OREAnalytics)
include_directories(${PROJECT_SOURCE_DIR}/../SWIG)
include_directories(${PROJECT_SOURCE_DIR}/../../QuantLib-SWIG/SWIG)
include_directories(${PROJECT_SOURCE_DIR}/../../QuantExt-SWIG/SWIG)
include_directories(${PROJECT_SOURCE_DIR}/../../OREData-SWIG/SWIG)
include_directories(${PROJECT_SOURCE_DIR}/../../OREAnalytics-SWIG/SWIG)

find_package (Boost COMPONENTS serialization date_time filesystem system REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# specify library search path (update this when we build ORE with cmake)
add_link_directory_if_exists(${ORE}/build/QuantLib/ql)
add_link_directory_if_exists(${ORE}/build/QuantExt/qle)
add_link_directory_if_exists(${ORE}/build/OREData/ored)
add_link_directory_if_exists(${ORE}/build/OREAnalytics/orea)

# use C++11
set (CMAKE_CXX_STANDARD 11)

# run SWIG
find_package(SWIG REQUIRED)
find_package(Java REQUIRED)
find_package(JNI REQUIRED)
include(UseJava)
include(UseSWIG)
include_directories(${JNI_INCLUDE_DIRS})
include(${SWIG_USE_FILE})
set(CMAKE_SWIG_FLAGS "")

set_source_files_properties(${PROJECT_SOURCE_DIR}/../SWIG/oreanalytics.i PROPERTIES CPLUSPLUS ON)
set_source_files_properties(${PROJECT_SOURCE_DIR}/../SWIG/oreanalytics.i PROPERTIES SWIG_FLAGS "-includeall")
set(CMAKE_SWIG_FLAGS -package oreanalytics)
set(CMAKE_SWIG_OUTDIR "oreanalytics")

swig_add_library(${ORE_JAVA_LIB_NAME} TYPE SHARED LANGUAGE java SOURCES ${PROJECT_SOURCE_DIR}/../SWIG/oreanalytics.i)

swig_link_libraries(${ORE_JAVA_LIB_NAME} ${QL_LIB_NAME})
swig_link_libraries(${ORE_JAVA_LIB_NAME} ${QLE_LIB_NAME})
swig_link_libraries(${ORE_JAVA_LIB_NAME} ${ORED_LIB_NAME})
swig_link_libraries(${ORE_JAVA_LIB_NAME} ${OREA_LIB_NAME})
swig_link_libraries(${ORE_JAVA_LIB_NAME} ${Boost_LIBRARIES})

#add_dependencies(${ORE_JAVA_LIB_NAME} ${QLE_LIB_NAME})
#add_dependencies(${ORE_JAVA_LIB_NAME} ${ORED_LIB_NAME})
#add_dependencies(${ORE_JAVA_LIB_NAME} ${OREA_LIB_NAME})

add_custom_command(TARGET ${ORE_JAVA_LIB_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory classes
    COMMAND "${Java_JAVAC_EXECUTABLE}" -d classes oreanalytics/*.java
    COMMAND "${Java_JAR_EXECUTABLE}" -cfM ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/OREAnalyticsJava.jar -C classes .
)

add_jar(ORERunner
        SOURCES Examples/ORERunner.java
        INCLUDE_JARS ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/OREAnalyticsJava.jar
        ENTRY_POINT orerunner/ORERunner
        MANIFEST Manifest.txt
    	OUTPUT_DIR ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})

add_dependencies(ORERunner ${ORE_JAVA_LIB_NAME})

