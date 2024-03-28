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

/*! \file engine/varcalculator.hpp
    \brief Base class for a var calculation
    \ingroup engine
*/

#pragma once

#include <ored/report/report.hpp>
#include <orea/engine/riskfilter.hpp>
#include <orea/engine/marketriskreport.hpp>

namespace ore {
namespace analytics {

class VarRiskGroup : public MarketRiskGroup {
public:
    VarRiskGroup() {}
    VarRiskGroup(VarConfiguration::RiskClass riskClass, VarConfiguration::RiskType riskType)
        : riskClass_(riskClass), riskType_(riskType) {}

    VarConfiguration::RiskClass riskClass() const { return riskClass_; };
    VarConfiguration::RiskType riskType() const { return riskType_; };

    std::string to_string() override;
    bool allLevel() const override;

private:
    VarConfiguration::RiskClass riskClass_;
    VarConfiguration::RiskType riskType_;
};

class VarRiskGroupContainer : public MarketRiskGroupContainer {
public:
    VarRiskGroupContainer() {}

    //! Used to order pairs [Risk class, Risk Type]
    struct CompRisk {
        static std::map<VarConfiguration::RiskClass, QuantLib::Size> rcOrder;
        static std::map<VarConfiguration::RiskType, QuantLib::Size> rtOrder;

        bool operator()(const QuantLib::ext::shared_ptr<VarRiskGroup>& lhs,
                        const QuantLib::ext::shared_ptr<VarRiskGroup>& rhs) const;
    };

    QuantLib::ext::shared_ptr<MarketRiskGroup> next() override;
    void add(const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup) override;
    void reset() override;
    QuantLib::Size size() override;

private:
    /*! Set of pairs [Risk Class, Risk Type] that will need to be covered by backtest
        Each pair in the set defines a particular filter of the risk factors in the backtest
    */
    std::set<QuantLib::ext::shared_ptr<VarRiskGroup>, CompRisk> riskGroups_;
    std::set<QuantLib::ext::shared_ptr<VarRiskGroup>>::const_iterator rgIdx_;
};

class VarTradeGroup : public TradeGroup {
public:
    VarTradeGroup() {}
    VarTradeGroup(std::string portfolioId) : portfolioId_(portfolioId) {}

    const std::string& portfolioId() const { return portfolioId_; };

    std::string to_string() override { return portfolioId_; };
    bool allLevel() const override { return true; };

private:
    std::string portfolioId_;
};

class VarTradeGroupContainer : public TradeGroupContainer {
public:
    VarTradeGroupContainer() {}

    QuantLib::ext::shared_ptr<TradeGroup> next() override;
    void add(const QuantLib::ext::shared_ptr<TradeGroup>& tradeGroup) override;
    void reset() override;

private:
    std::set<QuantLib::ext::shared_ptr<VarTradeGroup>> tradeGroups_;
    std::set<QuantLib::ext::shared_ptr<VarTradeGroup>>::const_iterator tgIdx_;
};

//! VaR Calculator
class VarCalculator {
public:
    VarCalculator() {}
    virtual ~VarCalculator() {}

    virtual QuantLib::Real var(QuantLib::Real confidence, const bool isCall = true, 
        const std::set<std::pair<std::string, QuantLib::Size>>& tradeIds = {}) = 0;
};

class VarReport : public MarketRiskReport {
public:
    VarReport(const std::string& baseCurrency,
        const QuantLib::ext::shared_ptr<Portfolio> & portfolio,
        const std::string& portfolioFilter,
        const vector<Real>& p, boost::optional<ore::data::TimePeriod> period,
        const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen = nullptr,
        std::unique_ptr<SensiRunArgs> sensiArgs = nullptr, std::unique_ptr<FullRevalArgs> fullRevalArgs = nullptr, 
        const bool breakdown = false);

    void createReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) override;

    const std::vector<Real>& p() const { return p_; }

protected:
    QuantLib::ext::shared_ptr<VarCalculator> varCalculator_;
    
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioFilter>
    createScenarioFilter(const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup) override;

    virtual void createVarCalculator() = 0;
    std::string portfolioId(const QuantLib::ext::shared_ptr<TradeGroup>& tradeGroup) const override;
    std::string tradeGroupKey(const QuantLib::ext::shared_ptr<TradeGroup>& tradeGroup) const override;
    void writeVarResults(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& report,
                         const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup,
                         const QuantLib::ext::shared_ptr<TradeGroup>& tradeGroup);

    virtual std::vector<ore::data::TimePeriod> timePeriods() { return {period_.get()}; }

private:
    QuantLib::ext::shared_ptr<Portfolio> portfolio_;
    std::string portfolioFilter_;
    std::vector<Real> p_;
};

} // namespace analytics
} // namespace ore
