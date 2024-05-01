/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file cashflowresults.hpp
    \brief class holding cashflow-related results

    \ingroup instruments
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/currency.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>
#include <ql/utilities/null.hpp>

namespace QuantExt {

struct CashFlowResults {
    QuantLib::Real amount = QuantLib::Null<QuantLib::Real>();
    QuantLib::Date payDate;
    std::string currency;
    QuantLib::Size legNumber = 0;
    std::string type = "Unspecified";
    QuantLib::Real rate = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real accrualPeriod = QuantLib::Null<QuantLib::Real>();
    QuantLib::Date accrualStartDate;
    QuantLib::Date accrualEndDate;
    QuantLib::Real accruedAmount = QuantLib::Null<QuantLib::Real>();
    QuantLib::Date fixingDate;
    QuantLib::Real fixingValue = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real notional = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real discountFactor = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real presentValue = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real presentValueBase = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real fxRateLocalBase = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real floorStrike = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real capStrike = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real floorVolatility = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real capVolatility = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real effectiveFloorVolatility = QuantLib::Null<QuantLib::Real>();
    QuantLib::Real effectiveCapVolatility = QuantLib::Null<QuantLib::Real>();
};

std::ostream& operator<<(std::ostream& out, const CashFlowResults& t);

CashFlowResults standardCashFlowResults(const QuantLib::ext::shared_ptr<QuantLib::CashFlow>& c,
                                        const QuantLib::Real multiplier = 1.0, const std::string& type = "Unspecified",
                                        const QuantLib::Size legNo = 0,
                                        const QuantLib::Currency& currency = QuantLib::Currency(),
                                        const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve =
                                            QuantLib::Handle<QuantLib::YieldTermStructure>());

CashFlowResults populateCashFlowResultsFromCashflow(const QuantLib::ext::shared_ptr<QuantLib::CashFlow>& c,
                                                    const QuantLib::Real multiplier = 1.0,
                                                    const QuantLib::Size legNo = 0,
                                                    const QuantLib::Currency& currency = QuantLib::Currency());

} // namespace QuantExt
