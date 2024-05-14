/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file qle/utilities/commodity.hpp
    \brief some commodity related utilities.
*/

#pragma once

#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/time/futureexpirycalculator.hpp>

namespace QuantExt {

//! Make a commodity indexed cashflow
QuantLib::ext::shared_ptr<CashFlow>
makeCommodityCashflowForBasisFuture(const QuantLib::Date& start, const QuantLib::Date& end,
                                    const QuantLib::ext::shared_ptr<CommodityIndex>& baseIndex,
                                    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& baseFec, bool baseIsAveraging,
                                    const QuantLib::Date& paymentDate = QuantLib::Date());
} // namespace Utilities