/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file orea/engine/historicalpnlgenerator.hpp
    \brief Class for generating portfolio P&Ls based on historical scenarios
*/

#pragma once

#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/utilities/timeperiod.hpp>
#include <orea/scenario/historicalscenariogenerator.hpp>
#include <ql/types.hpp>
#include <vector>

namespace ore {
namespace analytics {

/*! Class for generating historical P&L vectors for a given portfolio in a given currency.

    In particular, assume that the portfolio has a base NPV, \f$\Pi_0\f$, today i.e. at
    \f$t_0\f$. This class takes a HistoricalScenarioGenerator which holds a set of historical
    market moves, over a given period \f$\tau\f$ e.g. 10 business days, for a set of past dates
    \f$\{d_1, d_2, \ldots, d_N\}\f$. This class calculates the P&L changes on the portfolio,
    \f$\{\Delta_1, \Delta_2, \ldots, \Delta_N\}\f$, resulting from applying these market moves to
    the base market. In other words, \f$\Delta_i = \Pi_i - \Pi_0 \f$ where \f$\Pi_i\f$ is the
    portfolio NPV under the shifted market corresponding to date \f$d_i\f$ for
    \f$i = 1, 2, \ldots, N\f$.

    In the calculation of P&L, the class allows the scenario shifts to be filtered and also the
    trades to be filtered.
*/
class HistoricalPnlGenerator : public ore::data::ProgressReporter {
public:
    /*! Constructor to use a single-threaded valuation engine
        \param baseCurrency      currency in which the P&Ls will be calculated
        \param portfolio         portfolio of trades for which P&Ls will be calculated
        \param simMarket         simulation market used for valuation
        \param hisScenGen        historical scenario generator
        \param cube              an NPV cube that will be populated by each call to generateCube
        \param modelBuilders     model builders to update during a val engine run
        \param dryRun            for testing - limit the number of scenarios to one and fill the cube with random data
    */
    HistoricalPnlGenerator(const std::string& baseCurrency, const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                           const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket,
                           const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen,
                           const QuantLib::ext::shared_ptr<NPVCube>& cube,
                           const set<std::pair<string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>>& modelBuilders = {},
                           bool dryRun = false);

    /*! Constructor to use a multi-threaded valuation engine */
    HistoricalPnlGenerator(const string& baseCurrency, const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                           const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen,
                           const QuantLib::ext::shared_ptr<EngineData>& engineData, const Size nThreads, const Date& today,
                           const QuantLib::ext::shared_ptr<ore::data::Loader>& loader,
                           const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
                           const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
                           const std::string& configuration,
                           const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
                           const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr,
                           const IborFallbackConfig& iborFallbackConfig = IborFallbackConfig::defaultConfig(),
                           bool dryRun = false, const std::string& context = "historical pnl generation");

    /*! Generate a "cube" of P&L values for the trades in the portfolio on each of
        the scenarios provided by the historical scenario generator. The historical
        scenarios will have the given \p filter applied.
    */
    void generateCube(const QuantLib::ext::shared_ptr<ScenarioFilter>& filter);

    /*! Return a vector of historical portfolio P&L values restricted to scenarios
        falling in \p period and restricted to the given \p tradeIds. The P&L values
        are calculated from the last cube generated by generateCube.
    */
    std::vector<QuantLib::Real> pnl(const ore::data::TimePeriod& period,
                                    const std::set<std::pair<std::string, QuantLib::Size>>& tradeIds) const;

    /*! Return a vector of historical portfolio P&L values restricted to scenarios
        falling in \p period. The P&L values are calculated from the last cube
        generated by generateCube.
    */
    std::vector<QuantLib::Real> pnl(const ore::data::TimePeriod& period) const;

    /*! Return a vector of historical portfolio P&L values restricted to the given \p tradeIds.
        The P&L values are calculated from the last cube generated by generateCube.
    */
    std::vector<QuantLib::Real> pnl(const std::set<std::pair<std::string, QuantLib::Size>>& tradeIds) const;

    /*! Return a vector of historical portfolio P&L values for all scenarios generated
        by the historical scenario generator. The P&L values are calculated from the
        last cube generated by generateCube.
    */
    std::vector<QuantLib::Real> pnl() const;

    /*! Return a vector of historical trade level P&L values restricted to scenarios falling in \p period and
        restricted to the given \p tradeIds. The P&L values are calculated from the last cube generated by
        generateCube. The first dimension is time and the second dimension is tradeId.
    */
    using TradePnlStore = std::vector<std::vector<QuantLib::Real>>;
    TradePnlStore tradeLevelPnl(const ore::data::TimePeriod& period,
                                const std::set<std::pair<std::string, QuantLib::Size>>& tradeIds) const;

    /*! Return a vector of historical trade level P&L values restricted to scenarios falling in \p period. The P&L
        values are calculated from the last cube generated by generateCube. The first dimension is time and the
        second dimension is tradeId.
    */
    TradePnlStore tradeLevelPnl(const ore::data::TimePeriod& period) const;

    /*! Return a vector of historical trade level P&L values restricted to the given \p tradeIds. The P&L
        values are calculated from the last cube generated by generateCube. The first dimension is time and the
        second dimension is tradeId.
    */
    TradePnlStore tradeLevelPnl(const std::set<std::pair<std::string, QuantLib::Size>>& tradeIds) const;

    /*! Return a vector of historical trade level P&L values for all scenarios generated by the historical scenario
        generator. The P&L values are calculated from the last cube generated by generateCube. The first dimension is
        time and the second dimension is tradeId.
    */
    TradePnlStore tradeLevelPnl() const;

    /*! Return the last cube generated by generateCube.
     */
    const QuantLib::ext::shared_ptr<NPVCube>& cube() const;

    //! Set of trade ID and index pairs for all trades.
    std::set<std::pair<std::string, QuantLib::Size>> tradeIdIndexPairs() const;

    //! Time period covered by the historical P&L generator.
    ore::data::TimePeriod timePeriod() const;

private:
    bool useSingleThreadedEngine_;

    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio_;
    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket_;
    QuantLib::ext::shared_ptr<HistoricalScenarioGenerator> hisScenGen_;
    QuantLib::ext::shared_ptr<NPVCube> cube_;
    QuantLib::ext::shared_ptr<ValuationEngine> valuationEngine_;

    // additional parameters needed for multi-threaded ctor
    QuantLib::ext::shared_ptr<ore::data::EngineData> engineData_;
    Size nThreads_;
    Date today_;
    QuantLib::ext::shared_ptr<ore::data::Loader> loader_;
    QuantLib::ext::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
    QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    std::string configuration_;
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketData_;
    QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager> referenceData_;
    ore::data::IborFallbackConfig iborFallbackConfig_;

    bool dryRun_;
    std::string context_;

    std::function<std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>>()> npvCalculator_;

    //! Get the index of the as of date in the cube.
    QuantLib::Size indexAsof() const;
};

} // namespace analytics
} // namespace ore
