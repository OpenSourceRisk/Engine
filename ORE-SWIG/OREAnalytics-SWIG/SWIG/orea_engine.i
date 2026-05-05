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

%shared_ptr(ore::analytics::VarCalculator)
%nodefaultctor ore::analytics::VarCalculator;
%shared_ptr(ore::analytics::SensitivityAnalysis)
%shared_ptr(ore::analytics::ParSensitivityAnalysis)
%shared_ptr(ore::analytics::ValuationEngine)
%shared_ptr(ore::analytics::ParametricVarCalculator)
%nodefaultctor ore::analytics::ParametricVarCalculator;

namespace ore {
namespace analytics {
class VarCalculator {
public:
    virtual ~VarCalculator() {}
    virtual QuantLib::Real var(QuantLib::Real confidence, const bool isCall = true,
                               const std::set<std::pair<std::string, QuantLib::Size>>& tradeIds = {}) const = 0;
};

class SensitivityAnalysis {
public:
    SensitivityAnalysis(
        const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
        const QuantLib::ext::shared_ptr<ore::data::Market>& market, const std::string& marketConfiguration,
        const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
        const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
        const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensitivityData,
        const bool recalibrateModels,
        const bool laxFxConversion = false,
        const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs = nullptr,
        const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams = nullptr,
        const bool nonShiftedBaseCurrencyConversion = false,
        const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
        const QuantLib::ext::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig =
            QuantLib::ext::make_shared<ore::data::IborFallbackConfig>(ore::data::IborFallbackConfig::defaultConfig()),
        const bool continueOnError = false, const bool dryRun = false, const bool useAtParCouponsTrades = true);

    void generateSensitivities();
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket> simMarket() const;
};

class ParSensitivityAnalysis {
public:
    ParSensitivityAnalysis(const QuantLib::Date& asof,
                           const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
                           const ore::analytics::SensitivityScenarioData& sensitivityData,
                           const std::string& marketConfiguration = ore::data::Market::defaultConfiguration,
                           const bool continueOnError = false,
                           const std::set<ore::analytics::RiskFactorKey::KeyType>& typesDisabled = {});
    void computeParInstrumentSensitivities(const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket);
};

class ValuationEngine {
public:
    %extend {
        ValuationEngine(const QuantLib::Date& today,
                        const QuantLib::ext::shared_ptr<ore::data::DateGrid>& dg,
                        const QuantLib::ext::shared_ptr<ore::analytics::SimMarket>& simMarket,
                        const bool recalibrate = true) {
            return new ore::analytics::ValuationEngine(
                today, dg, simMarket,
                std::set<std::pair<std::string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>>(), recalibrate);
        }
    }
};

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
                   const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
                   const QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData>& stressData,
                   const QuantLib::ext::shared_ptr<ore::data::Report>& report,
                   const QuantLib::ext::shared_ptr<ore::data::Report>& cfReport = nullptr, const double threshold = 0.0,
                   const Size precision = 2, const bool includePastCashflows = false,
                   const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
                   const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
                   const QuantLib::ext::shared_ptr<ore::analytics::ScenarioFactory>& scenarioFactory = nullptr,
                   const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
                   const QuantLib::ext::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig =
                       QuantLib::ext::make_shared<ore::data::IborFallbackConfig>(ore::data::IborFallbackConfig::defaultConfig()),
                   bool continueOnError = false,
                   const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& scenarioReport = nullptr,
                   const bool useAtParCouponsTrades = true);

} // namespace analytics
} // namespace ore

#endif
