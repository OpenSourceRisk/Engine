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

#include <orea/engine/pnlexplainreport.hpp>

namespace ore {
namespace analytics {

void populateResults(QuantLib::ext::shared_ptr<MarketRiskGroup> mrg, PNLExplainReport::PNLExplainResults& result,
                     QuantLib::Real deltaPnl, QuantLib::Real gammaPnl, QuantLib::Real pnl) {
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::All) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.delta = deltaPnl;
            result.gamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.vega = pnl;
        else if (mrg->riskType() == MarketRiskConfiguration::RiskType::All)
            result.pnl = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::InterestRate) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.irDelta = deltaPnl;
            result.irGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.irVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::Equity) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.eqDelta = deltaPnl;
            result.eqGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.eqVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::FX) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.fxDelta = deltaPnl;
            result.fxGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.fxVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::Inflation) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.infDelta = deltaPnl;
            result.infGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.infVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::Credit) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.creditDelta = deltaPnl;
            result.creditGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.creditVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::Commodity) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            result.comDelta = deltaPnl;
            result.comGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            result.comVega = pnl;
    }
}

void PNLExplainReport::createReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    QL_REQUIRE(reports->reports().size() == 1, "We should only report for PNL Explain");
    QuantLib::ext::shared_ptr<Report> report = reports->reports().at(0);
    // prepare report
    report->addColumn("Portfolio", string())
        .addColumn("MTM - Asof Date", double(), 6)
        .addColumn("MTM - PNL Date", double(), 6)
        .addColumn("PNL", double(), 6)
        .addColumn("Delta", double(), 6)
        .addColumn("Gamma", double(), 6)
        .addColumn("Vega", double(), 6)
        .addColumn("IR Delta", double(), 6)
        .addColumn("IR Gamma", double(), 6)
        .addColumn("IR Vega", double(), 6)
        .addColumn("EQ Delta", double(), 6)
        .addColumn("EQ Gamma", double(), 6)
        .addColumn("EQ Vega", double(), 6)
        .addColumn("FX Delta", double(), 6)
        .addColumn("FX Gamma", double(), 6)
        .addColumn("FX Vega", double(), 6)
        .addColumn("INF Delta", double(), 6)
        .addColumn("INF Gamma", double(), 6)
        .addColumn("INF Vega", double(), 6)
        .addColumn("Credit Delta", double(), 6)
        .addColumn("Credit Gamma", double(), 6)
        .addColumn("Credit Vega", double(), 6)
        .addColumn("COM Delta", double(), 6)
        .addColumn("COM Gamma", double(), 6)
        .addColumn("COM Vega", double(), 6)
    ;
}

void PNLExplainReport::handleSensiResults(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& report,
    const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
    const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) {
}

void PNLExplainReport::addPnlCalculators(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    pnlCalculators_.push_back(QuantLib::ext::make_shared<PNLCalculator>(period_.get()));
}

void PNLExplainReport::writeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
    const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
    const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) {  

    std::vector<QuantLib::Real> sensiPnls = pnlCalculators_[0]->pnls();
    std::vector<QuantLib::Real> foSensiPnls = pnlCalculators_[0]->foPnls();
    TradePnLStore tradeSensiPnls = pnlCalculators_[0]->tradePnls();
    TradePnLStore foTradeSensiPnls = pnlCalculators_[0]->foTradePnls();

    QL_REQUIRE(sensiPnls.size() == 1, "PNLExplainReport::writeReports - should have exactly 1 sensi pnl");
    QL_REQUIRE(foSensiPnls.size() == 1, "PNLExplainReport::writeReports - should have exactly 1 fo sensi pnl");
    Real pnl = sensiPnls[0];
    Real deltaPnl = foSensiPnls[0];
    Real gammaPnl = pnl - deltaPnl;

    auto pId = portfolioId(tradeGroup);
    auto it = results_.find(pId);
    if (it == results_.end())
        results_[pId] = PNLExplainResults();

    auto mrg = ext::dynamic_pointer_cast<MarketRiskGroup>(riskGroup);
    QL_REQUIRE(mrg, "Require a group of type MarketRiskGroup");

    populateResults(mrg, results_[pId], deltaPnl, gammaPnl, pnl);

    if (runTradeDetail(reports)) {
        QL_REQUIRE(tradeSensiPnls.size() == 1,
                   "PNLExplainReport::writeReports - should have exactly 1 sensi pnl for each trade");
        QL_REQUIRE(foTradeSensiPnls.size() == 1,
                   "PNLExplainReport::writeReports - should have exactly 1 fo sensi pnl for each trade");
        QL_REQUIRE(tradeSensiPnls[0].size() == tradeIds_.size(),
                   "PNLExplainReport::writeReports - tradeSensiPNLs do not match portfolio size");
        QL_REQUIRE(foTradeSensiPnls[0].size() == tradeIds_.size(),
                   "PNLExplainReport::writeReports - foTradeSensiPNLs do not match portfolio size");

        for (QuantLib::Size i = 0; i < tradeIds_.size(); i++) {  
            auto tId = tradeIds_.at(i);
            Real tpnl = tradeSensiPnls[0].at(i);
            Real tdeltaPnl = foTradeSensiPnls[0].at(i);
            Real tgammaPnl = tpnl - tdeltaPnl;
            auto itt = results_.find(tId);
            if (itt == results_.end())
                results_[tId] = PNLExplainResults();
            
            populateResults(mrg, results_[tId], tdeltaPnl, tgammaPnl, tpnl);
        }
    }
}

bool PNLExplainReport::includeDeltaMargin(
    const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup) const {
    return true;
}
bool PNLExplainReport::includeGammaMargin(
    const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup) const {
    return true;
}

void PNLExplainReport::closeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    QL_REQUIRE(reports->reports().size() == 1, "We should only report for PNL Explain");
    QuantLib::ext::shared_ptr<Report> report = reports->reports().at(0);
    for (const auto& r : results_) {
        report->next().add(r.first)
            .add(0.0)
            .add(0.0)
            .add(r.second.pnl)
            .add(r.second.delta)
            .add(r.second.gamma)
            .add(r.second.vega)
            .add(r.second.irDelta)
            .add(r.second.irGamma)
            .add(r.second.irVega)
            .add(r.second.eqDelta)
            .add(r.second.eqGamma)
            .add(r.second.eqVega)
            .add(r.second.fxDelta)
            .add(r.second.fxGamma)
            .add(r.second.fxVega)
            .add(r.second.infDelta)
            .add(r.second.infGamma)
            .add(r.second.infVega)
            .add(r.second.creditDelta)
            .add(r.second.creditGamma)
            .add(r.second.creditVega)
            .add(r.second.comDelta)
            .add(r.second.comGamma)
            .add(r.second.comVega);    
    }

    MarketRiskReport::closeReports(reports);
}

} // namespace analytics
} // namespace ore