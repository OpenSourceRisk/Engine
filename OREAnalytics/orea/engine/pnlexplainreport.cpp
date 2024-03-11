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

void PNLExplainReport::createReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    QL_REQUIRE(reports->reports().size() == 1, "We should only report for PNL Explain");
    QuantLib::ext::shared_ptr<Report> report = reports->reports().at(0);
    // prepare report
    report->addColumn("MTM - Asof Date", string())
           .addColumn("MTM - PNL Date", string())
           .addColumn("PNL", string())
           .addColumn("Portfolio", string())
           .addColumn("IR Delta", double());
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

    QL_REQUIRE(sensiPnls_.size() == 1, "PNLExplainReport::writeReports - should have exactly 1 sensi pnl");
    Real pnl = sensiPnls_[0];

    auto pId = portfolioId(tradeGroup);
    auto it = results_.find(pId);
    if (it == results_.end())
        results_[pId] = PNLExplainResults();

    auto mrg = ext::dynamic_pointer_cast<MarketRiskGroup>(riskGroup);
    QL_REQUIRE(mrg, "Require a group of type MarketRiskGroup");

    if (mrg->riskClass() == MarketRiskConfiguration::RiskClass::InterestRate &&
        mrg->riskType() == MarketRiskConfiguration::RiskType::DeltaGamma) {
        if (!includeGammaMargin_)
            results_[pId].irDelta = pnl;
    
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

void PNLExplainReport::closeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) override {

    MarketRiskReport::closeReports(reports);
}

} // namespace analytics
} // namespace ore