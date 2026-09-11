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
%include <std_pair.i>
%include orea_scenario_ext.i
%include orea_cube.i
%include orea_sensitivity.i
%include orea_riskfilter.i
%include ored_market.i
%include ored_portfolio.i
%include ored_curveconfigurations.i
%include ored_utilities.i
%include linearalgebra.i

%{
#include <orea/engine/historicalsensipnlcalculator.hpp>
%}

%shared_ptr(ore::analytics::VarCalculator)
%nodefaultctor ore::analytics::VarCalculator;
%shared_ptr(ore::analytics::SensitivityAnalysis)
%shared_ptr(ore::analytics::ParSensitivityAnalysis)
%shared_ptr(ore::analytics::ValuationEngine)
%shared_ptr(ore::analytics::ParametricVarCalculator)
%nodefaultctor ore::analytics::ParametricVarCalculator;
%shared_ptr(ore::analytics::PNLCalculator)
%shared_ptr(ore::analytics::CovarianceCalculator)
%shared_ptr(ore::analytics::HistoricalSensiPnlCalculator)

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
        const ext::shared_ptr<ore::data::Portfolio>& portfolio,
        const ext::shared_ptr<ore::data::Market>& market, const std::string& marketConfiguration,
        const ext::shared_ptr<ore::data::EngineData>& engineData,
        const ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
        const ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensitivityData,
        const bool recalibrateModels,
        const bool laxFxConversion = false,
        const ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs = nullptr,
        const ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams = nullptr,
        const bool nonShiftedBaseCurrencyConversion = false,
        const ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
        const ext::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig =
            QuantLib::ext::make_shared<ore::data::IborFallbackConfig>(ore::data::IborFallbackConfig::defaultConfig()),
        const bool continueOnError = false, const bool dryRun = false, const bool useAtParCouponsTrades = true);

    void generateSensitivities();
    const ext::shared_ptr<ore::analytics::ScenarioSimMarket> simMarket() const;
    std::vector<ext::shared_ptr<ore::analytics::SensitivityCube>> sensiCubes() const;
    ext::shared_ptr<ore::analytics::SensitivityCube> sensiCube() const;
};

class ParSensitivityAnalysis {
public:
    ParSensitivityAnalysis(const QuantLib::Date& asof,
                           const ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
                           const ore::analytics::SensitivityScenarioData& sensitivityData,
                           const std::string& marketConfiguration = ore::data::Market::defaultConfiguration,
                           const bool continueOnError = false,
                           const std::set<ore::analytics::RiskFactorKey::KeyType>& typesDisabled = {});
    void computeParInstrumentSensitivities(const ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket);
};

class ValuationEngine {
public:
    %extend {
        ValuationEngine(const QuantLib::Date& today,
                        const ext::shared_ptr<ore::data::DateGrid>& dg,
                        const ext::shared_ptr<ore::analytics::SimMarket>& simMarket,
                        const bool recalibrate = true) {
            return new ore::analytics::ValuationEngine(
                today, dg, simMarket,
                std::set<std::pair<std::string, ext::shared_ptr<QuantExt::ModelBuilder>>>(), recalibrate);
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

void runStressTest(const ext::shared_ptr<ore::data::Portfolio>& portfolio,
                   const ext::shared_ptr<ore::data::Market>& market, const std::string& marketConfiguration,
                   const ext::shared_ptr<ore::data::EngineData>& engineData,
                   const ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
                   const ext::shared_ptr<ore::analytics::StressTestScenarioData>& stressData,
                   const ext::shared_ptr<ore::data::Report>& report,
				   const ext::shared_ptr<ore::data::Loader>& loader = nullptr,
                   const ext::shared_ptr<ore::data::Report>& cfReport = nullptr, const double threshold = 0.0,
                   const Size precision = 2, const bool includePastCashflows = false,
                   const ore::data::CurveConfigurations& curveConfigs = ore::data::CurveConfigurations(),
                   const ore::data::TodaysMarketParameters& todaysMarketParams = ore::data::TodaysMarketParameters(),
                   const ext::shared_ptr<ore::analytics::ScenarioFactory>& scenarioFactory = nullptr,
                   const ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
                   const ext::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig =
                       QuantLib::ext::make_shared<ore::data::IborFallbackConfig>(ore::data::IborFallbackConfig::defaultConfig()),
                   bool continueOnError = false,
                   const ext::shared_ptr<ore::data::InMemoryReport>& scenarioReport = nullptr,
                   const bool useAtParCouponsTrades = true, const Size nThreads = 1);

} // namespace analytics
} // namespace ore

// --- PNLCalculator / CovarianceCalculator / HistoricalSensiPnlCalculator ---

%template(RiskFactorKeySizePair) std::pair<QuantExt::RiskFactorKey, QuantLib::Size>;
%template(RiskFactorKeySizePairSet) std::set<std::pair<QuantExt::RiskFactorKey, QuantLib::Size>>;

// Convenience overload: constructing pairs whose first member is a shared_ptr-wrapped
// type (RiskFactorKey) directly via std::set::insert(pair) is not supported by SWIG's
// stock std_pair/std_set typemaps. Provide a 2-arg insert() that builds the pair in C++.
%extend std::set<std::pair<QuantExt::RiskFactorKey, QuantLib::Size>> {
    void insert(const QuantExt::RiskFactorKey& key, QuantLib::Size idx) {
        self->insert(std::make_pair(key, idx));
    }
}

%template(SensitivityRecordSet) std::set<ore::analytics::SensitivityRecord>;

// std::vector<std::vector<QuantLib::Real>> is already wrapped as DoubleVectorVector in QuantLib-SWIG/vectors.i

namespace ore { namespace analytics {

class PNLCalculator {
public:
    PNLCalculator(ore::data::TimePeriod pnlPeriod, bool runRiskFactorLevel = false);
    virtual ~PNLCalculator();

