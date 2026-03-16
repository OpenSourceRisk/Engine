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
target_link_libraries(orea-test-suite
    PRIVATE
        ${OREA_LIB_NAME}
        ${ORE_TEST_SUPPORT_NAME}
)

if (ORE_ENABLE_PARALLEL_UNIT_TEST_RUNNER)
    target_compile_definitions(orea-test-suite PRIVATE ORE_ENABLE_PARALLEL_UNIT_TEST_RUNNER)
    if(UNIX AND NOT APPLE)
        add_rt_library(orea-test-suite)
    endif()
endif()

add_test(NAME orea-test-suite WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} COMMAND orea-test-suite -- --base_data_path=.)

install(TARGETS orea-test-suite
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    OPTIONAL
)
