cmake_minimum_required(VERSION 3.8)

project(ORE CXX)

enable_testing()

include("cmake/commonSettings.cmake")

add_subdirectory("QuantLib")
add_subdirectory("QuantExt")
add_subdirectory("OREData")
add_subdirectory("OREAnalytics")
add_subdirectory("App")