    void populatePNLs(const std::vector<QuantLib::Real>& allPnls, const std::vector<QuantLib::Real>& foPnls,
                      const std::vector<QuantLib::Date>& startDates, const std::vector<QuantLib::Date>& endDates);

    void populateTradePNLs(const std::vector<std::vector<QuantLib::Real>>& allPnls,
                           const std::vector<std::vector<QuantLib::Real>>& foPnls);

    const std::vector<QuantLib::Real>& pnls();
    const std::vector<QuantLib::Real>& foPnls();

    const std::vector<std::vector<QuantLib::Real>>& tradePnls();
    const std::vector<std::vector<QuantLib::Real>>& foTradePnls();

    void clear();
};

}}

%template(PNLCalculatorVector) std::vector<ext::shared_ptr<ore::analytics::PNLCalculator>>;

namespace ore { namespace analytics {

class CovarianceCalculator {
public:
    CovarianceCalculator(ore::data::TimePeriod covariancePeriod);
    void initialise(const std::set<std::pair<QuantExt::RiskFactorKey, QuantLib::Size>>& keys);
    void populateCovariance(const std::set<std::pair<QuantExt::RiskFactorKey, QuantLib::Size>>& keys);
    const QuantLib::Matrix& covariance() const;
    const QuantLib::Matrix& correlation() const;
};

class HistoricalSensiPnlCalculator {
public:
    HistoricalSensiPnlCalculator(const ext::shared_ptr<ore::analytics::HistoricalScenarioGenerator>& hisScenGen,
                                 const ext::shared_ptr<ore::analytics::SensitivityStream>& ss);

    void populateSensiShifts(ext::shared_ptr<ore::analytics::NPVCube>& cube,
                             const std::vector<QuantExt::RiskFactorKey>& keys,
                             ext::shared_ptr<ore::analytics::ScenarioShiftCalculator> shiftCalculator,
                             const bool& supressError = false);

    void calculateSensiPnl(const std::set<ore::analytics::SensitivityRecord>& srs,
        const std::vector<QuantExt::RiskFactorKey>& rfKeys,
        ext::shared_ptr<ore::analytics::NPVCube>& shiftCube,
        const std::vector<ext::shared_ptr<ore::analytics::PNLCalculator>>& pnlCalculators,
        const ext::shared_ptr<ore::analytics::CovarianceCalculator>& covarianceCalculator,
        const std::vector<std::string>& tradeIds = {},
        const bool includeGammaMargin = true, const bool includeDeltaMargin = true,
        const bool tradeLevel = false,
        const bool runRiskFactorLevel = false);

    int getScenarioNumber() const;
};

}}

#endif
