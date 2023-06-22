/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <orea/app/analytics/parconversionanalytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/engine/sensitivityinmemorystream.hpp>
#include <orea/scenario/deltascenariofactory.hpp>
#include <orea/scenario/scenario.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <ored/utilities/to_string.hpp>
#include <orea/scenario/scenariosimmarketplus.hpp>

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

void ParConversionAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->parConversionSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->parConversionScenarioData();
    analytic()->configurations().engineData = inputs_->parConversionPricingEngine();
}

void ParConversionAnalyticImpl::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                            const std::set<std::string>& runTypes) {
    if (!analytic()->match(runTypes))
        return;

    LOG("ParConversionAnalytic::runAnalytic called");

    analytic()->buildMarket(loader, false);

    auto parConversionAnalytic = static_cast<ParConversionAnalytic*>(analytic());

    auto zeroSensis = parConversionAnalytic->loadZeroSensitivities();

    set<RiskFactorKey::KeyType> typesDisabled{RiskFactorKey::KeyType::OptionletVolatility};
    auto parAnalysis = boost::make_shared<ParSensitivityAnalysis>(
        inputs_->asof(), analytic()->configurations().simMarketParams, *analytic()->configurations().sensiScenarioData,
        "", true, typesDisabled);
    if (inputs_->parConversionAlignPillars()) {
        LOG("Sensi analysis - align pillars (for the par conversion or because alignPillars is enabled)");
        parAnalysis->alignPillars();
    } else {
        LOG("Sensi analysis - skip aligning pillars");
    }
    auto curveConfigs = analytic()->configurations().curveConfig;

    auto simMarket = boost::make_shared<ScenarioSimMarketPlus>(
        analytic()->market(), analytic()->configurations().simMarketParams, inputs_->marketConfig("pricing"),
        curveConfigs ? *curveConfigs : ore::data::CurveConfigurations(),
        analytic()->configurations().todaysMarketParams ? *analytic()->configurations().todaysMarketParams
                                                        : TodaysMarketParameters(),
        true, analytic()->configurations().sensiScenarioData->useSpreadedTermStructures(), false, false,
        *inputs_->iborFallbackConfig());

    auto scenarioFactory = boost::make_shared<DeltaScenarioFactory>(simMarket->baseScenario());
    auto scenarioGenerator = boost::make_shared<SensitivityScenarioGenerator>(
        analytic()->configurations().sensiScenarioData, simMarket->baseScenario(),
        analytic()->configurations().simMarketParams, simMarket, scenarioFactory, true, true,
        simMarket->baseScenarioAbsolute());

    // Set simulation market's scenario generator
    simMarket->scenarioGenerator() = scenarioGenerator;

    parAnalysis->computeParInstrumentSensitivities(simMarket);
    boost::shared_ptr<ParSensitivityConverter> parConverter =
        boost::make_shared<ParSensitivityConverter>(parAnalysis->parSensitivities(), parAnalysis->shiftSizes());

    map<RiskFactorKey, Size> factorToIndex;

    Size counter = 0;
    for (auto const& k : parConverter->rawKeys()) {
        factorToIndex[k] = counter++;
    }

    std::map<std::string, std::set<SensitivityRecord>> records;

    std::map<RiskFactorKey, std::string> descriptions;

    for (const auto& desc : scenarioGenerator->scenarioDescriptions()) {
        descriptions[desc.key1()] = desc.indexDesc1();
    }

    for (const auto& [id, sensis] : zeroSensis) {
        boost::numeric::ublas::vector<Real> zeroDeltas(parConverter->rawKeys().size(), 0.0);
        // TODO par conversion
        bool isPar = true;
        for (const auto& zero : sensis) {
            auto [rf, desc] = deconstructFactor(zero.riskFactor);
            descriptions[rf] = desc;
            if (rf.keytype != RiskFactorKey::KeyType::None) {
                auto it = factorToIndex.find(rf);
                if (it == factorToIndex.end()){
                    if (ParSensitivityAnalysis::isParType(rf.keytype) && typesDisabled.count(rf.keytype) != 1) {

                        ALOG(StructuredAnalyticsErrorMessage("Par conversion", "",
                                                             "Par factor " + ore::data::to_string(rf) +
                                                                 " not found in factorToIndex map"));
                    } else {
                        // Add deltas which don't need to be converted
                        SensitivityRecord sr;
                        sr.tradeId = id;
                        sr.isPar = isPar;
                        sr.key_1 = rf;
                        sr.desc_1 = desc;
                        sr.delta = zero.delta;
                        sr.baseNpv = sensis.begin()->baseNpv;
                        sr.currency = sensis.begin()->currency;
                        // TODO Shift size
                        sr.shift_1 = sensis.begin()->shiftSize;
                        sr.gamma = QuantLib::Null<QuantLib::Real>();
                        records[id].insert(sr);
                    }
                } else {
                    zeroDeltas[it->second] = zero.delta;
                }
            }
        }
        boost::numeric::ublas::vector<Real> parDeltas = parConverter->convertSensitivity(zeroDeltas);
        Size counter = 0;
        for (const auto& key : parConverter->parKeys()) {
            if (!close(parDeltas[counter], 0.0)) {
                SensitivityRecord sr;
                sr.tradeId = id;
                sr.isPar = isPar;
                sr.key_1 = key;
                sr.desc_1 = descriptions[key];
                sr.delta = parDeltas[counter];
                sr.baseNpv = sensis.begin()->baseNpv;
                sr.currency = sensis.begin()->currency;
                sr.shift_1 = sensis.begin()->shiftSize;
                sr.gamma = QuantLib::Null<QuantLib::Real>();
                records[id].insert(sr);
            }
            counter++;
        }
    }

    std::vector<SensitivityRecord> results;
    for (const auto& [id, recs] : records) {
        results.insert(results.end(), recs.begin(), recs.end());
    }
    auto ss = boost::make_shared<SensitivityInMemoryStream>(results.begin(), results.end());
    boost::shared_ptr<InMemoryReport> report = boost::make_shared<InMemoryReport>();
    ReportWriter(inputs_->reportNaString()).writeSensitivityReport(*report, ss, inputs_->sensiThreshold());
    analytic()->reports()["PARCONVERSION"]["parConversionSensitivity"] = report;
    LOG("Sensi Analysis - Completed");
    CONSOLE("OK");
}

} // namespace analytics
} // namespace ore
