/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file zerofixedcoupon.hpp
    \brief Nominal flow associated with a floating annuity coupon
    \ingroup cashflows
*/

#ifndef quantext_zero_fixed_coupon_hpp
#define quantext_zero_fixed_coupon_hpp

#include <ql/cashflows/coupon.hpp>
#include <ql/time/schedule.hpp>
#include <ql/time/businessdayconvention.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/compounding.hpp>
#include <vector>

namespace QuantExt {

using namespace QuantLib;

//! \ingroup cashflows
class ZeroFixedCoupon : public Coupon {
public:
    ZeroFixedCoupon(const Date& paymentDate,
                    double notional,
                    double rate,
                    const DayCounter& dc,
                    const std::vector<Date>& dates,
                    const Compounding& comp,
                    bool subtractNotional);

    //! \name Coupon interface
    //@{
    Real amount() const override;
    Real nominal() const override;
    Real rate() const override;
    DayCounter dayCounter() const override;
    Real accruedAmount(const Date& accrualEnd) const override;
    //@}

    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}

private:

    double notional_;
    double rate_;
    DayCounter dc_;
    std::vector<Date> dates_;
    Compounding comp_;
    bool subtractNotional_;

    double amount_;
};

} // namespace QuantExt

#endif
