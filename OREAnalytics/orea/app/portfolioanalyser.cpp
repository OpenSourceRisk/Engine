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

#include <orea/app/portfolioanalyser.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/utilities/dependencies.hpp>
#include <ored/utilities/to_string.hpp>

using namespace ore::data;
using std::vector;

namespace ore {
namespace analytics {

PortfolioAnalyser::PortfolioAnalyser(const QuantLib::ext::shared_ptr<Portfolio>& p, const QuantLib::ext::shared_ptr<EngineData>& ed,
                                     const string& baseCcy,
                                     const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs,
                                     const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                                     const IborFallbackConfig& iborFallbackConfig,
                                     bool recordSecuritySpecificCreditCurves, const std::string& baseCcyDiscountCurve)
    : portfolio_(p) {

    QL_REQUIRE(portfolio_ != nullptr, "PortfolioAnalyser: portfolio is null");

    underlyingIndices_ = portfolio_->underlyingIndices();

    // Build Dependency Market
    market_ = QuantLib::ext::make_shared<DependencyMarket>(baseCcy, true, curveConfigs, iborFallbackConfig,
                                                   recordSecuritySpecificCreditCurves);

    // Build EngineFactory
    // We use a copy of engine data that has the global parameter "Calibrate" set to "False". We do this in an
    // attempt to avoid EngineBuilders doing calibrations on DependencyMarket which can lead to failures.
    QuantLib::ext::shared_ptr<EngineData> edCopy = QuantLib::ext::make_shared<EngineData>(*ed);
    edCopy->globalParameters()["Calibrate"] = "false";
    edCopy->globalParameters()["RunType"] = "PortfolioAnalyser";
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(
        edCopy, market_, std::map<MarketContext, string>(), referenceData, iborFallbackConfig);

    // Build Portfolio
    p->build(factory, "portfolio-analyzer");
    maturity_ = p->maturity();

    // Build bonds having a security entry to get additional deps to the curves needed by the bond
    for (auto const& securityId : market_->marketObjectNames(ore::data::MarketObject::Security)) {
        try {
	    ore::data::BondFactory::instance().build(factory, referenceData, securityId);
        } catch (const std::exception& e) {
            WLOG("PortfolioAnalyser: error during build of bond '"
                 << securityId << "', we might miss out dependencies (" << e.what() << ").");
        }
    }

    // Get a list of counterparties 
    for (const auto& [tid,t] : p->trades())
        counterparties_.insert(t->envelope().counterparty());

    // Add some additional quotes:
    vector<string> ccyFields{"present_value_currency", "notional_currency"};
    bool baseUsdAdded = false;
    for (const auto& [tradeId, t] : p->trades()) {

        // add any missed npv ccys
        market_->fxRate(t->npvCurrency() + baseCcy, Market::defaultConfiguration);

        // CCY/USD FX quotes that are required for IM Schedule calculations driven by envelope additional fields.
        const map<string, string>& addFields = t->envelope().additionalFields();
        auto it = addFields.find("im_model");
        if (it != addFields.end() && it->second == "Schedule") {
            for (const string& ccyField : ccyFields) {
                auto itCcyField = addFields.find(ccyField);
                if (itCcyField != addFields.end() && itCcyField->second != "USD") {
                    DLOG("Add CCY/USD Quote for additional field " << ccyField << " and value " << itCcyField->second);
                    market_->fxRate(itCcyField->second + "USD", Market::defaultConfiguration);
                }
            }
        }

        // USD/base when base != USD for trades with type "UseCounterparty"
        if (!baseUsdAdded && t->tradeType() == "UseCounterparty" && baseCcy != "USD") {
            market_->fxRate("USD" + baseCcy, Market::defaultConfiguration);
            baseUsdAdded = true;
        }
    }

    // add any curve dependencies from the marketobjects obtained
    marketObjects_ = market_->marketObjects();
    DLOG("Start adding dependent curves");
    ore::data::addMarketObjectDependencies(&marketObjects_, curveConfigs, baseCcy, baseCcyDiscountCurve);
    DLOG("Finished adding dependent curves");
}

std::map<ore::data::MarketObject, std::set<std::string>>
PortfolioAnalyser::marketObjects(const boost::optional<std::string> config) const {
    if (config)
        return marketObjects_[*config];
    std::map<ore::data::MarketObject, std::set<std::string>> result;
    for (auto const& m : marketObjects_) {
        for (auto const& e : m.second) {
            result[e.first].insert(e.second.begin(), e.second.end());
        }
    }
    return result;
}

void PortfolioAnalyser::riskFactorReport(Report& report) const {
    // Add report headers
    report.addColumn("RiskFactorType", string()).addColumn("RiskFactorName", string());

    // Build report
    for (const ore::analytics::RiskFactorKey::KeyType& type : market_->riskFactorTypes()) {
        string strType = to_string(type);
        string names;
        for (const string& name : market_->riskFactorNames(type)) {
            if (names.empty())
                names += name;
            else
                names += "|" + name;
        }
        report.next().add(strType).add(names);
    }
}

void PortfolioAnalyser::marketObjectReport(Report& report) const {
    // Add report headers
    report.addColumn("MarketObjectType", string())
        .addColumn("MarketObjectName", string());

    // Build report
    for (const auto& type : market_->marketObjectTypes()) {
        string strType = to_string(type);
        string names;
        for (const string& name : market_->marketObjectNames(type)) {
            if (names.empty())
                names += name;
            else
                names += "|" + name;
        }            
        report.next().add(strType).add(names);
    }
}

} // namespace analytics
} // namespace ore
