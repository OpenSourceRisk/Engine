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

/*! \file orea/engine/marketriskbacktest.hpp
    \brief bace class for all market risk backtests
    \ingroup engine
*/

#pragma once

#include <qle/math/covariancesalvage.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/utilities/progressbar.hpp>
#include <orea/engine/historicalpnlgenerator.hpp>
#include <orea/engine/historicalsensipnlcalculator.hpp>
#include <orea/engine/varcalculator.hpp>
#include <orea/scenario/scenariofilter.hpp>
#include <orea/scenario/scenarioshiftcalculator.hpp>
#include <orea/engine/marketriskreport.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <boost/make_shared.hpp>

namespace ore {
namespace analytics {

class MarketRiskBacktest : public ore::analytics::MarketRiskReport {
public:
    //! VAR types used as a benchmark against which SIMM can be compared
    enum class VarType { HistSim, HistSimTaylor, Parametric, Lch };
    
    class BacktestReports : public ore::analytics::MarketRiskReport::Reports {
    public:
        //! Report types that can be populated during a SIMM backtest
        enum class ReportType { Summary, Detail, PnlContribution, DetailTrade, PnlContributionTrade };

        BacktestReports() : ore::analytics::MarketRiskReport::Reports() {}
        void add(const QuantLib::ext::shared_ptr<ore::data::Report>& report) {
            QL_FAIL("Please use alternative add method, providing a ReportType");
        }
        void add(ReportType type, const QuantLib::ext::shared_ptr<ore::data::Report>& report) {
            types_.push_back(type);
            reports_.push_back(report);
        }
        const bool has(ReportType type) const {
            auto it = std::find(types_.begin(), types_.end(), type);
            return it != types_.end();
        }
        const QuantLib::ext::shared_ptr<ore::data::Report>& get(ReportType type);

    protected:
        std::vector<ReportType> types_;
    };

    struct BacktestArgs {
        //! Time period over which to perform the backtest
        ore::data::TimePeriod backtestPeriod_;
        //! Time period over which to calculate the benchmark VAR
        ore::data::TimePeriod benchmarkPeriod_;
        //! Confidence level in the SIMM backtest
        QuantLib::Real confidence_;
        //! Amount by which absolute P&L value must exceed 0 for exception counting
        QuantLib::Real exceptionThreshold_;
        bool tradeDetailIncludeAllColumns_ = false;

        //! Call side trade IDs to be considered in the backtest. Other trades' PnLs will be removed from the total PnL
        std::set<std::string> callTradeIds_ = {};
        //! Post side trade IDs to be considered in the backtest. Other trades' PnLs will be removed from the total PnL
        std::set<std::string> postTradeIds_ = {};

        //! Confidence levels that feed in to defining the stop light bounds
        std::vector<QuantLib::Real> ragLevels_ = {0.95, 0.9999};
        BacktestArgs(
            ore::data::TimePeriod btPeriod, ore::data::TimePeriod bmPeriod,
            QuantLib::Real conf = 0.99, QuantLib::Real exThres = 0.01, 
            bool tdc = false, const std::set<std::string>& callTradeIds = {},
            const std::set<std::string>& postTradeIds = {})
            : backtestPeriod_(btPeriod), benchmarkPeriod_(bmPeriod),
              confidence_(conf), exceptionThreshold_(exThres),
              tradeDetailIncludeAllColumns_(tdc), callTradeIds_(callTradeIds), postTradeIds_(postTradeIds) {}
    };

    //! Used to pass information
    struct Data {
        std::string counterparty;
        QuantLib::ext::shared_ptr<ore::analytics::TradeGroupBase> tradeGroup;
        QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase> riskGroup;
    };
    
    //! Used to store results for writing rows in the summary report
    struct SummaryResults {
        QuantLib::Size observations;
        QuantLib::Real callValue;
        QuantLib::Size callExceptions;
        QuantLib::Real postValue;
        QuantLib::Size postExceptions;
        std::vector<QuantLib::Size> bounds;
    };

