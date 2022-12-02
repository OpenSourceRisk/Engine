# cpp files, this list is maintained manually

set(QuantExt-Test_SRC analyticcashsettledeuropeanengine.cpp
analyticlgmswaptionengine.cpp
basecorrelationcurve.cpp
bfrrvolsurface.cpp
blackvariancecurve.cpp
blackvariancesurfacesparse.cpp
blackvariancesurfacestddevs.cpp
blackvolsurfacedelta.cpp
blackvolsurfaceproxy.cpp
bonds.cpp
calendars.cpp
capfloortermvolcurve.cpp
cashflow.cpp
commodityforward.cpp
commodityschwartzmodel.cpp
commodityspreadoption.cpp
correlationtermstructure.cpp
cpicapfloor.cpp
crcirpp.cpp
crossassetmodel.cpp
crossassetmodel2.cpp
crossassetmodelparametrizations.cpp
crossccybasismtmresetswap.cpp
crossccybasismtmresetswaphelper.cpp
crossccyfixfloatswap.cpp
crossccyfixfloatswaphelper.cpp
currency.cpp
dategeneration.cpp
deltagammavar.cpp
deposit.cpp
discountcurve.cpp
discountingcommodityforwardengine.cpp
discountratiomodifiedcurve.cpp
durationadjustedcmscoupon.cpp
dynamicblackvoltermstructure.cpp
dynamicswaptionvolmatrix.cpp
equityforwardcurvestripper.cpp
exactbachelierimpliedvolatility.cpp
fillemptymatrix.cpp
forwardbond.cpp
fxvolsmile.cpp
index.cpp
inflationcurve.cpp
inflationvol.cpp
interpolatedyoycapfloortermpricesurface.cpp
logquote.cpp
mclgmswaptionengine.cpp
multilegoption.cpp
normalfreeboundarysabr.cpp
optionletstripper.cpp
payment.cpp
piecewiseatmoptionletcurve.cpp
piecewiseoptionletcurve.cpp
piecewiseoptionletstripper.cpp
pricecurve.cpp
pricetermstructureadapter.cpp
qle_calendars.cpp
quadraticinterpolation.cpp
randomvariable.cpp
ratehelpers.cpp
stabilisedglls.cpp
staticallycorrectedyieldtermstructure.cpp
strippedoptionletadapter.cpp
strippedoptionletadapter2.cpp
survivalprobabilitycurve.cpp
swaptionvolatilityconverter.cpp
swaptionvolconstantspread.cpp
testsuite.cpp)

add_executable(quantext-test-suite ${QuantExt-Test_SRC})
target_link_libraries(quantext-test-suite ${QL_LIB_NAME})
target_link_libraries(quantext-test-suite ${QLE_LIB_NAME})
target_link_libraries(quantext-test-suite ${Boost_LIBRARIES} ${RT_LIBRARY})

add_test(NAME quantext-test-suite COMMAND quantext-test-suite WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

install(TARGETS quantext-test-suite
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
