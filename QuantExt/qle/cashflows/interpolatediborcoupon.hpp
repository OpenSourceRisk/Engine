/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file interpolatediborcoupon.hpp
    \brief Coupon paying an interpolated ibor fixing
    \ingroup cashflows
*/

#pragma once

#include <qle/indexes/interpolatediborindex.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/iborindex.hpp>

namespace QuantExt {

class InterpolatedIborCoupon : public QuantLib::FloatingRateCoupon {
public:
    InterpolatedIborCoupon(const Date& paymentDate, const Real nominal, const Date& accrualStart,
                           const Date& accrualEnd, const Size fixingDays,
                           const QuantLib::ext::shared_ptr<InterpolatedIborIndex>& index, Real gearing = 1.0, Real spread = 0.0,
                           const Date& refPeriodStart = Date(), const Date& refPeriodEnd = Date(),
                           const QuantLib::DayCounter& dayCounter = QuantLib::DayCounter(), bool isInArrears = false,
                           const Date& exCouponDate = Date(),
                           const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& iborIndex = nullptr);

    QuantLib::ext::shared_ptr<InterpolatedIborIndex> interpolatedIborIndex() const { return interpolatedIborIndex_; }
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& iborIndex() const { return iborIndex_; }

    //! \name Visitor interface
    //@{
    virtual void accept(QuantLib::AcyclicVisitor&);
    //@}

private:
    friend class InterpolatedIborCouponPricer;
    const boost::shared_ptr<InterpolatedIborIndex> interpolatedIborIndex_;
    const boost::shared_ptr<QuantLib::IborIndex> iborIndex_;
    // computed by coupon pricer (depending on par coupon flag) and stored here
    void initializeCachedData() const;
    mutable bool cachedDataIsInitialized_ = false;
    mutable Date fixingValueDate_, fixingEndDate_, fixingMaturityDate_;
    mutable QuantLib::Time spanningTime_, spanningTimeIndexMaturity_;
};

} // namespace QuantExt
