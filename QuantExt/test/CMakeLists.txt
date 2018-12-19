# cpp files, this list is maintained manually

set(QuantExt-Test_SRC blackvariancecurve.cpp
commodityforward.cpp
currency.cpp
deltagammavar.cpp
deposit.cpp
discountcurve.cpp
discountingcommodityforwardengine.cpp
index.cpp
logquote.cpp
payment.cpp
ratehelpers.cpp
survivalprobabilitycurve.cpp 
testsuite.cpp)

add_executable(quantext-test-suite ${QuantExt-Test_SRC})
target_link_libraries(quantext-test-suite ${QL_LIB_NAME})
target_link_libraries(quantext-test-suite ${QLE_LIB_NAME})
target_link_libraries(quantext-test-suite ${ORED_LIB_NAME})
target_link_libraries(quantext-test-suite ${Boost_LIBRARIES})

add_test(NAME quantext-test-suite COMMAND quantext-test-suite WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
