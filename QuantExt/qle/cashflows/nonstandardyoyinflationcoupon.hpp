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

/*! \file qle/cashflows/nonstandardyoyinflationcoupon.hpp
    \brief coupon which generalize the yoy inflation coupon
    it pays:
         N * (alpha * I_t/I_s + beta)
         N * (alpha * (I_t/I_s - 1) + beta)
    with s<t, if s < today its a ZC Inflation Coupon.
    \ingroup cashflows
*/

#ifndef quantext_nonstandardyoycoupon_coupon_hpp
#define quantext_nonstandardyoycoupon_coupon_hpp

#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %Coupon paying a YoY-inflation type index
class NonStandardYoYInflationCoupon : public InflationCoupon {
public:
    // This Coupon uses the start and end period to
    // t = endDate - observationLeg, s = startDate - observationLeg
    NonStandardYoYInflationCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                                  Natural fixingDays, const ext::shared_ptr<ZeroInflationIndex>& index,
                                  const Period& observationLag, const DayCounter& dayCounter, Real gearing = 1.0,
                                  Spread spread = 0.0, const Date& refPeriodStart = Date(),
                                  const Date& refPeriodEnd = Date(), bool addInflationNotional = false, 
                                  QuantLib::CPI::InterpolationType interpolation = QuantLib::CPI::InterpolationType::Flat);

    //! \name Inspectors
    //@{
    //! index gearing, i.e. multiplicative coefficient for the index
    Real gearing() const { return gearing_; }
    //! spread paid over the fixing of the underlying index
    Spread spread() const { return spread_; }

    Rate adjustedFixing() const;

    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}
    virtual Date fixingDateNumerator() const;
    virtual Date fixingDateDenumerator() const;

    virtual ext::shared_ptr<ZeroInflationIndex> cpiIndex() const;

    virtual Rate indexFixing() const override;
    virtual Date fixingDate() const override;
    virtual Rate rate() const override;

    bool addInflationNotional() const;
    bool isInterpolated() const { return interpolationType() == CPI::Linear; }
    QuantLib::CPI::InterpolationType interpolationType() const { return interpolationType_; }

protected:
    Date fixingDateNumerator_;
    Date fixingDateDenumerator_;
    Real gearing_;
    Spread spread_;
    bool addInflationNotional_;
    QuantLib::CPI::InterpolationType interpolationType_;
    bool checkPricerImpl(const ext::shared_ptr<InflationCouponPricer>&) const override;

private:
    void setFixingDates(const Date& denumatorDate, const Date& numeratorDate, const Period& observationLag);
};

} // namespace QuantExt

#endif
