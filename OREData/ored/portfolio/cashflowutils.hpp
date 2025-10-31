/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/trade.hpp
    \brief base trade data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ql/any.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>
#include <ql/handle.hpp>

namespace QuantLib {
class CashFlow;
class YieldTermStructure;
class SwaptionVolatilityStructure;
class OptionletVolatilityStructure;
} // namespace QuantLib

namespace ore {
namespace data {

class Market;

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

// Populate TradeCashflowReportData based on CashFlow. Note: cashfowNo and legNo will _not_ be populated.
TradeCashflowReportData getCashflowReportData(
    QuantLib::ext::shared_ptr<QuantLib::CashFlow> ptrFlow, const bool payer, const double multiplier,
    const std::string& baseCcy, const std::string ccy, const Date asof,
    const QuantLib::ext::shared_ptr<QuantLib::YieldTermStructure>& discountCurveCcy, const double fxCcyBase,
    const std::function<QuantLib::ext::shared_ptr<QuantLib::SwaptionVolatilityStructure>(const std::string& qualifier)>&
        swaptionVol,
    const std::function<
        QuantLib::ext::shared_ptr<QuantLib::OptionletVolatilityStructure>(const std::string& qualifier)>& optionletVol);

// Populate vector<TradeCashflowReportData> based on additional results map
void populateReportDataFromAdditionalResults(
    std::vector<TradeCashflowReportData>& result, std::map<Size, Size>& cashflowNumber,
    const std::map<std::string, QuantLib::ext::any>& addResults, const Real multiplier, const std::string& baseCurrency,
    const std::string& npvCurrency, QuantLib::ext::shared_ptr<ore::data::Market> market,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& specificDiscountCurve, const std::string& configuration,
    const bool includePastCashflows);

} // namespace data
} // namespace ore
