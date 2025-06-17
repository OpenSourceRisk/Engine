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

/*! \file orea/engine/cashflowreportgenerator.hpp
    \brief generates cashflow data for reporting based on a given xtrade
    \ingroup simulation
*/

#pragma once

#include <ored/portfolio/trade.hpp>

#include <ql/types.hpp>

namespace ore {
namespace analytics {

struct TradeCashflowReportData {
    QuantLib::Size cashflowNo;
    QuantLib::Size legNo;
    QuantLib::Date payDate;
    std::string flowType;
    double amount;
    std::string currency;
    double coupon;
    double accrual;
    QuantLib::Date accrualStartDate;
    QuantLib::Date accrualEndDate;
    double accruedAmount;
    QuantLib::Date fixingDate;
    double fixingValue;
    double notional;
    double discountFactor;
    double presentValue;
    double fxRateLocalBase;
    double presentValueBase;
    std::string baseCurrency;
    double floorStrike;
    double capStrike;
    double floorVolatility;
    double capVolatility;
    double effectiveFloorVolatility;
    double effectiveCapVolatility;
};

std::vector<TradeCashflowReportData> generateCashflowReportData(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                                                                const std::string& baseCurrency,
                                                                QuantLib::ext::shared_ptr<ore::data::Market> market,
                                                                const std::string& configuration,
                                                                const bool includePastCashflows);

} // namespace analytics
} // namespace ore