    struct VarBenchmark {
        VarType type;
        QuantLib::ext::shared_ptr<ore::analytics::VarCalculator> calculator;
        QuantLib::Real var = 0.0;

        VarBenchmark(VarType type, QuantLib::ext::shared_ptr<ore::analytics::VarCalculator> calculator,
                     QuantLib::Real var = 0.0)
            : type(type), calculator(calculator), var(var) {}

        void reset() { var = 0.0; }
    };

    MarketRiskBacktest(const std::string& calculationCurrency,
                       const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                       const std::string& portfolioFilter,
                       std::unique_ptr<BacktestArgs> btArgs,
                       std::unique_ptr<SensiRunArgs> sensiArgs = nullptr,
                       std::unique_ptr<FullRevalArgs> revalArgs = nullptr,
                       std::unique_ptr<MultiThreadArgs> mtArgs = nullptr,
                       const QuantLib::ext::shared_ptr<ore::analytics::HistoricalScenarioGenerator>& hisScenGen = nullptr,
                       const bool breakdown = false,
                       const bool requireTradePnl = false);

    virtual ~MarketRiskBacktest() {}

    /*! Check if the given scenario \p filter turns off all risk factors in the
        historical scenario generator
    */
    bool disablesAll(const QuantLib::ext::shared_ptr<ore::analytics::ScenarioFilter>& filter) const override;
        
    //! Add a row to the P&L contribution report
    virtual void addPnlRow(const QuantLib::ext::shared_ptr<BacktestReports>& reports, QuantLib::Size scenarioIdx,
                           bool isCall, const ore::analytics::RiskFactorKey& key_1, QuantLib::Real shift_1,
                           QuantLib::Real delta, QuantLib::Real gamma, QuantLib::Real deltaPnl, QuantLib::Real gammaPnl,
                           const ore::analytics::RiskFactorKey& key_2 = ore::analytics::RiskFactorKey(),
                           QuantLib::Real shift_2 = 0.0, const std::string& tradeId = "",
                           const std::string& currency = "", QuantLib::Real fxSpot = 1.0);

protected:
    std::unique_ptr<BacktestArgs> btArgs_;

    void initialise() override;
    
    //! pointers to the VAR benchmarks
    typedef std::map<VarType, std::pair<QuantLib::ext::shared_ptr<ore::analytics::VarCalculator>, QuantLib::Real>>
        VarBenchmarks;
    VarBenchmarks sensiCallBenchmarks_, sensiPostBenchmarks_, fullRevalCallBenchmarks_, fullRevalPostBenchmarks_;

    //! variables for benchmark calculations
    std::vector<QuantLib::Real> bmSensiPnls_, bmFoSensiPnls_, pnls_, bmPnls_, sensiPnls_, foSensiPnls_;
    ore::analytics::TradePnLStore foTradePnls_, tradePnls_, sensiTradePnls_;
    std::set<std::string> callTradeIds_;
    std::set<std::string> postTradeIds_;

    virtual const std::vector<std::tuple<std::string, ore::data::Report::ReportType, QuantLib::Size>> summaryColumns() = 0;
    virtual const std::vector<std::tuple<std::string, ore::data::Report::ReportType, QuantLib::Size, bool>> detailColumns() = 0;
    virtual const std::vector<std::tuple<std::string, ore::data::Report::ReportType, QuantLib::Size>> pnlColumns() = 0;
    virtual QuantLib::Real callValue(const Data& data) = 0;
    virtual QuantLib::Real postValue(const Data& data) = 0;
    virtual std::string counterparty(const std::string& tradeId) const = 0;
    virtual void setUpBenchmarks() = 0;
    virtual void reset(const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup) override;

    void createReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) override;
    virtual bool runTradeDetail(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) override;
    ore::data::TimePeriod covariancePeriod() const override { return btArgs_->benchmarkPeriod_; }
    void addPnlCalculators(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) override;
    
