/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file orea/app/reportwriters/pricingreportwriter.hpp
  \brief A Class to write ORE Pricing outputs to reports
  \ingroup app
 */

#pragma once

#include <orea/app/reportwriter.hpp>
#include <orea/scenario/scenario.hpp>

namespace ore {
namespace data {
class Report;
class DateGrid;
class Market;
class Portfolio;
class InMemoryReport;
class TodaysMarketParameters;
struct TradeCashflowReportData;
} // namespace data
} // namespace ore

namespace ore {
namespace analytics {

class SensitivityStream;

//! Write ORE Pricing outputs to reports
/*! \ingroup app
 */
class PricingReportWriter : public ReportWriter {
public:
    /*! Constructor.
        \param nullString used to represent string values that are not applicable.
    */
    PricingReportWriter(const std::string& nullString = "#NA") : ReportWriter(nullString) {}

    virtual void writeNpv(ore::data::Report& report, const std::string& baseCurrency,
                          QuantLib::ext::shared_ptr<ore::data::Market> market, const std::string& configuration,
                          QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio);

    virtual void writeCashflow(ore::data::Report& report, const std::string& baseCurrency,
                               QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio,
                               QuantLib::ext::shared_ptr<ore::data::Market> market, const std::string& configuration,
                               const bool includePastCashflows = false);

    virtual void
    writeCashflow(ore::data::Report& report, QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio,
                  const std::map<std::string, std::vector<ore::data::TradeCashflowReportData>>& tradeCashflows);

    virtual void writeCashflowNpv(ore::data::Report& report, const ore::data::InMemoryReport& cashflowReport,
                            QuantLib::ext::shared_ptr<ore::data::Market> market, const std::string& configuration,
                            const std::string& baseCcy, const QuantLib::Date& horizon = QuantLib::Date::maxDate());

    virtual void writeCurves(ore::data::Report& report, const std::string& configID, const ore::data::DateGrid& grid,
                            const ore::data::TodaysMarketParameters& marketConfig,
                            const QuantLib::ext::shared_ptr<ore::data::Market>& market, const bool continueOnError = false);

    virtual void writeSensitivityReport(ore::data::Report& report,
                            const QuantLib::ext::shared_ptr<SensitivityStream>& ss,
                            QuantLib::Real outputThreshold = 0.0,
                            const QuantLib::ext::shared_ptr<ore::data::Market>& market = nullptr,
                            const std::string& configuration = ore::data::Market::defaultConfiguration,
                            QuantLib::Size outputPrecision = 2);

    virtual void writeSensitivityConfigReport(ore::data::Report& report,
                            const std::map<RiskFactorKey, QuantLib::Real>& shiftSizes,
                            const std::map<RiskFactorKey, QuantLib::Real>& baseValues,
                            const std::map<RiskFactorKey, std::string>& keyToFactor);
};

} // namespace analytics
} // namespace ore
