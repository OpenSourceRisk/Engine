/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef orea_engine_i
#define orea_engine_i

%include stl.i
%include types.i
%include orea_scenario_ext.i
%include orea_cube.i
%include ored_market.i
%include ored_portfolio.i
%include ored_curveconfigurations.i

%{
using ore::analytics::SensitivityAnalysis;
using ore::analytics::ParSensitivityAnalysis;
using ore::analytics::ValuationEngine;
using ore::analytics::ParametricVarCalculator;
using ore::analytics::VarCalculator;
using ore::analytics::runStressTest;
%}

%shared_ptr(VarCalculator)
%nodefaultctor VarCalculator;
class VarCalculator {
public:
    virtual ~VarCalculator() {}
    virtual QuantLib::Real var(QuantLib::Real confidence, const bool isCall = true,
                               const std::set<std::pair<std::string, QuantLib::Size>>& tradeIds = {}) const = 0;
};

%shared_ptr(SensitivityAnalysis)
class SensitivityAnalysis {
public:
    SensitivityAnalysis(
        const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
        const QuantLib::ext::shared_ptr<ore::data::Market>& market, const std::string& marketConfiguration,
        const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
        const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
        const QuantLib::ext::shared_ptr<SensitivityScenarioData>& sensitivityData, const bool recalibrateModels,
        const bool laxFxConversion = false,
        const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs = nullptr,
        const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams = nullptr,
        const bool nonShiftedBaseCurrencyConversion = false,
        const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
        const QuantLib::ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig =
            QuantLib::ext::make_shared<IborFallbackConfig>(IborFallbackConfig::defaultConfig()),
        const bool continueOnError = false, const bool dryRun = false, const bool useAtParCouponsTrades = true);

    void generateSensitivities();
    const QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket() const;
};

%shared_ptr(ParSensitivityAnalysis)
class ParSensitivityAnalysis {
public:
    ParSensitivityAnalysis(const QuantLib::Date& asof,
                           const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
                           const ore::analytics::SensitivityScenarioData& sensitivityData,
                           const std::string& marketConfiguration = Market::defaultConfiguration,
                           const bool continueOnError = false,
                           const std::set<ore::analytics::RiskFactorKey::KeyType>& typesDisabled = {});
    void computeParInstrumentSensitivities(const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket);
};

%shared_ptr(ValuationEngine)
class ValuationEngine {
public:
    %extend {
        ValuationEngine(const QuantLib::Date& today,
                        const QuantLib::ext::shared_ptr<ore::data::DateGrid>& dg,
                        const QuantLib::ext::shared_ptr<ore::analytics::SimMarket>& simMarket,
                        const bool recalibrate = true) {
            return new ValuationEngine(today, dg, simMarket,
                                       std::set<std::pair<std::string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>>(),
                                       recalibrate);
        }
    }
};

%shared_ptr(ParametricVarCalculator)
%nodefaultctor ParametricVarCalculator;
class ParametricVarCalculator : public VarCalculator {
public:
    struct ParametricVarParams {
        enum class Method {
            Delta,
            DeltaGammaNormal,
            MonteCarlo,
            CornishFisher,
            Saddlepoint,
        };

        ParametricVarParams();
        ParametricVarParams(const std::string& m, QuantLib::Size samples, QuantLib::Size seed);

        Method method;
        QuantLib::Size samples;
        QuantLib::Size seed;
    };

    QuantLib::Real var(QuantLib::Real confidence, const bool isCall = true,
                       const std::set<std::pair<std::string, QuantLib::Size>>& tradeIds = {}) const override;
};

void runStressTest(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                   const QuantLib::ext::shared_ptr<ore::data::Market>& market, const std::string& marketConfiguration,
                   const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
                   const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                   const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressData,
                   const QuantLib::ext::shared_ptr<ore::data::Report>& report,
                   const QuantLib::ext::shared_ptr<ore::data::Report>& cfReport = nullptr, const double threshold = 0.0,
                   const Size precision = 2, const bool includePastCashflows = false,
                   const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
                   const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
                   const QuantLib::ext::shared_ptr<ScenarioFactory>& scenarioFactory = nullptr,
                   const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                   const QuantLib::ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig =
                       QuantLib::ext::make_shared<IborFallbackConfig>(IborFallbackConfig::defaultConfig()),
                   bool continueOnError = false,
                   const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& scenarioReport = nullptr,
                   const bool useAtParCouponsTrades = true);

#endif
