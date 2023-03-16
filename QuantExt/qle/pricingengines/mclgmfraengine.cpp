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

#include <qle/pricingengines/mclgmfraengine.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>

namespace QuantExt {

void McLgmFraEngine::calculate() const {

    ext::shared_ptr<IborIndex> index = arguments_.index;

    Date payDate = arguments_.valueDate;

    Real notional = arguments_.notionalAmount;
    Real fixedRate = arguments_.strikeForwardRate.rate();
    Date accrualStartDate = arguments_.valueDate;
    Date accrualEndDate = arguments_.maturityDate;
    Position::Type type = arguments_.type;

    Leg leg{boost::make_shared<IborCoupon>(payDate, notional, accrualStartDate, accrualEndDate,
                                           index->fixingDays(), index, 1.0, -fixedRate)};

    leg_ = {leg};
    currency_ = std::vector<Currency>(leg_.size(), model_->irlgm1f(0)->currency());
    if (type == Position::Long)
        payer_ = {1};
    else
        payer_ = {-1};
    exercise_ = nullptr;
    McMultiLegBaseEngine::calculate();
    results_.value = resultValue_;
    results_.additionalResults["amcCalculator"] = amcCalculator();
} // McLgmFraEngine::calculate

} // namespace QuantExt
