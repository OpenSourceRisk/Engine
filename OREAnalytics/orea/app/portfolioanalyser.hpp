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

#include <orea/engine/dependencymarket.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/portfolio/enginedata.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/report/report.hpp>

/*! \file orea/app/portfolioanalyser.hpp
    \brief Class for examining a portfolio and returning information about its risk factors
*/

#pragma once

namespace ore {
namespace analytics {

class PortfolioAnalyser {
public:
    //! Constructor
    PortfolioAnalyser(
        const QuantLib::ext::shared_ptr<ore::data::Portfolio>& p,
        const QuantLib::ext::shared_ptr<ore::data::EngineData>& ed, const std::string& baseCcy,
        const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs = nullptr,
        const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData = nullptr,
        const ore::data::IborFallbackConfig& iborFallbackConfig = ore::data::IborFallbackConfig::defaultConfig(),
        const bool recordSecuritySpecificCreditCurves = false, const bool recordRequiredFixings = true);

    //! Check if the portfolio has risk factors of a given type
    bool hasRiskFactorType(const RiskFactorKey::KeyType& riskFactorType) const {
        return market_->hasRiskFactorType(riskFactorType);
    }

    //! Check if the portfolio has market objects of a given type
    bool hasMarketObjectType(const ore::data::MarketObject& marketObject) const {
        return market_->hasMarketObjectType(marketObject);
    }

    //! Return the risk factor names of the given risk factor type in the portfolio
    std::set<std::string> riskFactorNames(const RiskFactorKey::KeyType& riskFactorType) const {
        return market_->riskFactorNames(riskFactorType);
    }

    //! Return all of the risk factor types in the portfolio
    std::set<RiskFactorKey::KeyType> riskFactorTypes() const { return market_->riskFactorTypes(); }

    //! Return all of the market objects needed by the portfolio
    std::map<ore::data::MarketObject, std::set<std::string>>
    marketObjects(const boost::optional<std::string> config = boost::none) const {
        return market_->marketObjects(config);
    }

    //! Return the names of swap indices needed by the portfolio
    std::set<std::string> swapindices() const { return market_->swapindices(); }

    //! Populate a report with the type and name of each risk factor in the portfolio
    //! Report has headers 'RiskFactorType', 'RiskFactorName'
    void riskFactorReport(ore::data::Report& reportOut) const;

    //! Populate a report with the type and name of each market object in the portfolio
    //! Report has headers 'MarketObjectType', 'MarketObjectName'
    void marketObjectReport(ore::data::Report& reportOut) const;
    
    //! Return a set of all counterparties 
    std::set<std::string> counterparties() { return counterparties_; }

    //! Return portfolio maturity date
    QuantLib::Date maturity() { return maturity_; }

    //! pointer to portfolio
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio() const { return portfolio_; }

    //! return underlying indices of portfolio
    std::map<ore::data::AssetClass, std::set<std::string>> underlyingIndices() const { return underlyingIndices_; }

private:
    QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio_;
    QuantLib::ext::shared_ptr<DependencyMarket> market_;
    std::set<std::string> counterparties_;
    QuantLib::Date maturity_;
    std::map<ore::data::AssetClass, std::set<std::string>> underlyingIndices_;
};

} // namespace analytics
} // namespace ore
