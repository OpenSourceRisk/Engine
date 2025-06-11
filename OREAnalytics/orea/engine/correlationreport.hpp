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

#include <orea/engine/marketriskreport.hpp>
#include <ored/report/report.hpp>

namespace ore {
namespace analytics {


class CorrelationReport : public MarketRiskReport {
public:
    CorrelationReport(const std::string& correlationMethod, const std::string& baseCurrency,
                      const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const std::string& portfolioFilter,
                      boost::optional<ore::data::TimePeriod> period,
                      const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen = nullptr,
                      std::unique_ptr<SensiRunArgs> sensiArgs = nullptr,
                      std::unique_ptr<FullRevalArgs> fullRevalArgs = nullptr,
                      std::unique_ptr<MultiThreadArgs> multiThreadArgs = nullptr, const bool requireTradePnl = false)
        : MarketRiskReport(baseCurrency, portfolio, portfolioFilter, period, hisScenGen, std::move(sensiArgs),
                           std::move(fullRevalArgs), false, false) {
        sensiBased_ = true;
        correlation_ = true;
        correlationMethod_ = correlationMethod;
    }
    const QuantLib::Matrix& getCorrelation() const { return m_; }
    void setCorrelation(const QuantLib::Matrix& m) { m_ = m; }

protected:
    void createReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports);
    void writeHeader(const QuantLib::ext::shared_ptr<Report>& report);
    void writeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& report,
                      const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                      const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) override;

    virtual void writeAdditionalReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
                                        const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                                        const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup){};

private:
    QuantLib::Matrix m_;
};

} // namespace analytics
} // namespace ore
