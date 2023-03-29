/*
 Copyright (C) 2021 Quaternion Risk Management Ltd

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

/*
 Copyright (C) 2009 Chris Kenyon

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
 */

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <qle/cashflows/nonstandardcapflooredyoyinflationcoupon.hpp>

namespace QuantExt {

void NonStandardCappedFlooredYoYInflationCoupon::setCommon(Rate cap, Rate floor) {

    isCapped_ = false;
    isFloored_ = false;

    if (gearing_ > 0) {
        if (cap != Null<Rate>()) {
            isCapped_ = true;
            cap_ = cap;
        }
        if (floor != Null<Rate>()) {
            floor_ = floor;
            isFloored_ = true;
        }
    } else {
        if (cap != Null<Rate>()) {
            floor_ = cap;
            isFloored_ = true;
        }
        if (floor != Null<Rate>()) {
            isCapped_ = true;
            cap_ = floor;
        }
    }
    if (isCapped_ && isFloored_) {
        QL_REQUIRE(cap >= floor, "cap level (" << cap << ") less than floor level (" << floor << ")");
    }
}

NonStandardCappedFlooredYoYInflationCoupon::NonStandardCappedFlooredYoYInflationCoupon(
    const ext::shared_ptr<NonStandardYoYInflationCoupon>& underlying, Rate cap, Rate floor)
    : NonStandardYoYInflationCoupon(underlying->date(), underlying->nominal(), underlying->accrualStartDate(),
                                    underlying->accrualEndDate(), underlying->fixingDays(), underlying->cpiIndex(),
                                    underlying->observationLag(), underlying->dayCounter(), underlying->gearing(),
                                    underlying->spread(), underlying->referencePeriodStart(),
                                    underlying->referencePeriodEnd(), underlying->addInflationNotional(), 
                                    underlying->interpolationType()), 
    underlying_(underlying), isFloored_(false), isCapped_(false) {
    setCommon(cap, floor);
    registerWith(underlying);
}

NonStandardCappedFlooredYoYInflationCoupon::NonStandardCappedFlooredYoYInflationCoupon(
    const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate, Natural fixingDays,
    const ext::shared_ptr<ZeroInflationIndex>& index, const Period& observationLag, const DayCounter& dayCounter,
    Real gearing, Spread spread, const Rate cap, const Rate floor, const Date& refPeriodStart, const Date& refPeriodEnd,
    bool addInflationNotional, QuantLib::CPI::InterpolationType interpolation)
    : NonStandardYoYInflationCoupon(paymentDate, nominal, startDate, endDate, fixingDays, index, observationLag,
                                    dayCounter, gearing, spread, refPeriodStart, refPeriodEnd, addInflationNotional,
                                    interpolation),
      isFloored_(false), isCapped_(false) {
    setCommon(cap, floor);
}

void NonStandardCappedFlooredYoYInflationCoupon::setPricer(
    const ext::shared_ptr<NonStandardYoYInflationCouponPricer>& pricer) {

    NonStandardYoYInflationCoupon::setPricer(pricer);
    if (underlying_)
        underlying_->setPricer(pricer);
}

Rate NonStandardCappedFlooredYoYInflationCoupon::rate() const {
    Rate swapletRate = underlying_ ? underlying_->rate() : NonStandardYoYInflationCoupon::rate();

    if (isFloored_ || isCapped_) {
        if (underlying_) {
            QL_REQUIRE(underlying_->pricer(), "pricer not set");
        } else {
            QL_REQUIRE(pricer_, "pricer not set");
        }
    }

    Rate floorletRate = 0.;
    if (isFloored_) {
        floorletRate = underlying_ ? underlying_->pricer()->floorletRate(effectiveFloor())
                                   : pricer()->floorletRate(effectiveFloor());
    }
    Rate capletRate = 0.;
    if (isCapped_) {
        capletRate =
            underlying_ ? underlying_->pricer()->capletRate(effectiveCap()) : pricer()->capletRate(effectiveCap());
    }

    return swapletRate + floorletRate - capletRate;
}

Rate NonStandardCappedFlooredYoYInflationCoupon::cap() const {
    if ((gearing_ > 0) && isCapped_)
        return cap_;
    if ((gearing_ < 0) && isFloored_)
        return floor_;
    return Null<Rate>();
}

Rate NonStandardCappedFlooredYoYInflationCoupon::floor() const {
    if ((gearing_ > 0) && isFloored_)
        return floor_;
    if ((gearing_ < 0) && isCapped_)
        return cap_;
    return Null<Rate>();
}

Rate NonStandardCappedFlooredYoYInflationCoupon::effectiveCap() const {
    return (cap_ - (addInflationNotional_ ? 1 : 0) - spread()) / gearing();
}

Rate NonStandardCappedFlooredYoYInflationCoupon::effectiveFloor() const {
    return (floor_ - (addInflationNotional_ ? 1 : 0) - spread()) / gearing();
}

void NonStandardCappedFlooredYoYInflationCoupon::update() { notifyObservers(); }

void NonStandardCappedFlooredYoYInflationCoupon::accept(AcyclicVisitor& v) {
    Visitor<NonStandardCappedFlooredYoYInflationCoupon>* v1 =
        dynamic_cast<Visitor<NonStandardCappedFlooredYoYInflationCoupon>*>(&v);

    if (v1 != 0)
        v1->visit(*this);
    else
        NonStandardYoYInflationCoupon::accept(v);
}

NonStandardYoYInflationLeg::NonStandardYoYInflationLeg(const Schedule& schedule, const Calendar& paymentCalendar,
                                                       const ext::shared_ptr<ZeroInflationIndex>& index,
                                                       const Period& observationLag)
    : schedule_(schedule), index_(index), observationLag_(observationLag), paymentAdjustment_(ModifiedFollowing),
      paymentCalendar_(paymentCalendar), addInflationNotional_(false), interpolation_(QuantLib::CPI::Flat) {}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withNotionals(Real notional) {
    notionals_ = std::vector<Real>(1, notional);
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withNotionals(const std::vector<Real>& notionals) {
    notionals_ = notionals;
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withPaymentDayCounter(const DayCounter& dayCounter) {
    paymentDayCounter_ = dayCounter;
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withPaymentAdjustment(BusinessDayConvention convention) {
    paymentAdjustment_ = convention;
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withFixingDays(Natural fixingDays) {
    fixingDays_ = std::vector<Natural>(1, fixingDays);
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withFixingDays(const std::vector<Natural>& fixingDays) {
    fixingDays_ = fixingDays;
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withGearings(Real gearing) {
    gearings_ = std::vector<Real>(1, gearing);
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withGearings(const std::vector<Real>& gearings) {
    gearings_ = gearings;
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withSpreads(Spread spread) {
    spreads_ = std::vector<Spread>(1, spread);
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withSpreads(const std::vector<Spread>& spreads) {
    spreads_ = spreads;
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withCaps(Rate cap) {
    caps_ = std::vector<Rate>(1, cap);
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withCaps(const std::vector<Rate>& caps) {
    caps_ = caps;
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withFloors(Rate floor) {
    floors_ = std::vector<Rate>(1, floor);
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withFloors(const std::vector<Rate>& floors) {
    floors_ = floors;
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withRateCurve(const Handle<YieldTermStructure>& rateCurve) {
    rateCurve_ = rateCurve;
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withInflationNotional(bool addInflationNotional) {
    addInflationNotional_ = addInflationNotional;
    return *this;
}

NonStandardYoYInflationLeg& NonStandardYoYInflationLeg::withObservationInterpolation(QuantLib::CPI::InterpolationType interpolation) {
    interpolation_ = interpolation;
    return *this;
}

NonStandardYoYInflationLeg::operator Leg() const {

    Size n = schedule_.size() - 1;
    QL_REQUIRE(!notionals_.empty(), "no notional given");
    QL_REQUIRE(notionals_.size() <= n, "too many nominals (" << notionals_.size() << "), only " << n << " required");
    QL_REQUIRE(gearings_.size() <= n, "too many gearings (" << gearings_.size() << "), only " << n << " required");
    QL_REQUIRE(spreads_.size() <= n, "too many spreads (" << spreads_.size() << "), only " << n << " required");
    QL_REQUIRE(caps_.size() <= n, "too many caps (" << caps_.size() << "), only " << n << " required");
    QL_REQUIRE(floors_.size() <= n, "too many floors (" << floors_.size() << "), only " << n << " required");

    Leg leg;
    leg.reserve(n);

    Calendar calendar = paymentCalendar_;

    Date refStart, start, refEnd, end;

    for (Size i = 0; i < n; ++i) {
        refStart = start = schedule_.date(i);
        refEnd = end = schedule_.date(i + 1);
        Date paymentDate = calendar.adjust(end, paymentAdjustment_);
        if (i == 0 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1)) {
            BusinessDayConvention bdc = schedule_.businessDayConvention();
            refStart = schedule_.calendar().adjust(end - schedule_.tenor(), bdc);
        }
        if (i == n - 1 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1)) {
            BusinessDayConvention bdc = schedule_.businessDayConvention();
            refEnd = schedule_.calendar().adjust(start + schedule_.tenor(), bdc);
        }
        if (detail::get(gearings_, i, 1.0) == 0.0) { // fixed coupon
            leg.push_back(ext::shared_ptr<CashFlow>(new FixedRateCoupon(
                paymentDate, detail::get(notionals_, i, 1.0), detail::effectiveFixedRate(spreads_, caps_, floors_, i),
                paymentDayCounter_, start, end, refStart, refEnd)));
        } else {                                       // yoy inflation coupon
            if (detail::noOption(caps_, floors_, i)) { // just swaplet
                ext::shared_ptr<NonStandardYoYInflationCoupon> coup(new NonStandardYoYInflationCoupon(
                    paymentDate, detail::get(notionals_, i, 1.0), start, end, detail::get(fixingDays_, i, 0), index_,
                    observationLag_, paymentDayCounter_, detail::get(gearings_, i, 1.0), detail::get(spreads_, i, 0.0),
                    refStart, refEnd, addInflationNotional_, interpolation_));

                // in this case you can set a pricer
                // straight away because it only provides computation - not data
                ext::shared_ptr<NonStandardYoYInflationCouponPricer> pricer(
                    new NonStandardYoYInflationCouponPricer(rateCurve_));
                coup->setPricer(pricer);
                leg.push_back(ext::dynamic_pointer_cast<CashFlow>(coup));

            } else { // cap/floorlet
                leg.push_back(ext::shared_ptr<CashFlow>(new NonStandardCappedFlooredYoYInflationCoupon(
                    paymentDate, detail::get(notionals_, i, 1.0), start, end, detail::get(fixingDays_, i, 0), index_,
                    observationLag_, paymentDayCounter_, detail::get(gearings_, i, 1.0), detail::get(spreads_, i, 0.0),
                    detail::get(caps_, i, Null<Rate>()), detail::get(floors_, i, Null<Rate>()), refStart, refEnd,
                    addInflationNotional_, interpolation_)));
            }
        }
    }

    return leg;
}

} // namespace QuantExt
