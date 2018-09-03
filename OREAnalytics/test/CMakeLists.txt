# cpp files, this list is maintained manually

set(OREAnalytics-Test_SRC aggregationscenariodata.cpp
cube.cpp
main.cpp
observationmode.cpp
scenariogenerator.cpp
scenariosimmarket.cpp
sensitivityaggregator.cpp
sensitivityanalysis.cpp
sensitivityanalysisanalytic.cpp
sensitivityperformance.cpp
shiftscenariogenerator.cpp
stresstest.cpp
swapperformance.cpp
testmarket.cpp
testportfolio.cpp
testsuite.cpp)

writeTestSuiteMain("main.cpp")

add_executable(orea-test-suite ${OREAnalytics-Test_SRC})
target_link_libraries(orea-test-suite OREAnalytics OREData QuantExt QuantLib ${Boost_LIBRARIES})
