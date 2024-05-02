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

void PnlExplainReport::createReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    QL_REQUIRE(reports->reports().size() == 1, "We should only report for PNL Explain");
    QuantLib::ext::shared_ptr<Report> report = reports->reports().at(0);
    // prepare report
    report->addColumn("Portfolio", string())
           .addColumn("MTM - Asof Date", double())
           .addColumn("MTM - PNL Date", double())
           .addColumn("PNL", double())
           .addColumn("IR Delta", double())
           .addColumn("IR Gamma", double())
           .addColumn("IR Vega", double())
           .addColumn("EQ Delta", double())
           .addColumn("EQ Gamma", double())
           .addColumn("EQ Vega", double())
           .addColumn("FX Delta", double())
           .addColumn("FX Gamma", double())
           .addColumn("FX Vega", double())
           .addColumn("INF Delta", double())
           .addColumn("INF Gamma", double())
           .addColumn("INF Vega", double())
           .addColumn("Credit Delta", double())
           .addColumn("Credit Gamma", double())
           .addColumn("Credit Vega", double())
           .addColumn("COM Delta", double())
           .addColumn("COM Gamma", double())
           .addColumn("COM Vega", double())
    ;
}

void PnlExplainReport::handleSensiResults(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& report,
    const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
    const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) {

}

void PnlExplainReport::addPnlCalculators(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    pnlCalculators_.push_back(QuantLib::ext::make_shared<PNLCalculator>(period_.get()));
}

void PnlExplainReport::writeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
    const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
    const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) {  

    std::vector<QuantLib::Real> sensiPnls = pnlCalculators_[0]->pnls();
    std::vector<QuantLib::Real> foSensiPnls = pnlCalculators_[0]->foPnls();
    QL_REQUIRE(sensiPnls.size() == 1, "PnlExplainReport::writeReports - should have exactly 1 sensi pnl");
    Real pnl = sensiPnls[0];
    Real deltaPnl = foSensiPnls[0];
    Real gammaPnl = pnl - deltaPnl;

    auto pId = portfolioId(tradeGroup);
    auto it = results_.find(pId);
    if (it == results_.end())
        results_[pId] = PnlExplainResults();

    auto mrg = ext::dynamic_pointer_cast<MarketRiskGroup>(riskGroup);
    QL_REQUIRE(mrg, "Require a group of type MarketRiskGroup");

    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::InterestRate) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            results_[pId].irDelta = deltaPnl;
            results_[pId].irGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            results_[pId].irVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::Equity) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            results_[pId].eqDelta = deltaPnl;
            results_[pId].eqGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            results_[pId].eqVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::FX) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            results_[pId].fxDelta = deltaPnl;
            results_[pId].fxGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            results_[pId].fxVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::Inflation) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            results_[pId].infDelta = deltaPnl;
            results_[pId].infGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            results_[pId].infVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::Credit) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            results_[pId].creditDelta = deltaPnl;
            results_[pId].creditGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            results_[pId].creditVega = pnl;
    }
    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::Commodity) {
        if (mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
            results_[pId].comDelta = deltaPnl;
            results_[pId].comGamma = gammaPnl;
        } else if (mrg->riskType() == MarketRiskConfiguration::RiskType::Vega)
            results_[pId].comVega = pnl;
    }
}

bool PnlExplainReport::includeDeltaMargin(
    const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup) const {
    return true;
}
bool PnlExplainReport::includeGammaMargin(
    const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup) const {
    return true;
}

void PnlExplainReport::closeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    QL_REQUIRE(reports->reports().size() == 1, "We should only report for PNL Explain");
    QuantLib::ext::shared_ptr<Report> report = reports->reports().at(0);
    for (const auto& r : results_) {
        report->next().add(r.first)
            .add(0.0)
            .add(0.0)
            .add(0.0)
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