# cpp files, this list is maintained manually

set(OREAnalytics-Test_SRC aggregationscenariodata.cpp
amcbermudanswaption.cpp
cube.cpp
historicalscenariogenerator.cpp
nettedexpsoure.cpp
observationmode.cpp
parsensitivityanalysis.cpp
parsensitivityanalysismanual.cpp
scenario.cpp
scenariogenerator.cpp
scenarioshiftcalculator.cpp
scenariosimmarket.cpp
sensitivityaggregator.cpp
sensitivityanalysis.cpp
sensitivityanalysisanalytic.cpp
sensitivityperformance.cpp
sensitivityperformanceplus.cpp
sensitivityvsanalytic.cpp
shiftscenariogenerator.cpp
simulationmeasures.cpp
stresstest.cpp
swapperformance.cpp
testmarket.cpp
testportfolio.cpp
testsuite.cpp)

add_executable(orea-test-suite ${OREAnalytics-Test_SRC})
target_link_libraries(orea-test-suite ${QL_LIB_NAME})
target_link_libraries(orea-test-suite ${QLE_LIB_NAME})
target_link_libraries(orea-test-suite ${ORED_LIB_NAME})
target_link_libraries(orea-test-suite ${OREA_LIB_NAME})
target_link_libraries(orea-test-suite ${Boost_LIBRARIES} ${RT_LIBRARY})

add_test(NAME orea-test-suite WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} COMMAND orea-test-suite -- --base_data_path=.)

install(TARGETS orea-test-suite
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        OPTIONAL
        )
