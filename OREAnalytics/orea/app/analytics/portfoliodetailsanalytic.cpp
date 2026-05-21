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

#include <orea/app/analytics/portfoliodetailsanalytic.hpp>
#include <orea/app/inputparameters.hpp>
#include <ored/report/inmemoryreport.hpp>

namespace ore {
namespace analytics {

typedef std::map<ore::data::MarketObject, std::set<std::string>> MarketObjectSets;
typedef std::map<ore::analytics::RiskFactorKey::KeyType, std::set<std::string>> RiskFactorSets;

void PortfolioDetailsVariables::loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) { }

void PortfolioDetailsAnalyticImpl::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
		const std::set<std::string>& runTypes) {

    auto effectivePortfolio = analytic()->portfolio() == nullptr ? inputs_->portfolio() : analytic()->portfolio();

    if (!portfolioAnalyser_) {
        portfolioAnalyser_ = QuantLib::ext::make_shared<PortfolioAnalyser>(
            effectivePortfolio,
            inputs_->pricingEngine(), inputs_->baseCurrency(), analytic()->configurations().curveConfig,
            inputs_->refDataManager(), inputs_->iborFallbackConfig());
    }

    // risk factor report
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> rfReport =
        QuantLib::ext::make_shared<ore::data::InMemoryReport>();
    portfolioAnalyser_->riskFactorReport(*rfReport);
    analytic()->addReport(label_, "risk_factors", rfReport);

    // market object report
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> moReport =
        QuantLib::ext::make_shared<ore::data::InMemoryReport>();
    portfolioAnalyser_->marketObjectReport(*moReport);
    analytic()->addReport(label_, "market_objects", moReport);

    // swap indices report
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> siReport =
        QuantLib::ext::make_shared<ore::data::InMemoryReport>();
    siReport->addColumn("SwapIndices", string());
    for (const auto& si : portfolioAnalyser_->swapindices())
        siReport->next().add(si);
    siReport->end();
    analytic()->addReport(label_, "swap_indices", siReport);

    // Count of cps, netting sets and trade types
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> cpReport = QuantLib::ext::make_shared<ore::data::InMemoryReport>();
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> nsReport = QuantLib::ext::make_shared<ore::data::InMemoryReport>();
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> ttReport = QuantLib::ext::make_shared<ore::data::InMemoryReport>();
    cpReport->addColumn("Counterparty", string()).addColumn("Count", Size());
    nsReport->addColumn("NettingSets", string()).addColumn("Count", Size());
    ttReport->addColumn("TradeTypes", string()).addColumn("Count", Size());
    map<string, Size> counterParties, nettingSets, tradeTypes;
    for (const auto& [_, t] : effectivePortfolio->trades()) {
        counterParties[t->envelope().counterparty()]++;
        nettingSets[t->envelope().nettingSetId()]++;
        tradeTypes[t->tradeType()]++;
    }
    for (auto const& cp : counterParties)
        cpReport->next().add(cp.first).add(cp.second);
    for (auto const& ns : nettingSets)
        nsReport->next().add(ns.first).add(ns.second);
    for (auto const& tt : tradeTypes)
        ttReport->next().add(tt.first).add(tt.second);
    cpReport->end();
    nsReport->end();
    ttReport->end();
    analytic()->addReport(label_, "counterparties", cpReport);
    analytic()->addReport(label_, "netting_sets", nsReport);
    analytic()->addReport(label_, "trade_types", ttReport);

    // underlying indices report
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> uiReport =
        QuantLib::ext::make_shared<ore::data::InMemoryReport>();
    uiReport->addColumn("AssetType", string()).addColumn("Indices", string());
    for (const auto& ui : portfolioAnalyser_->underlyingIndices()) {
        string indices;
        for (const auto& s : ui.second)
            indices += indices.empty() ? s : "|" + s;
        uiReport->next().add(to_string(ui.first)).add(indices);
    }
    uiReport->end();
    analytic()->addReport(label_, "underlying_indices", uiReport);

    // trade-by-trade market object and risk factor reports
    // run a portfolio_analyser for each trade in the portfolio to generate a trade-by-trade report.
    std::map<string, MarketObjectSets> tradeMarketObjects;
    std::map<string, RiskFactorSets> tradeRiskFactors;
    for (const auto& t : effectivePortfolio->trades()) {
        auto singleTradePortfolio = QuantLib::ext::make_shared<ore::data::Portfolio>();
        singleTradePortfolio->add(t.second);
        auto tradeAnalyser = QuantLib::ext::make_shared<PortfolioAnalyser>(
            singleTradePortfolio, inputs_->pricingEngine(), inputs_->baseCurrency(),
            analytic()->configurations().curveConfig, inputs_->refDataManager(), inputs_->iborFallbackConfig());
        tradeMarketObjects[t.first] = tradeAnalyser->marketObjects();
        tradeRiskFactors[t.first] = tradeAnalyser->riskFactors();
    }

    // now construct the market object report (should be moved to portfolio analyser)
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> tmoReport =
        QuantLib::ext::make_shared<ore::data::InMemoryReport>();

    (*tmoReport)
        .addColumn("TradeId", string())
        .addColumn("MarketObjectType", string())
        .addColumn("MarketObjectName", string());

    for (const auto& [tradeId, marketObjects] : tradeMarketObjects) {
        for (const auto& [marketObject, names] : marketObjects) {
            for (const string& name : names) {
                (*tmoReport).next().add(tradeId).add(to_string(marketObject)).add(name);
            }
        }
    }

    analytic()->addReport(label_, "tradewise_market_objects", tmoReport);

    // now construct the risk factor report (should be moved to portfolio analyser)
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> trfReport =
        QuantLib::ext::make_shared<ore::data::InMemoryReport>();
    (*trfReport)
        .addColumn("TradeId", string())
        .addColumn("RiskFactorType", string())
        .addColumn("RiskFactorName", string());

    // Build report
    for (const auto& [tradeId, riskFactors] : tradeRiskFactors) {
        for (const auto& [riskFactor, names] : riskFactors) {
            for (const string& name : names) {
                (*trfReport).next().add(tradeId).add(to_string(riskFactor)).add(name);
            }
        }
    }

    analytic()->addReport(label_, "tradewise_risk_factors", trfReport);
    // end tradewise reports.
}


} // namespace analytics
} // namespace ore
