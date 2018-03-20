/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file floatingannuitycoupon.hpp
    \brief Coupon paying a Libor-type index
    \ingroup cashflows
*/

#ifndef quantext_floating_annuity_coupon_hpp
#define quantext_floating_annuity_coupon_hpp

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/time/schedule.hpp>

using namespace QuantLib;

namespace QuantExt {
//! floating annuity coupon
/*! %Coupon paying a Libor-type index on a variable nominal such that total flows are constant
    \ingroup cashflows
*/
class FloatingAnnuityCoupon : public Coupon, public LazyObject {
public:
    FloatingAnnuityCoupon(Real annuity, bool underflow, const boost::shared_ptr<Coupon>& previousCoupon,
                          const Date& paymentDate, const Date& startDate, const Date& endDate, Natural fixingDays,
                          const boost::shared_ptr<InterestRateIndex>& index, Real gearing = 1.0, Spread spread = 0.0,
                          const Date& refPeriodStart = Date(), const Date& refPeriodEnd = Date(),
                          const DayCounter& dayCounter = DayCounter(), bool isInArrears = false);
    //! \name Cashflow interface
    Rate amount() const;

    //! \name Inspectors
    Real accruedAmount(const Date& d) const;
    Rate nominal() const;
    Rate previousNominal() const;
    Rate rate() const;
    Real price(const Handle<YieldTermStructure>& discountingCurve) const;
    const boost::shared_ptr<InterestRateIndex>& index() const;
    DayCounter dayCounter() const;
    Rate indexFixing() const;
    Natural fixingDays() const;
    Date fixingDate() const;
    Real gearing() const;
    Spread spread() const;
    virtual Rate convexityAdjustment() const { return 0.0; }      // FIXME
    virtual Rate adjustedFixing() const { return indexFixing(); } // FIXME
    bool isInArrears() const;
    //@}
    //! \name Visiter interface
    //@{
    virtual void accept(AcyclicVisitor&);
    //@}
    //! \name LazyObject interface
    void performCalculations() const;

private:
    Real annuity_;
    bool underflow_;
    boost::shared_ptr<Coupon> previousCoupon_;
    mutable Real nominal_;

    // floating rate coupon members
    int fixingDays_;
    boost::shared_ptr<InterestRateIndex> index_;
    Real gearing_;
    Real spread_;
    DayCounter dayCounter_;
    bool isInArrears_;
};

inline const boost::shared_ptr<InterestRateIndex>& FloatingAnnuityCoupon::index() const { return index_; }

inline DayCounter FloatingAnnuityCoupon::dayCounter() const { return dayCounter_; }

inline Natural FloatingAnnuityCoupon::fixingDays() const { return fixingDays_; }

inline Real FloatingAnnuityCoupon::gearing() const { return gearing_; }

inline Spread FloatingAnnuityCoupon::spread() const { return spread_; }

inline bool FloatingAnnuityCoupon::isInArrears() const { return isInArrears_; }
} // namespace QuantExt

#endif
