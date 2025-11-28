# cpp files, this list is maintained manually

set(OREAnalytics-Test_SRC aggregationscenariodata.cpp
amcbermudanswaption.cpp
cube.cpp
historicalscenariogenerator.cpp
nettedexpsoure.cpp
observationmode.cpp
parsensitivityanalysis.cpp
parsensitivityanalysismanual.cpp
saccr.cpp
sacva.cpp
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
target_link_libraries(orea-test-suite ${QL_LIB_NAME} ${QLE_LIB_NAME} ${ORED_LIB_NAME} ${OREA_LIB_NAME})
if(DEFINED RT_LIBRARY AND NOT "${RT_LIBRARY}" MATCHES ".*NOTFOUND$")
    target_link_libraries(orea-test-suite ${RT_LIBRARY})
endif()

add_test(NAME orea-test-suite WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} COMMAND orea-test-suite -- --base_data_path=.)

install(TARGETS orea-test-suite
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        OPTIONAL
        )
