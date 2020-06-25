/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file cpicoupon.hpp
    \brief CPI leg builder extending QuantLib's to handle caps and floors
*/

#ifndef quantext_cpicoupon_hpp
#define quantext_cpicoupon_hpp

#include <ql/cashflows/cpicoupon.hpp>
#include <ql/instruments/cpicapfloor.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {
using namespace QuantLib;

class InflationCashFlowPricer;

//! Capped or floored CPI cashflow.
/*! Extended QuantLib::CPICashFlow
 */
class CappedFlooredCPICashFlow : public QuantLib::CPICashFlow {
public:
    CappedFlooredCPICashFlow(const ext::shared_ptr<CPICashFlow>& underlying, Date startDate = Date(),
                             Period observationLag = 0 * Days, Rate cap = Null<Rate>(), Rate floor = Null<Rate>());

    virtual Real amount() const;
    void setPricer(const ext::shared_ptr<InflationCashFlowPricer>& pricer);
    bool isCapped() const { return isCapped_; }
    bool isFloored() const { return isFloored_; }
    ext::shared_ptr<CPICashFlow> underlying() const { return underlying_; }

private:
    void setCommon(Rate cap, Rate floor);
    Real cap_, floor_;
    ext::shared_ptr<CPICashFlow> underlying_;
    ext::shared_ptr<CPICapFloor> cpiCap_, cpiFloor_;
    Date startDate_;
    Period observationLag_;
    bool isFloored_, isCapped_;
    ext::shared_ptr<InflationCashFlowPricer> pricer_;
};

//! Capped or floored CPI coupon.
/*! Extended QuantLib::CPICoupon
 */
class CappedFlooredCPICoupon : public CPICoupon {
public:
    CappedFlooredCPICoupon(const ext::shared_ptr<CPICoupon>& underlying, Date startDate = Date(),
                           Rate cap = Null<Rate>(), Rate floor = Null<Rate>());
    virtual Rate rate() const;

    ext::shared_ptr<CPICoupon> underlying() const { return underlying_; }

    //! \name Observer interface
    //@{
    void update();
    //@}

    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor& v);
    //@}

    bool isCapped() const { return isCapped_; }
    bool isFloored() const { return isFloored_; }

protected:
    virtual void setCommon(Rate cap, Rate floor);

    ext::shared_ptr<CPICoupon> underlying_;
    ext::shared_ptr<CPICapFloor> cpiCap_, cpiFloor_;
    Date startDate_;
    bool isFloored_, isCapped_;
    Rate cap_, floor_;
};

//! Helper class building a sequence of capped/floored CPI coupons.
/*! Also allowing for the inflated notional at the end...
    especially if there is only one date in the schedule.
    If a fixedRate is zero you get a FixedRateCoupon, otherwise
    you get a ZeroInflationCoupon.

    payoff is: spread + fixedRate x index
*/
class CPILeg {
public:
    CPILeg(const Schedule& schedule, const ext::shared_ptr<ZeroInflationIndex>& index, const Real baseCPI,
           const Period& observationLag);
    CPILeg& withNotionals(Real notional);
    CPILeg& withNotionals(const std::vector<Real>& notionals);
    CPILeg& withFixedRates(Real fixedRate);
    CPILeg& withFixedRates(const std::vector<Real>& fixedRates);
    CPILeg& withPaymentDayCounter(const DayCounter&);
    CPILeg& withPaymentAdjustment(BusinessDayConvention);
    CPILeg& withPaymentCalendar(const Calendar&);
    CPILeg& withFixingDays(Natural fixingDays);
    CPILeg& withFixingDays(const std::vector<Natural>& fixingDays);
    CPILeg& withObservationInterpolation(CPI::InterpolationType);
    CPILeg& withSubtractInflationNominal(bool);
    CPILeg& withSpreads(Spread spread);
    CPILeg& withSpreads(const std::vector<Spread>& spreads);
    CPILeg& withCaps(Rate cap);
    CPILeg& withCaps(const std::vector<Rate>& caps);
    CPILeg& withFloors(Rate floor);
    CPILeg& withFloors(const std::vector<Rate>& floors);
    CPILeg& withExCouponPeriod(const Period&, const Calendar&, BusinessDayConvention, bool endOfMonth = false);
    CPILeg& withStartDate(const Date& startDate);
    CPILeg& withObservationLag(const Period& observationLag);
    operator Leg() const;

private:
    Schedule schedule_;
    ext::shared_ptr<ZeroInflationIndex> index_;
    Real baseCPI_;
    Period observationLag_;
    std::vector<Real> notionals_;
    std::vector<Real> fixedRates_; // aka gearing
    DayCounter paymentDayCounter_;
    BusinessDayConvention paymentAdjustment_;
    Calendar paymentCalendar_;
    std::vector<Natural> fixingDays_;
    CPI::InterpolationType observationInterpolation_;
    bool subtractInflationNominal_;
    std::vector<Spread> spreads_;
    std::vector<Rate> caps_, floors_;
    Period exCouponPeriod_;
    Calendar exCouponCalendar_;
    BusinessDayConvention exCouponAdjustment_;
    bool exCouponEndOfMonth_;

    // Needed for pricing the embedded caps/floors
    Date startDate_;
};

} // namespace QuantExt

#endif
