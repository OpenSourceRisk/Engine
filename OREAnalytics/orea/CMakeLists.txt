# cpp files, this list is maintained manually

set(OREAnalytics_SRC aggregation/collateralaccount.cpp
aggregation/collatexposurehelper.cpp
aggregation/cvaspreadsensitivitycalculator.cpp
aggregation/dimcalculator.cpp
aggregation/dimregressioncalculator.cpp
aggregation/dynamiccreditxvacalculator.cpp
aggregation/exposureallocator.cpp
aggregation/exposurecalculator.cpp
aggregation/nettedexposurecalculator.cpp
aggregation/postprocess.cpp
aggregation/staticcreditxvacalculator.cpp
aggregation/xvacalculator.cpp
app/oreapp.cpp
app/parameters.cpp
app/reportwriter.cpp
app/sensitivityrunner.cpp
app/xvarunner.cpp
cube/cubeinterpretation.cpp
cube/cubewriter.cpp
cube/sensitivitycube.cpp
engine/amcvaluationengine.cpp
engine/bufferedsensitivitystream.cpp
engine/cptycalculator.cpp
engine/filteredsensitivitystream.cpp
engine/mporcalculator.cpp
engine/npvrecord.cpp
engine/parametricvar.cpp
engine/riskfilter.cpp
engine/sensitivityaggregator.cpp
engine/sensitivityanalysis.cpp
engine/sensitivitycubestream.cpp
engine/sensitivityfilestream.cpp
engine/sensitivityinmemorystream.cpp
engine/sensitivityrecord.cpp
engine/stresstest.cpp
engine/valuationcalculator.cpp
engine/valuationengine.cpp
scenario/clonedscenariogenerator.cpp
scenario/clonescenariofactory.cpp
scenario/crossassetmodelscenariogenerator.cpp
scenario/lgmscenariogenerator.cpp
scenario/scenario.cpp
scenario/scenariogeneratorbuilder.cpp
scenario/scenariogeneratordata.cpp
scenario/scenariosimmarket.cpp
scenario/scenariosimmarketparameters.cpp
scenario/scenariowriter.cpp
scenario/sensitivityscenariodata.cpp
scenario/sensitivityscenariogenerator.cpp
scenario/shiftscenariogenerator.cpp
scenario/simplescenario.cpp
scenario/stressscenariodata.cpp
scenario/stressscenariogenerator.cpp
simulation/fixingmanager.cpp
simulation/simmarket.cpp)

# hpp files, this list is maintained manually

set(OREAnalytics_HDR aggregation/collateralaccount.hpp
aggregation/collatexposurehelper.hpp
aggregation/cvaspreadsensitivitycalculator.hpp
aggregation/dimcalculator.hpp
aggregation/dimregressioncalculator.hpp
aggregation/dynamiccreditxvacalculator.hpp
aggregation/exposureallocator.hpp
aggregation/exposurecalculator.hpp
aggregation/nettedexposurecalculator.hpp
aggregation/postprocess.hpp
aggregation/staticcreditxvacalculator.hpp
aggregation/xvacalculator.hpp
app/oreapp.hpp
app/parameters.hpp
app/reportwriter.hpp
app/sensitivityrunner.hpp
app/structuredanalyticserror.hpp
app/structuredanalyticswarning.hpp
app/xvarunner.hpp
auto_link.hpp
cube/cubeinterpretation.hpp
cube/cubewriter.hpp
cube/inmemorycube.hpp
cube/npvcube.hpp
cube/npvsensicube.hpp
cube/sensicube.hpp
cube/sensitivitycube.hpp
engine/amcvaluationengine.hpp
engine/bufferedsensitivitystream.hpp
engine/cptycalculator.hpp
engine/filteredsensitivitystream.hpp
engine/mporcalculator.hpp
engine/npvrecord.hpp
engine/observationmode.hpp
engine/parametricvar.hpp
engine/riskfilter.hpp
engine/sensitivityaggregator.hpp
engine/sensitivityanalysis.hpp
engine/sensitivitycubestream.hpp
engine/sensitivityfilestream.hpp
engine/sensitivityinmemorystream.hpp
engine/sensitivityrecord.hpp
engine/sensitivitystream.hpp
engine/stresstest.hpp
engine/valuationcalculator.hpp
engine/valuationengine.hpp
scenario/aggregationscenariodata.hpp
scenario/clonedscenariogenerator.hpp
scenario/clonescenariofactory.hpp
scenario/crossassetmodelscenariogenerator.hpp
scenario/lgmscenariogenerator.hpp
scenario/scenario.hpp
scenario/scenariofactory.hpp
scenario/scenariogenerator.hpp
scenario/scenariogeneratorbuilder.hpp
scenario/scenariogeneratordata.hpp
scenario/scenariosimmarket.hpp
scenario/scenariosimmarketparameters.hpp
scenario/scenariowriter.hpp
scenario/sensitivityscenariodata.hpp
scenario/sensitivityscenariogenerator.hpp
scenario/shiftscenariogenerator.hpp
scenario/simplescenario.hpp
scenario/simplescenariofactory.hpp
scenario/stressscenariodata.hpp
scenario/stressscenariogenerator.hpp
simulation/fixingmanager.hpp
simulation/simmarket.hpp
version.hpp)

writeAll("orea" "orea.hpp" "auto_link.hpp" "${OREAnalytics_HDR}")
add_library(${OREA_LIB_NAME} ${OREAnalytics_SRC})
target_link_libraries(${OREA_LIB_NAME} ${QL_LIB_NAME})
target_link_libraries(${OREA_LIB_NAME} ${QLE_LIB_NAME})
target_link_libraries(${OREA_LIB_NAME} ${ORED_LIB_NAME})
target_link_libraries(${OREA_LIB_NAME} ${Boost_LIBRARIES})

install(DIRECTORY . DESTINATION include/orea
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS ${OREA_LIB_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
