/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file engine/marketriskreport.hpp
    \brief Base class for a market risk report
    \ingroup engine
*/

#pragma once

#include <qle/math/covariancesalvage.hpp>
#include <ored/report/report.hpp>
#include <orea/engine/historicalsensipnlcalculator.hpp>
#include <orea/engine/historicalpnlgenerator.hpp>
#include <orea/engine/riskfilter.hpp>
#include <orea/scenario/scenariofilter.hpp>

#include <vector>

namespace ore {
namespace analytics {

using TradePnLStore = std::vector<std::vector<QuantLib::Real>>;

class MarketRiskGroupBase {
public:
    MarketRiskGroupBase() {}
    virtual ~MarketRiskGroupBase() {}

    virtual std::string to_string() = 0;
    virtual bool allLevel() const = 0;
};

std::ostream& operator<<(std::ostream& out, const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup);

class MarketRiskGroupBaseContainer {
public:
    MarketRiskGroupBaseContainer() {}
    virtual ~MarketRiskGroupBaseContainer() {}

    virtual QuantLib::ext::shared_ptr<MarketRiskGroupBase> next() = 0;
    virtual void add(const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup) = 0;
    virtual void reset() = 0;
    virtual QuantLib::Size size() = 0;
};

class TradeGroupBase {
public:
    TradeGroupBase() {}
    virtual ~TradeGroupBase() {}

    virtual std::string to_string() = 0;
    virtual bool allLevel() const = 0;
};

std::ostream& operator<<(std::ostream& out, const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup);

class TradeGroupBaseContainer {
public:
    TradeGroupBaseContainer() {}
    virtual ~TradeGroupBaseContainer() {}

    virtual QuantLib::ext::shared_ptr<TradeGroupBase> next() = 0;
    virtual void add(const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) = 0;
    virtual void reset() = 0;
};

class MarketRiskGroup : public MarketRiskGroupBase {
public:
    MarketRiskGroup() {}
    MarketRiskGroup(MarketRiskConfiguration::RiskClass riskClass, MarketRiskConfiguration::RiskType riskType)
        : riskClass_(riskClass), riskType_(riskType) {}

    MarketRiskConfiguration::RiskClass riskClass() const { return riskClass_; };
    MarketRiskConfiguration::RiskType riskType() const { return riskType_; };

    std::string to_string() override;
    bool allLevel() const override;

private:
    MarketRiskConfiguration::RiskClass riskClass_;
    MarketRiskConfiguration::RiskType riskType_;
};

class MarketRiskGroupContainer : public MarketRiskGroupBaseContainer {
public:
    MarketRiskGroupContainer() {}

    //! Used to order pairs [Risk class, Risk Type]
    struct CompRisk {
        static std::map<MarketRiskConfiguration::RiskClass, QuantLib::Size> rcOrder;
        static std::map<MarketRiskConfiguration::RiskType, QuantLib::Size> rtOrder;

        bool operator()(const QuantLib::ext::shared_ptr<MarketRiskGroup>& lhs,
                        const QuantLib::ext::shared_ptr<MarketRiskGroup>& rhs) const;
    };

    QuantLib::ext::shared_ptr<MarketRiskGroupBase> next() override;
    void add(const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup) override;
    void reset() override;
    QuantLib::Size size() override;

private:
    /*! Set of pairs [Risk Class, Risk Type] that will need to be covered by backtest
        Each pair in the set defines a particular filter of the risk factors in the backtest
    */
    std::set<QuantLib::ext::shared_ptr<MarketRiskGroup>, CompRisk> riskGroups_;
    std::set<QuantLib::ext::shared_ptr<MarketRiskGroup>>::const_iterator rgIdx_;
};

class TradeGroup : public TradeGroupBase {
public:
    TradeGroup() {}
    TradeGroup(std::string portfolioId) : portfolioId_(portfolioId) {}

    const std::string& portfolioId() const { return portfolioId_; };

    std::string to_string() override { return portfolioId_; };
    bool allLevel() const override { return true; };

private:
    std::string portfolioId_;
};

class TradeGroupContainer : public TradeGroupBaseContainer {
public:
    TradeGroupContainer() {}

