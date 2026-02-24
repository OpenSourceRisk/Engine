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
target_link_libraries(ored-test-suite
    PRIVATE
        ${ORED_LIB_NAME}
        ${ORE_TEST_SUPPORT_NAME}
)

if (ORE_ENABLE_PARALLEL_UNIT_TEST_RUNNER)
    target_compile_definitions(ored-test-suite PRIVATE ORE_ENABLE_PARALLEL_UNIT_TEST_RUNNER)
    if(UNIX AND NOT APPLE)
        add_rt_library(ored-test-suite)
    endif()
endif()

# under windows our unit test code switches to .. since it assumes the test executable is run from /bin
# we work around that by explicitly specifying the base_data_path to be the current directory
add_test(NAME ored-test-suite WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} COMMAND ored-test-suite -- --base_data_path=.)

install(TARGETS ored-test-suite
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    OPTIONAL
)
