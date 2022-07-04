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

/*! \file qle/termstructures/discountratiomodifiedcurve.hpp
    \brief discount curve modified by the ratio of two other discount curves
*/

#ifndef quantext_discount_ratio_modified_curve_hpp
#define quantext_discount_ratio_modified_curve_hpp

#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

/*! The DiscountRatioModifiedCurve depends on three other yield curves. The dependency is via the discount factor.
    In particular, the discount factor \f$P(0, t)\f$ at time \f$t\f$ is given by:
    \f[
      P(0, t) = P_b(0, t) \frac{P_n(0, t)}{P_d(0, t)}
    \f]
    where \f$P_b(0, t)\f$ is the base curve discount factor, \f$P_n(0, t)\f$ is the numerator curve discount factor
    and \f$P_d(0, t)\f$ is the denominator curve discount factor.

    A use case for this type of discount curve is where we need to discount cashflows denominated in a currency, call
    it currency 1, and collateralised in a different currency, call it currency 2. Let \f$P_{1, 2}(0, t)\f$ denote the
    discount factor on this curve at time \f$t\f$. Assume that we have curves for discounting cashflows denominated in
    currency 1 and currency 2 and collaterised in a common reference currency. Let \f$P_{1, ref}(0, t)\f$ and
    \f$P_{2, ref}(0, t)\f$ denote the discount factors on these two curves respectively. Assume also that we have a
    curve for discounting cashflows denominated and collateralised in currency 2. Let \f$P_{2, 2}(0, t)\f$ denote the
    discount factor on this curve at time \f$t\f$. Then, by using DiscountRatioModifiedCurve we can set up the
    following relation:
    \f[
      P_{1, 2}(0, t) = P_{2, 2}(0, t) \frac{P_{1, ref}(0, t)}{P_{2, ref}(0, t)}
    \f]
    The assumption here is that forward FX rates remain the same if the FX forward's collateral currency is switched
    from the reference currency to currency 2.

    \warning One must be careful about mixing floating reference date and fixed reference date curves together as the
             underlying curves in this yield curve and then moving Settings::instance().evaluationDate(). This is
             alluded to in the corresponding unit tests. If the <code>moving_</code> member variable of TermStructure
             had an inspector method, then we could enforce that all underlying curves here are either floating or fixed
             reference date curves.
*/
class DiscountRatioModifiedCurve : public QuantLib::YieldTermStructure {
public:
    //! Constructor providing the three underlying yield curves
    DiscountRatioModifiedCurve(const QuantLib::Handle<QuantLib::YieldTermStructure>& baseCurve,
                               const QuantLib::Handle<QuantLib::YieldTermStructure>& numCurve,
                               const QuantLib::Handle<QuantLib::YieldTermStructure>& denCurve);

    //! \name Inspectors
    //@{
    //! Return the base curve
    const QuantLib::Handle<QuantLib::YieldTermStructure>& baseCurve() const { return baseCurve_; }
    //! Return the numerator curve
    const QuantLib::Handle<QuantLib::YieldTermStructure>& numeratorCurve() const { return numCurve_; }
    //! Return the denominator curve
    const QuantLib::Handle<QuantLib::YieldTermStructure>& denominatorCurve() const { return denCurve_; }
    //@}

    //! \name YieldTermStructure interface
    //@{
    //! Returns the day counter from the base curve
    QuantLib::DayCounter dayCounter() const override;
    //! Returns the calendar from the base curve
    QuantLib::Calendar calendar() const override;
    //! Returns the settlement days from the base curve
    QuantLib::Natural settlementDays() const override;
    //! Returns the reference date from the base curve
    const QuantLib::Date& referenceDate() const override;
    //! All range checks happen in the underlying curves
    QuantLib::Date maxDate() const override { return QuantLib::Date::maxDate(); }
    //@}

    //! \name Observer interface
    //@{
    void update() override;
    //@}

protected:
    //! \name YieldTermStructure interface
    //@{
    //! Perform the discount factor calculation using the three yield curves
    QuantLib::DiscountFactor discountImpl(QuantLib::Time t) const override;
    //@}

private:
    QuantLib::Handle<YieldTermStructure> baseCurve_;
    QuantLib::Handle<YieldTermStructure> numCurve_;
    QuantLib::Handle<YieldTermStructure> denCurve_;

    //! Check that none of the underlying term structures are empty
    void check() const;
};

} // namespace QuantExt

#endif
