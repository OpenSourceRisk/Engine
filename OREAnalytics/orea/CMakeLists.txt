# cpp files, this list is maintained manually

set(OREAnalytics_SRC aggregation/collateralaccount.cpp
aggregation/collatexposurehelper.cpp
aggregation/postprocess.cpp
app/oreapp.cpp
app/parameters.cpp
app/reportwriter.cpp
app/sensitivityrunner.cpp
cube/cubewriter.cpp
cube/sensitivitycube.cpp
engine/filteredsensitivitystream.cpp
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
simulation/dategrid.cpp
simulation/fixingmanager.cpp
simulation/simmarket.cpp)

# hpp files, this list is maintained manually

set(OREAnalytics_HDR auto_link.hpp
version.hpp
aggregation/collateralaccount.hpp
aggregation/collatexposurehelper.hpp
aggregation/postprocess.hpp
app/oreapp.hpp
app/parameters.hpp
app/reportwriter.hpp
app/sensitivityrunner.hpp
cube/cubewriter.hpp
cube/inmemorycube.hpp
cube/npvcube.hpp
cube/npvsensicube.hpp
cube/sensicube.hpp
cube/sensitivitycube.hpp
engine/filteredsensitivitystream.hpp
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
simulation/dategrid.hpp
simulation/fixingmanager.hpp
simulation/simmarket.hpp)

writeAll("orea" "orea.hpp" "auto_link.hpp" "${OREAnalytics_HDR}")
add_library(OREAnalytics ${OREAnalytics_SRC})
target_link_libraries(OREAnalytics OREData QuantExt QuantLib ${Boost_LIBRARIES})
