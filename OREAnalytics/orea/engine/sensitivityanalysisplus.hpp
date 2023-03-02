/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file orea/engine/sensitivityanalysisplus.hpp
    \brief Perform sensitivity analysis for a given portfolio
    \ingroup simulation
*/

#pragma once

#include <orea/engine/sensitivityanalysis.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/portfolio/enginefactory.hpp>

namespace ore {
namespace analytics {
using namespace QuantLib;
using namespace ore::data;

//! Sensitivity Analysis
/*!
  This class wraps functionality to perform a sensitivity analysis for a given portfolio.
  It is derived from the ORE SensitivityAnalysis class, and simply overrides one virtual functionality
  It uses a more space-efficient scenario factory so as to mitigate memory bloat

  \ingroup simulation
*/

class SensitivityAnalysisPlus : public SensitivityAnalysis {
public:
    //! Constructor using single-threaded engine
    SensitivityAnalysisPlus(const boost::shared_ptr<ore::data::Portfolio>& portfolio,
                            const boost::shared_ptr<ore::data::Market>& market, const string& marketConfiguration,
                            const boost::shared_ptr<ore::data::EngineData>& engineData,
                            const boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
                            const boost::shared_ptr<ore::analytics::SensitivityScenarioData>& sensitivityData,
                            const bool recalibrateModels,
                            const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs = nullptr,
                            const boost::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams = nullptr,
                            const bool nonShiftedBaseCurrencyConversion = false,
                            const boost::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                            const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                            const bool continueOnError = false, bool analyticFxSensis = false, bool dryRun = false)
        : ore::analytics::SensitivityAnalysis(portfolio, market, marketConfiguration, engineData, simMarketData,
                                              sensitivityData, recalibrateModels, curveConfigs, todaysMarketParams,
                                              nonShiftedBaseCurrencyConversion, referenceData, iborFallbackConfig,
                                              continueOnError, analyticFxSensis, dryRun),
          useSingleThreadedEngine_(true) {}

    //! Constructor using multi-threaded engine
    SensitivityAnalysisPlus(const Size nThreads, const Date& asof, const boost::shared_ptr<ore::data::Loader>& loader,
                            const boost::shared_ptr<ore::data::Portfolio>& portfolio, const string& marketConfiguration,
                            const boost::shared_ptr<ore::data::EngineData>& engineData,
                            const boost::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
                            const boost::shared_ptr<ore::analytics::SensitivityScenarioData>& sensitivityData,
                            const bool recalibrateModels,
                            const boost::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                            const boost::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
                            const bool nonShiftedBaseCurrencyConversion = false,
                            const boost::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                            const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                            const bool continueOnError = false, bool analyticFxSensis = false, bool dryRun = false,
                            const std::string& context = "sensi analysis")
        : ore::analytics::SensitivityAnalysis(portfolio, nullptr, marketConfiguration, engineData, simMarketData,
                                              sensitivityData, recalibrateModels, curveConfigs, todaysMarketParams,
                                              nonShiftedBaseCurrencyConversion, referenceData, iborFallbackConfig,
                                              continueOnError, analyticFxSensis, dryRun),
          useSingleThreadedEngine_(false), nThreads_(nThreads), loader_(loader), context_(context) {
        asof_ = asof;
    }

    void generateSensitivities(boost::shared_ptr<ore::analytics::NPVSensiCube> cube =
                                   boost::shared_ptr<ore::analytics::NPVSensiCube>()) override;

protected:
    //! initialize the SensitivityScenarioGenerator that determines which sensitivities to compute
    virtual void initializeSimMarket(boost::shared_ptr<ore::analytics::ScenarioFactory> scenFact = {}) override;
    virtual void initialize(boost::shared_ptr<ore::analytics::NPVSensiCube>& cube) override;
    virtual void initializeCube(boost::shared_ptr<ore::analytics::NPVSensiCube>& cube) const override;
    std::vector<boost::shared_ptr<ore::data::EngineBuilder>> extraBuilders_;
    std::vector<boost::shared_ptr<ore::data::LegBuilder>> extraLegBuilders_;

    //! reset and rebuild the portfolio to make use of the appropriate engine factory
    void resetPortfolio(const boost::shared_ptr<ore::data::EngineFactory>& factory) override;

    //! Overwrite FX sensitivities in the cube with first order analytical values where possible.
    void addAnalyticFxSensitivities() override;

private:
    bool useSingleThreadedEngine_;

    // additional members needed for multihreaded constructor
    Size nThreads_;
    boost::shared_ptr<ore::data::Loader> loader_;
    std::string context_;
};
} // namespace analytics
} // namespace ore
