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

#include <orea/engine/marketriskreport.hpp>

namespace ore {
namespace analytics {

class PnlExplainReport : public MarketRiskReport {
public:

    struct PnlExplainResults {
        QuantLib::Real pnl = 0.0;
        QuantLib::Real delta = 0.0;
        QuantLib::Real gamma = 0.0;
        QuantLib::Real vega = 0.0;
        QuantLib::Real irDelta = 0.0;
        QuantLib::Real irGamma = 0.0;
        QuantLib::Real irVega = 0.0;
        QuantLib::Real eqDelta = 0.0;
        QuantLib::Real eqGamma = 0.0;
        QuantLib::Real eqVega = 0.0;
        QuantLib::Real fxDelta = 0.0;
        QuantLib::Real fxGamma = 0.0;
        QuantLib::Real fxVega = 0.0;
        QuantLib::Real infDelta = 0.0;
        QuantLib::Real infGamma = 0.0;
        QuantLib::Real infVega = 0.0;
        QuantLib::Real creditDelta = 0.0;
        QuantLib::Real creditGamma = 0.0;
        QuantLib::Real creditVega = 0.0;
        QuantLib::Real comDelta = 0.0;
        QuantLib::Real comGamma = 0.0;
        QuantLib::Real comVega = 0.0;
    };

    PnlExplainReport(const std::string& baseCurrency, const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                     const std::string& portfolioFilter, boost::optional<ore::data::TimePeriod> period,
                     const QuantLib::ext::shared_ptr<Report>& pnlReport = nullptr,
                     const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen = nullptr,
                     std::unique_ptr<SensiRunArgs> sensiArgs = nullptr,
                     std::unique_ptr<FullRevalArgs> fullRevalArgs = nullptr,
                     std::unique_ptr<MultiThreadArgs> multiThreadArgs = nullptr,
                     const bool requireTradePnl = false)
        : MarketRiskReport(baseCurrency, portfolio, portfolioFilter, period, hisScenGen, std::move(sensiArgs), std::move(fullRevalArgs), 
            std::move(multiThreadArgs), true, requireTradePnl), pnlReport_(pnlReport) {
        sensiBased_ = true;
    }

protected:
    void createReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) override;
    void handleSensiResults(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& report,
                                    const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                                    const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) override;
    void addPnlCalculators(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) override;
    void writeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
                        const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                        const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) override;
    bool
    includeDeltaMargin(const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup) const override;
    bool
    includeGammaMargin(const QuantLib::ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup) const override;
    void closeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) override;

private:
    std::map<std::string, PnlExplainResults> results_;
    QuantLib::ext::shared_ptr<Report> pnlReport_;
    QuantLib::Size pnlReportColumnSize_;
};

} // namespace analytics
} // namespace ore