    void handleSensiResults(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
                            const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup,
                            const QuantLib::ext::shared_ptr<ore::analytics::TradeGroupBase>& tradeGroup) override;

    void handleFullRevalResults(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
                                const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup,
                                const QuantLib::ext::shared_ptr<ore::analytics::TradeGroupBase>& tradeGroup) override;

    virtual void adjustFullRevalPnls(std::vector<QuantLib::Real>& pnls, std::vector<QuantLib::Real>& bmPnls,
                                     ore::analytics::TradePnLStore& tradePnls,
                                     const std::vector<QuantLib::Real>& foSensiPnls,
                                     const std::vector<QuantLib::Real>& bmFoSensiPnls,
                                     const ore::analytics::TradePnLStore& foTradePnls,
                                     const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup){};
    
    //! Add a row to the detail report
    virtual void addDetailRow(const QuantLib::ext::shared_ptr<BacktestReports>& reports, const Data& data, bool isCall,
        QuantLib::Real im, const QuantLib::Date& start, const QuantLib::Date& end, bool isFull, QuantLib::Real pnl, 
        const std::string& result, const std::string& tradeId = "") const = 0;

    //! Add a row to the summary report
    virtual void addSummaryRow(const QuantLib::ext::shared_ptr<BacktestReports>& reports, const Data& data, bool isCall,
                               QuantLib::Real im, QuantLib::Size observations,
                               bool isFull, QuantLib::Size exceptions, const std::vector<QuantLib::Size>& ragBounds,
                               const VarBenchmarks& sensiBenchmarks, const VarBenchmarks& fullBenchmarks) const = 0;

    //! Calculate and update the benchmarks
    virtual void calculateBenchmarks(VarBenchmarks& benchmarks, QuantLib::Real confidence, const bool isCall,
                                     const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup,
                                     std::set<std::pair<std::string, QuantLib::Size>>& tradeIdIdxPairs);

    void writeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
                      const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup,
                      const QuantLib::ext::shared_ptr<ore::analytics::TradeGroupBase>& tradeGroup) override;

   std::vector<ore::data::TimePeriod> timePeriods() override;

 private:
    /*! Calculate the number of exceptions given the current \p data and the associated
    P&L vector \p pnls for both call and post sides. The parameter
    \p isFull is <code>true</code> if pnls come from a full revaluation and <code>false</code>
    if they are sensitivity based.

    The parameters \p tradeIds and \p tradePnls are used if we are writing a trade level backtest
    detail report.
    */
     SummaryResults calculateSummary(const QuantLib::ext::shared_ptr<BacktestReports>& reports, const Data& data,
                                     bool isFull,
         const std::vector<QuantLib::Real>& pnls, const std::vector<std::string>& tradeIds, const TradePnLStore& tradePnls);
};

class BacktestPNLCalculator : public PNLCalculator {
public:
    BacktestPNLCalculator(ore::data::TimePeriod pnlPeriod, const bool& writePnl, MarketRiskBacktest* backtest,
                          const QuantLib::ext::shared_ptr<MarketRiskBacktest::BacktestReports>& reports)
        : PNLCalculator(pnlPeriod), writePnl_(writePnl), backtest_(backtest), reports_(reports) {}

    void writePNL(QuantLib::Size scenarioIdx, bool isCall, const RiskFactorKey& key_1,
                  QuantLib::Real shift_1, QuantLib::Real delta, QuantLib::Real gamma, QuantLib::Real deltaPnl, 
                  QuantLib::Real gammaPnl, const RiskFactorKey& key_2 = RiskFactorKey(),
                  QuantLib::Real shift_2 = 0.0, const std::string& tradeId = "") override;

    const TradePnLStore& tradePnls() { return tradePnls_; }
    const TradePnLStore& foTradePnls() { return foTradePnls_; }

private:
    const bool& writePnl_ = false;
    MarketRiskBacktest* backtest_;
    QuantLib::ext::shared_ptr<MarketRiskBacktest::BacktestReports> reports_;
};

} // namespace analytics
} // namespace ore