    QuantLib::ext::shared_ptr<TradeGroupBase> next() override;
    void add(const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) override;
    void reset() override;

private:
    std::set<QuantLib::ext::shared_ptr<TradeGroup>> tradeGroups_;
    std::set<QuantLib::ext::shared_ptr<TradeGroup>>::const_iterator tgIdx_;
};

class MarketRiskReport : public ore::data::ProgressReporter {
public:
    struct SensiRunArgs {
        //! Stream of sensitivity records used for the sensitivity based backtest
        QuantLib::ext::shared_ptr<SensitivityStream> sensitivityStream_;
        //! Calculates the shift multiple in moving from one scenario to another
        QuantLib::ext::shared_ptr<ScenarioShiftCalculator> shiftCalculator_;
        //! Amount by which absolute P&L value must exceed 0 to be written to P&L contribution report
        QuantLib::Real pnlWriteThreshold_;
        //! Optional input of covariance matrix
        std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covarianceInput_;

        SensiRunArgs(const QuantLib::ext::shared_ptr<SensitivityStream>& ss,
                     const QuantLib::ext::shared_ptr<ScenarioShiftCalculator>& sc,
                     QuantLib::Real pnlThres = 0.01,
                     std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> ci = {})
            : sensitivityStream_(ss), shiftCalculator_(sc), pnlWriteThreshold_(pnlThres),
              covarianceInput_(ci) {}
    };

    struct FullRevalArgs {
        QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket> simMarket_;
        QuantLib::ext::shared_ptr<ore::data::EngineData> engineData_;
        QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager> referenceData_;
        ore::data::IborFallbackConfig iborFallbackConfig_;
        bool dryRun_ = false;
        //! True to enable cube writing
        bool writeCube_ = false;
        //! If cube writing is enabled, cube(s) will be written to this directory
        std::string cubeDir_;
        /*! If cube writing is enabled, cube(s) will be written to this file with the
            FILTER pattern replaced by a description of the scenario filter
        */
        std::string cubeFilename_;

        FullRevalArgs(const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& sm,
                      const QuantLib::ext::shared_ptr<ore::data::EngineData>& ed,
                      const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& rd = nullptr,
                      const ore::data::IborFallbackConfig ifc = ore::data::IborFallbackConfig::defaultConfig(),
                      const bool dr = false)
            : simMarket_(sm), engineData_(ed), referenceData_(rd), iborFallbackConfig_(ifc), dryRun_(dr) {}
    };

    struct MultiThreadArgs {
        QuantLib::Size nThreads_;
        QuantLib::Date today_;
        QuantLib::ext::shared_ptr<ore::data::Loader> loader_;
        QuantLib::ext::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
        QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
        std::string configuration_;
        QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketData_;
        std::string context_;

        MultiThreadArgs(QuantLib::Size n, QuantLib::Date t, const QuantLib::ext::shared_ptr<ore::data::Loader>& l,
                        const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& cc,
                        const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& tmp, std::string conf,
                        const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& smd,
                        const std::string& context)
            : nThreads_(n), today_(t), loader_(l), curveConfigs_(cc), todaysMarketParams_(tmp), configuration_(conf),
              simMarketData_(smd), context_(context) {}
    };

    class Reports {
    public:
        virtual ~Reports() {} 
        Reports() {}
        void add(const QuantLib::ext::shared_ptr<ore::data::Report>& report) { reports_.push_back(report); }
        const std::vector<QuantLib::ext::shared_ptr<ore::data::Report>>& reports() const { return reports_; };

    protected:
        std::vector<QuantLib::ext::shared_ptr<ore::data::Report>> reports_;
    };

    MarketRiskReport(const std::string& calculationCurrency, const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                     const std::string& portfolioFilter, boost::optional<ore::data::TimePeriod> period,
                     const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen = nullptr, 
        std::unique_ptr<SensiRunArgs> sensiArgs = nullptr, std::unique_ptr<FullRevalArgs> fullRevalArgs = nullptr,
        std::unique_ptr<MultiThreadArgs> multiThreadArgs = nullptr, const bool breakdown = false, 
        const bool requireTradePnl = false)
        : calculationCurrency_(calculationCurrency), portfolio_(portfolio), portfolioFilter_(portfolioFilter), period_(period),
          hisScenGen_(hisScenGen), sensiArgs_(std::move(sensiArgs)),
          fullRevalArgs_(std::move(fullRevalArgs)),  multiThreadArgs_(std::move(multiThreadArgs)), breakdown_(breakdown), 
          requireTradePnl_(requireTradePnl) {
    }
    virtual ~MarketRiskReport() {}

    virtual void initialise();
    //! Method to init simMarket_ for multi-threaded ctors
    void initSimMarket();

    virtual void calculate(const QuantLib::ext::shared_ptr<Reports>& report);

