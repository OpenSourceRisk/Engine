file(GLOB_RECURSE QuantExt_SRC "*.cpp")
file(GLOB_RECURSE QuantExt_HDR RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.hpp")

# build the all.hpp
writeAll("qle" "quantext.hpp" "auto_link.hpp" "${QuantExt_HDR}")

add_library(QuantExt ${QuantExt_SRC})

target_link_libraries(QuantExt QuantLib ${Boost_LIBRARIES})
