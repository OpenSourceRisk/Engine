# cpp files, this list is maintained manually

set(OREData-Test_SRC bond.cpp
calendars.cpp
ccyswapwithresets.cpp
cds.cpp
cms.cpp
commoditycurve.cpp
commoditycurveconfig.cpp
commodityoption.cpp
commodityvolcurve.cpp
commodityvolcurveconfig.cpp
conventions.cpp
cpiswap.cpp
crossassetmodeldata.cpp
curveconfig.cpp
equitymarketdata.cpp
equityswap.cpp
equitytrades.cpp
fxoption.cpp
fxswap.cpp
fxtriangulation.cpp
indices.cpp
inflationcapfloor.cpp
legdata.cpp
ored_commodityforward.cpp
parser.cpp
portfolio.cpp
schedule.cpp
swaption.cpp
testsuite.cpp
todaysmarket.cpp
xmlmanipulation.cpp
yieldcurve.cpp)

add_executable(ored-test-suite ${OREData-Test_SRC})
target_link_libraries(ored-test-suite ${ORED_LIB_NAME})
target_link_libraries(ored-test-suite ${QLE_LIB_NAME})
target_link_libraries(ored-test-suite ${QL_LIB_NAME})
target_link_libraries(ored-test-suite ${Boost_LIBRARIES})

add_test(NAME ored-test-suite COMMAND ored-test-suite WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
