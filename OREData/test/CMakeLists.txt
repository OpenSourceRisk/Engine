# cpp files, this list is maintained manually

set(OREData-Test_SRC adjustmentfactors.cpp
basecorrelationcurve.cpp
bond.cpp
calendaradjustment.cpp
calendars.cpp
cbo.cpp
ccyswapwithresets.cpp
cds.cpp
cdsindexoption.cpp
cms.cpp
commodityapo.cpp
commodityasianoption.cpp
commoditycurve.cpp
commoditycurveconfig.cpp
commodityoption.cpp
commodityswaption.cpp
commodityvolcurve.cpp
commodityvolcurveconfig.cpp
compositetrade.cpp
compositewrapper.cpp
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
formulaparser.cpp
fxaccumulator.cpp
fxasianoption.cpp
fxdom.cpp
fxexotics.cpp
fxoption.cpp
fxswap.cpp
fxtarf.cpp
fxtriangulation.cpp
fxvolcurve.cpp
gaussiancam.cpp
generalisedreplicatingvarianceswapengine.cpp
indices.cpp
inflationcapfloor.cpp
inflationcurve.cpp
legdata.cpp
localvol.cpp
mxnircurves.cpp
optionpaymentdata.cpp
ored_commodityforward.cpp
oredtestmarket.cpp
parser.cpp
portfolio.cpp
representativefxoption.cpp
representativeswaption.cpp
riskparticipationagreement.cpp
schedule.cpp
scriptengine.cpp
scriptparser.cpp
strike.cpp
swaption.cpp
testsuite.cpp
todaysmarket.cpp
value.cpp
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

install(TARGETS ored-test-suite
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        OPTIONAL
        )
