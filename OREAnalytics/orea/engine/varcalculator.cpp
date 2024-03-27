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
#include <orea/engine/varcalculator.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/math/deltagammavar.hpp>

#include <boost/regex.hpp>

namespace ore {
namespace analytics {

string VarRiskGroup::to_string() {
    return "[" + ore::data::to_string(riskClass_) + ", " + ore::data::to_string(riskType_) + "]";
}

bool VarRiskGroup::allLevel() const {
    return riskClass_ == VarConfiguration::RiskClass::All && riskType_ == VarConfiguration::RiskType::All;
}

map<VarConfiguration::RiskClass, Size> VarRiskGroupContainer::CompRisk::rcOrder = {
    {VarConfiguration::RiskClass::All, 0},
    {VarConfiguration::RiskClass::InterestRate, 1},
    {VarConfiguration::RiskClass::Inflation, 2},
    {VarConfiguration::RiskClass::Credit, 3},
    {VarConfiguration::RiskClass::Equity, 4},
    {VarConfiguration::RiskClass::FX, 5}};

map<VarConfiguration::RiskType, Size> VarRiskGroupContainer::CompRisk::rtOrder = {
    {VarConfiguration::RiskType::All, 0},       {VarConfiguration::RiskType::DeltaGamma, 1},
    {VarConfiguration::RiskType::Vega, 2},      {VarConfiguration::RiskType::BaseCorrelation, 3}};

bool VarRiskGroupContainer::CompRisk::operator()(const QuantLib::ext::shared_ptr<VarRiskGroup>& lhs,
                                                  const QuantLib::ext::shared_ptr<VarRiskGroup>& rhs) const {

    if (rcOrder.at(lhs->riskClass()) < rcOrder.at(rhs->riskClass()))
        return true;
    if (rcOrder.at(lhs->riskClass()) > rcOrder.at(rhs->riskClass()))
        return false;

    if (rtOrder.at(lhs->riskType()) < rtOrder.at(rhs->riskType()))
        return true;
    if (rtOrder.at(lhs->riskType()) > rtOrder.at(rhs->riskType()))
        return false;

    return false;
}

void VarRiskGroupContainer::reset() { rgIdx_ = riskGroups_.begin(); }

QuantLib::Size VarRiskGroupContainer::size() { return riskGroups_.size(); }

QuantLib::ext::shared_ptr<MarketRiskGroup> VarRiskGroupContainer::next() {
    if (rgIdx_ == riskGroups_.end())
        return nullptr;
    auto rg = *rgIdx_;
    ++rgIdx_;
    return rg;
}

void VarRiskGroupContainer::add(const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup) {
    auto rg = QuantLib::ext::dynamic_pointer_cast<VarRiskGroup>(riskGroup);
    QL_REQUIRE(rg, "riskGroup must be of type VarRiskGroup");
    riskGroups_.insert(rg);
}

void VarTradeGroupContainer::reset() { tgIdx_ = tradeGroups_.begin(); }

QuantLib::ext::shared_ptr<TradeGroup> VarTradeGroupContainer::next() {
    if (tgIdx_ == tradeGroups_.end())
        return nullptr;
    auto tg = *tgIdx_;
    ++tgIdx_;
    return tg;
}

void VarTradeGroupContainer::add(const QuantLib::ext::shared_ptr<TradeGroup>& tradeGroup) {
    auto tg = QuantLib::ext::dynamic_pointer_cast<VarTradeGroup>(tradeGroup);
    QL_REQUIRE(tg, "tradeGroup must be of type VarTradeGroup");
    tradeGroups_.insert(tg);
}

VarReport::VarReport(const std::string& baseCurrency, const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                     const std::string& portfolioFilter, const vector<Real>& p, boost::optional<ore::data::TimePeriod> period,
                     const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen,
                     std::unique_ptr<SensiRunArgs> sensiArgs, std::unique_ptr<FullRevalArgs> fullRevalArgs, const bool breakdown) 
    : MarketRiskReport(baseCurrency, period, hisScenGen, std::move(sensiArgs), std::move(fullRevalArgs), nullptr, breakdown), portfolio_(portfolio), 
      portfolioFilter_(portfolioFilter), p_(p) {
    init();

    riskGroups_ = QuantLib::ext::make_shared<VarRiskGroupContainer>();
    tradeGroups_ = QuantLib::ext::make_shared<VarTradeGroupContainer>();

    // build portfolio filter, if given
    bool hasFilter = false;
    boost::regex filter;
    if (portfolioFilter_ != "") {
        hasFilter = true;
        filter = boost::regex(portfolioFilter_);
        LOG("Portfolio filter: " << portfolioFilter_);
    }

    Size pos = 0;
    string allStr = "All";
    auto tradeGroup = QuantLib::ext::make_shared<VarTradeGroup>(allStr);
    tradeGroups_->add(tradeGroup);

    for (const auto& pId : portfolio_->portfolioIds()) {
        if (breakdown && (!hasFilter || boost::regex_match(pId, filter))) {
            auto tradeGroupP = QuantLib::ext::make_shared<VarTradeGroup>(pId);
            tradeGroups_->add(tradeGroupP);
        }
    }

    for (auto const& [tradeId, trade] : portfolio_->trades()) {
        if (!hasFilter && trade->portfolioIds().size() == 0)
            tradeIdGroups_[allStr].insert(make_pair(tradeId, pos));
        else {
            for (auto const& pId : trade->portfolioIds()) {
                if (!hasFilter || boost::regex_match(pId, filter)) {
                    tradeIdGroups_[allStr].insert(make_pair(tradeId, pos));
                    if (breakdown)
                        tradeIdGroups_[pId].insert(make_pair(tradeId, pos));
                }
            }
        }
        pos++;
    }

    // Create the Var risk groups, pairs of risk class/type
    for (auto rc : VarConfiguration::riskClasses(true)) {
        for (auto rt : VarConfiguration::riskTypes(true)) {
            auto riskGroup = QuantLib::ext::make_shared<VarRiskGroup>(rc, rt);
            riskGroups_->add(riskGroup);
        }        
    }
    riskGroups_->reset();
    tradeGroups_->reset();
}

void VarReport::createReports(const ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    QL_REQUIRE(reports->reports().size() == 1, "We should only report for VAR report");
    QuantLib::ext::shared_ptr<Report> report = reports->reports().at(0);
    // prepare report
    report->addColumn("Portfolio", string()).addColumn("RiskClass", string()).addColumn("RiskType", string());
    for (Size i = 0; i < p_.size(); ++i)
        report->addColumn("Quantile_" + std::to_string(p_[i]), double(), 6);

    createVarCalculator();
}

void VarReport::writeVarResults(const ext::shared_ptr<MarketRiskReport::Reports>& reports,
                                const ext::shared_ptr<MarketRiskGroup>& riskGroup,
                                const ext::shared_ptr<TradeGroup>& tradeGroup) {

    QL_REQUIRE(reports->reports().size() == 1, "We should only report for VAR report");
    QuantLib::ext::shared_ptr<Report> report = reports->reports().at(0);

    auto rg = ext::dynamic_pointer_cast<VarRiskGroup>(riskGroup);
    auto tg = ext::dynamic_pointer_cast<VarTradeGroup>(tradeGroup);

    std::vector<Real> var;
    auto quantiles = p();
    for (auto q : quantiles)
        var.push_back(varCalculator_->var(q));

    if (!close_enough(QuantExt::detail::absMax(var), 0.0)) {
        report->next();
        report->add(tg->portfolioId());
        report->add(to_string(rg->riskClass()));
        report->add(to_string(rg->riskType()));
        for (auto const& v : var)
            report->add(v);
    }
}

QuantLib::ext::shared_ptr<ore::analytics::ScenarioFilter> 
VarReport::createScenarioFilter(const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup) {
    auto rg = QuantLib::ext::dynamic_pointer_cast<VarRiskGroup>(riskGroup);
    QL_REQUIRE(rg, "riskGroup must be of type VarRiskGroup");
    return QuantLib::ext::make_shared<RiskFilter>(rg->riskClass(), rg->riskType());
}

string VarReport::portfolioId(const QuantLib::ext::shared_ptr<TradeGroup>& tradeGroup) const {
    auto vtg = QuantLib::ext::dynamic_pointer_cast<VarTradeGroup>(tradeGroup);
    QL_REQUIRE(vtg, "TradeGroup of type VarTradeGroup required");
    return vtg->portfolioId();
}

string VarReport::tradeGroupKey(const QuantLib::ext::shared_ptr<TradeGroup>& tradeGroup) const {
    return portfolioId(tradeGroup);
}

} // namespace analytics
} // namespace ore