    /*! Enable cube file generation to the given output directory \p cubeDir with the
        given cube file name \p cubeFilename. The \p cubeFilename should be of the form
        'stemFILTER.dat'. In the code the 'FILTER' piece is replaced with a description
        of the scenario filter under which the cube was generated so that separate cube
        files are generated for each scenario filter.

        \remark This will only have an effect when full revaluation is enabled
    */
    void enableCubeWrite(const std::string& cubeDir, const std::string& cubeFilename);

protected:
    //! Method for shared initialisation
    virtual void initialiseRiskGroups();

    bool sensiBased_ = false;
    bool fullReval_ = false;

    std::string calculationCurrency_;
    QuantLib::ext::shared_ptr<Portfolio> portfolio_;
    std::string portfolioFilter_;
    boost::optional<ore::data::TimePeriod> period_;
    QuantLib::ext::shared_ptr<HistoricalScenarioGenerator> hisScenGen_;
    std::unique_ptr<SensiRunArgs> sensiArgs_;
    std::unique_ptr<FullRevalArgs> fullRevalArgs_;
    std::unique_ptr<MultiThreadArgs> multiThreadArgs_;

    bool breakdown_ = false;
    // Whether we require trade-level PnLs to use for later calculations
    bool requireTradePnl_ = false;

    QuantLib::ext::shared_ptr<MarketRiskGroupBaseContainer> riskGroups_;
    QuantLib::ext::shared_ptr<TradeGroupBaseContainer> tradeGroups_;

    /*! Partition of portfolio's trade IDs (and index as a pair), into groups.
     */
    std::map<std::string, std::set<std::pair<std::string, QuantLib::Size>>> tradeIdGroups_;
    std::set<std::pair<std::string, QuantLib::Size>> tradeIdIdxPairs_;
    // Set of trade IDs in this trade group.
    std::vector<std::string> tradeIds_;

    std::map<RiskFactorKey, QuantLib::Real> deltas_;
    std::map<std::pair<RiskFactorKey, RiskFactorKey>, QuantLib::Real> gammas_;
    QuantLib::Matrix covarianceMatrix_;
    bool writePnl_ = false;
    std::vector<QuantLib::ext::shared_ptr<PNLCalculator>> pnlCalculators_;
    QuantLib::ext::shared_ptr<QuantExt::CovarianceSalvage> salvage_ =
        QuantLib::ext::make_shared<QuantExt::SpectralCovarianceSalvage>();
    bool includeDeltaMargin_ = true;
    bool includeGammaMargin_ = true;

    // initialized if using single threaded engine and fullRevaluation_ is true
    QuantLib::ext::shared_ptr<ore::data::EngineFactory> factory_;

    QuantLib::ext::shared_ptr<ore::analytics::HistoricalPnlGenerator> histPnlGen_;
    QuantLib::ext::shared_ptr<HistoricalSensiPnlCalculator> sensiPnlCalculator_;

    virtual void registerProgressIndicators();
    virtual void createReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) = 0;
    virtual bool runTradeDetail(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) { return requireTradePnl_; };
    virtual QuantLib::ext::shared_ptr<ScenarioFilter>
    createScenarioFilter(const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup);

    virtual void reset(const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup);
    virtual bool runTradeRiskGroup(const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup,
                                   const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup) const {
        return true;
    }

    /*! Check if the given scenario \p filter turns off all risk factors in the
        historical scenario generator
    */
    virtual bool disablesAll(const QuantLib::ext::shared_ptr<ScenarioFilter>& filter) const { return false; };

    //! update any filters required
    virtual void updateFilter(const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                              const QuantLib::ext::shared_ptr<ScenarioFilter>& filter) {}
    virtual std::string portfolioId(const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) const;
    virtual std::string tradeGroupKey(const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) const;
    virtual ore::data::TimePeriod covariancePeriod() const { return period_.value(); }
    virtual void addPnlCalculators(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {}

    // handle results specific to this report
    virtual void handleSensiResults(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& report,
                                    const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                                    const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup){};

    virtual void handleFullRevalResults(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
                                        const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                                        const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup){};

    virtual bool includeDeltaMargin(const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup) const {
        return true;
    }
    virtual bool includeGammaMargin(const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup) const {
        return true;
    }
    virtual bool runFullReval(const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup) const { return true; }
    virtual bool generateCube(const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup) const { return true; }
    virtual std::string cubeFilePath(const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup) const {
        return std::string();
    }
    virtual std::vector<ore::data::TimePeriod> timePeriods() { return {period_.get()}; }
    virtual void writeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
                              const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                              const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) {}
    virtual void closeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports);
};
} // namespace analytics
} // namespace ore
