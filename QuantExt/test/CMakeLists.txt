# cpp files, this list is maintained manually

set(QuantExt-Test_SRC analyticlgmswaptionengine.cpp
blackvariancecurve.cpp
bonds.cpp
cashflow.cpp
commodityforward.cpp
crossassetmodel.cpp
crossassetmodel2.cpp
crossassetmodelparametrizations.cpp
currency.cpp
deltagammavar.cpp
deposit.cpp
discountcurve.cpp
discountingcommodityforwardengine.cpp
discountratiomodifiedcurve.cpp
dynamicblackvoltermstructure.cpp
dynamicswaptionvolmatrix.cpp
fxvolsmile.cpp
index.cpp
interpolatedyoycapfloortermpricesurface.cpp
logquote.cpp
main.cpp
optionletstripper.cpp
payment.cpp
pricecurve.cpp
pricetermstructureadapter.cpp
qle_calendars.cpp
ratehelpers.cpp
stabilisedglls.cpp
staticallycorrectedyieldtermstructure.cpp
survivalprobabilitycurve.cpp
swaptionvolatilityconverter.cpp
swaptionvolconstantspread.cpp
testsuite.cpp)

writeTestSuiteMain("main.cpp")

add_executable(quantext-test-suite ${QuantExt-Test_SRC})
target_link_libraries(quantext-test-suite QuantLib QuantExt ${Boost_LIBRARIES})
