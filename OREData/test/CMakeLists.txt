# cpp files, this list is maintained manually

set(OREData-Test_SRC basecorrelationcurve.cpp
bond.cpp
calendaradjustment.cpp
calendars.cpp
ccyswapwithresets.cpp
cds.cpp
cms.cpp
commodityasianoption.cpp
commoditycurve.cpp
commoditycurveconfig.cpp
commodityoption.cpp
commodityvolcurve.cpp
commodityvolcurveconfig.cpp
conventions.cpp
conventionsbasedfutureexpiry.cpp
correlationcurveconfig.cpp
cpicapfloor.cpp
cpiswap.cpp
creditdefaultswapdata.cpp
crossassetmodeldata.cpp
curveconfig.cpp
curvespecparser.cpp
digitalcms.cpp
equityasianoption.cpp
equitymarketdata.cpp
equityswap.cpp
equitytrades.cpp
expiry.cpp
fittedbondcurve.cpp
fixings.cpp
fxasianoption.cpp
fxoption.cpp
fxswap.cpp
fxtriangulation.cpp
fxvolcurve.cpp
indices.cpp
inflationcapfloor.cpp
inflationcurve.cpp
legdata.cpp
mxnircurves.cpp
optionasiandata.cpp
optionpaymentdata.cpp
ored_commodityforward.cpp
parser.cpp
portfolio.cpp
schedule.cpp
strike.cpp
swaption.cpp
testsuite.cpp
todaysmarket.cpp
xmlmanipulation.cpp
yieldcurve.cpp
zerocouponswap.cpp)

add_executable(ored-test-suite ${OREData-Test_SRC})
target_link_libraries(ored-test-suite ${ORED_LIB_NAME})
target_link_libraries(ored-test-suite ${QLE_LIB_NAME})
target_link_libraries(ored-test-suite ${QL_LIB_NAME})
target_link_libraries(ored-test-suite ${Boost_LIBRARIES})

# under windows our unit test code switches to .. since it assumes the test executable is run from /bin
# we work around that by explicitly specifying the base_data_path to be the current directory
add_test(NAME ored-test-suite WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} COMMAND ored-test-suite -- --base_data_path=.)
