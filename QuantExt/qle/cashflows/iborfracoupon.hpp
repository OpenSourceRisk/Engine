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

/*! \file iborfracoupon.hpp
    \brief coupon representing an forward rate agreement

        \ingroup cashflows
*/

#pragma once

#include <ql/cashflows/iborcoupon.hpp>

namespace QuantExt {
//! %Coupon paying a Forward rate aggreement payoff with and ibor-type index underlying
class IborFraCoupon : public QuantLib::IborCoupon {
public:
    IborFraCoupon(const QuantLib::Date& startDate, const QuantLib::Date& endDate, QuantLib::Real nominal,
                  const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index, const double strikeRate);

    QuantLib::Real amount() const override;
};
} // namespace QuantExt