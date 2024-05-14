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
#include <orea/engine/marketriskreport.hpp>

namespace ore {
namespace analytics {

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
    
    virtual void createVarCalculator() = 0;
    void writeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& report,
                         const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                         const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) override;

    std::vector<ore::data::TimePeriod> timePeriods() override { return {period_.get()}; }

private:
    std::vector<Real> p_;
};

} // namespace analytics
} // namespace ore
