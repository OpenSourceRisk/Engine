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

using namespace ore::data;
using namespace boost::filesystem;

namespace ore {
namespace analytics {

std::map<RiskFactorKey, std::string> getScenarioDescriptions(QuantLib::ext::shared_ptr<ScenarioGenerator> scenGen) {
    std::map<RiskFactorKey, std::string> descriptions;
    auto sensiScenGen = QuantLib::ext::dynamic_pointer_cast<SensitivityScenarioGenerator>(scenGen);
    if (sensiScenGen) {
        for (const auto& desc : sensiScenGen->scenarioDescriptions()) {
            descriptions[desc.key1()] = desc.indexDesc1();
        }
    }
    return descriptions;
}


void ParConversionAnalyticImpl::setUpConfigurations() {
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
    analytic()->configurations().simMarketParams = inputs_->parConversionSimMarketParams();
    analytic()->configurations().sensiScenarioData = inputs_->parConversionScenarioData();
    analytic()->configurations().engineData = inputs_->parConversionPricingEngine();
}

void ParConversionAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                            const std::set<std::string>& runTypes) {
    if (!analytic()->match(runTypes))
        return;

    LOG("ParConversionAnalytic::runAnalytic called");

    analytic()->buildMarket(loader, false);

    auto parConversionAnalytic = static_cast<ParConversionAnalytic*>(analytic());

    QL_REQUIRE(parConversionAnalytic, "ParConversionAnalyticImpl internal error, can not convert analytic() to ParConversionAnalytic");

    auto zeroSensis = parConversionAnalytic->loadZeroSensitivities();

    if (!zeroSensis.empty()) {
        set<RiskFactorKey::KeyType> typesDisabled{RiskFactorKey::KeyType::OptionletVolatility};

        auto parAnalysis = QuantLib::ext::make_shared<ParSensitivityAnalysis>(
            inputs_->asof(), analytic()->configurations().simMarketParams,
            *analytic()->configurations().sensiScenarioData, Market::defaultConfiguration, true, typesDisabled);

        if (inputs_->parConversionAlignPillars()) {
            LOG("Sensi analysis - align pillars (for the par conversion or because alignPillars is enabled)");
            parAnalysis->alignPillars();
        } else {
            LOG("Sensi analysis - skip aligning pillars");
        }

        auto& configs = analytic()->configurations();

        auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
            analytic()->market(), configs.simMarketParams, inputs_->marketConfig("pricing"),
            configs.curveConfig ? *configs.curveConfig : ore::data::CurveConfigurations(),
            configs.todaysMarketParams ? *configs.todaysMarketParams : ore::data::TodaysMarketParameters(), true,
            configs.sensiScenarioData->useSpreadedTermStructures(), false, false, *inputs_->iborFallbackConfig());

        auto scenarioGenerator = QuantLib::ext::make_shared<SensitivityScenarioGenerator>(
            configs.sensiScenarioData, simMarket->baseScenario(), configs.simMarketParams, simMarket,
            QuantLib::ext::make_shared<DeltaScenarioFactory>(simMarket->baseScenario()), true, std::string(), true,
            simMarket->baseScenarioAbsolute());

        simMarket->scenarioGenerator() = scenarioGenerator;

        parAnalysis->computeParInstrumentSensitivities(simMarket);

        QuantLib::ext::shared_ptr<ParSensitivityConverter> parConverter =
            QuantLib::ext::make_shared<ParSensitivityConverter>(parAnalysis->parSensitivities(), parAnalysis->shiftSizes());

        map<RiskFactorKey, Size> factorToIndex;

        auto shiftSizes = parAnalysis->shiftSizes();

        Size counter = 0;
        for (auto const& k : parConverter->rawKeys()) {
            factorToIndex[k] = counter++;
        }

        std::vector<SensitivityRecord> results;
        std::map<RiskFactorKey, std::string> descriptions = getScenarioDescriptions(simMarket->scenarioGenerator());

        for (const auto& [id, sensis] : zeroSensis) {
            boost::numeric::ublas::vector<Real> zeroDeltas(parConverter->rawKeys().size(), 0.0);
            std::vector<SensitivityRecord> excludedDeltas;
            bool valid = true;
            for (const auto& zero : sensis) {
                if (zero.currency != configs.simMarketParams->baseCcy()) {
                    valid = false;
                    ALOG("Currency in the sensitivity input and config aren't consistent. Skip trade " << id);
                    break;
                }
                auto [rf, desc] = deconstructFactor(zero.riskFactor);
                if (rf.keytype != RiskFactorKey::KeyType::None) {
                    auto it = factorToIndex.find(rf);
                    if (it == factorToIndex.end()) {
                        if (ParSensitivityAnalysis::isParType(rf.keytype) && typesDisabled.count(rf.keytype) != 1) {

                            StructuredAnalyticsErrorMessage("Par conversion", "",
                                                            "Par factor " + ore::data::to_string(rf) +
                                                                " not found in factorToIndex map")
                                .log();
                        } else {
                            SensitivityRecord sr;
                            sr.tradeId = id;
                            sr.isPar = true;
                            sr.key_1 = rf;
                            sr.desc_1 = desc;
                            sr.delta = zero.delta;
                            sr.baseNpv = zero.baseNpv;
                            sr.currency = zero.currency;
                            sr.shift_1 = zero.shiftSize;
                            sr.gamma = QuantLib::Null<QuantLib::Real>();
                            excludedDeltas.push_back(sr);
                        }
                    } else {
                        auto shiftSize = shiftSizes.find(rf);
                        if (shiftSize == shiftSizes.end() || !close_enough(shiftSize->second.first, zero.shiftSize)) {
                            valid = false;
                            ALOG("Shift sizes in the sensitivity input and config aren't consistent. Skip trade "
                                 << id);
                            break;
                        }

                        zeroDeltas[it->second] = zero.delta;
                    }
                }
            }
            if (!sensis.empty() && valid) {
                boost::numeric::ublas::vector<Real> parDeltas = parConverter->convertSensitivity(zeroDeltas);
                Size counter = 0;
                for (const auto& key : parConverter->parKeys()) {
                    if (!close(parDeltas[counter], 0.0)) {
                        SensitivityRecord sr;
                        sr.tradeId = id;
                        sr.isPar = true;
                        sr.key_1 = key;
                        sr.desc_1 = descriptions[key];
                        sr.delta = parDeltas[counter];
                        sr.baseNpv = sensis.begin()->baseNpv;
                        sr.currency = sensis.begin()->currency;
                        sr.shift_1 = shiftSizes[key].second;
                        sr.gamma = QuantLib::Null<QuantLib::Real>();
                        results.push_back(sr);
                    }
                    counter++;
                }
                results.insert(results.end(), excludedDeltas.begin(), excludedDeltas.end());
            }
        }

        auto ss = QuantLib::ext::make_shared<SensitivityInMemoryStream>(results.begin(), results.end());
        QuantLib::ext::shared_ptr<InMemoryReport> report = QuantLib::ext::make_shared<InMemoryReport>();
        ReportWriter(inputs_->reportNaString()).writeSensitivityReport(*report, ss, inputs_->parConversionThreshold());
        analytic()->reports()["PARCONVERSION"]["parConversionSensitivity"] = report;

        if (inputs_->parConversionOutputJacobi()) {
            QuantLib::ext::shared_ptr<InMemoryReport> jacobiReport = QuantLib::ext::make_shared<InMemoryReport>();
            writeParConversionMatrix(parAnalysis->parSensitivities(), *jacobiReport);
            analytic()->reports()["PARCONVERSION"]["parConversionJacobi"] = jacobiReport;

            QuantLib::ext::shared_ptr<InMemoryReport> jacobiInverseReport = QuantLib::ext::make_shared<InMemoryReport>();
            parConverter->writeConversionMatrix(*jacobiInverseReport);
            analytic()->reports()["PARCONVERSION"]["parConversionJacobi_inverse"] = jacobiInverseReport;
        }
    }
    LOG("Sensi Analysis - Completed");
    CONSOLE("OK");
}

} // namespace analytics
} // namespace ore
