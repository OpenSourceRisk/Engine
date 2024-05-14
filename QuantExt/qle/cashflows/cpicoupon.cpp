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

/*! \file cpicoupon.cpp
    \brief CPI leg builder extending QuantLib's to handle caps and floors
*/

#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <qle/cashflows/cpicoupon.hpp>

#include <ql/cashflows/cpicouponpricer.hpp>
#include <qle/cashflows/cpicouponpricer.hpp>
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>
//#include <iostream>

namespace QuantExt {

Rate CPICoupon::rate() const {
    Rate r = QuantLib::CPICoupon::rate() ;
        if (subtractInflationNominal_) {
            Rate adjusted_r = r / fixedRate_;
            r = (adjusted_r - 1) * fixedRate_;
        }
        return r;
    }

void CappedFlooredCPICoupon::setCommon(Rate cap, Rate floor) {

    isCapped_ = false;
    isFloored_ = false;

    if (fixedRate_ > 0) {
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

CappedFlooredCPICoupon::CappedFlooredCPICoupon(const ext::shared_ptr<CPICoupon>& underlying, Date startDate, Rate cap,
                                               Rate floor)
    : CPICoupon(underlying->baseCPI(), underlying->baseDate(), underlying->date(), underlying->nominal(), underlying->accrualStartDate(),
                underlying->accrualEndDate(), underlying->cpiIndex(),
                underlying->observationLag(), underlying->observationInterpolation(), underlying->dayCounter(),
                underlying->fixedRate(), underlying->referencePeriodStart(),
                underlying->referencePeriodEnd(), underlying->exCouponDate(), underlying->subtractInflationNotional()),
      underlying_(underlying), startDate_(startDate), isFloored_(false), isCapped_(false) {

    setCommon(cap, floor);
    registerWith(underlying);

    Calendar cal = underlying->cpiIndex()->fixingCalendar(); // not used by the CPICapFloor engine
    BusinessDayConvention conv = Unadjusted;                 // not used by the CPICapFloor engine

    if (isCapped_) {
        Rate effectiveCap = cap_;
        cpiCap_ = QuantLib::ext::make_shared<CPICapFloor>(
            Option::Call, underlying_->nominal(), startDate_, underlying_->baseCPI(), underlying_->date(), cal, conv,
            cal, conv, effectiveCap, underlying_->cpiIndex(), underlying_->observationLag(),
            underlying_->observationInterpolation());
        // std::cout << "Capped/Floored CPI Coupon" << std::endl
        // 	  << "  nominal = " << underlying_->nominal() << std::endl
        // 	  << "  paymentDate = " << QuantLib::io::iso_date(underlying_->date()) << std::endl
        // 	  << "  startDate = " << QuantLib::io::iso_date(startDate_) << std::endl
        // 	  << "  baseCPI = " << underlying_->baseCPI() << std::endl
        // 	  << "  lag = " << underlying_->observationLag() << std::endl
        // 	  << "  interpolation = " << underlying_->observationInterpolation() << std::endl;
    }
    if (isFloored_) {
        Rate effectiveFloor = floor_;
        cpiFloor_ = QuantLib::ext::make_shared<CPICapFloor>(
            Option::Put, underlying_->nominal(), startDate_, underlying_->baseCPI(), underlying_->date(), cal, conv,
            cal, conv, effectiveFloor, underlying_->cpiIndex(),
            underlying_->observationLag(),
            underlying_->observationInterpolation());
    }
}

Rate CappedFlooredCPICoupon::rate() const {
    // rate =  fixedRate * capped/floored index
    QuantLib::ext::shared_ptr<CappedFlooredCPICouponPricer> blackPricer =
        QuantLib::ext::dynamic_pointer_cast<CappedFlooredCPICouponPricer>(pricer_);
    QL_REQUIRE(blackPricer, "BlackCPICouponPricer or BachelierCPICouponPricer expected");
    Real capValue = 0.0, floorValue = 0.0;
    if (isCapped_) {
        cpiCap_->setPricingEngine(blackPricer->engine());
        capValue = cpiCap_->NPV();
    }
    if (isFloored_) {
        cpiFloor_->setPricingEngine(blackPricer->engine());
        floorValue = cpiFloor_->NPV();
    }
    Real discount = blackPricer->yieldCurve()->discount(underlying_->date());
    Real nominal = underlying_->nominal();
    // normalize, multiplication with nominal, year fraction, discount follows
    Real capAmount = capValue / (nominal * discount);
    Real floorAmount = floorValue / (nominal * discount);

    Rate swapletRate = underlying_->rate();
    // apply gearing
    Rate floorletRate = floorAmount * underlying_->fixedRate();
    Rate capletRate = capAmount * underlying_->fixedRate();
    // long floor, short cap
    Rate totalRate = swapletRate + floorletRate - capletRate;

    return totalRate;
}

void CappedFlooredCPICoupon::update() { notifyObservers(); }

void CappedFlooredCPICoupon::accept(AcyclicVisitor& v) {
    Visitor<CappedFlooredCPICoupon>* v1 = dynamic_cast<Visitor<CappedFlooredCPICoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        CPICoupon::accept(v);
}

CappedFlooredCPICashFlow::CappedFlooredCPICashFlow(const ext::shared_ptr<CPICashFlow>& underlying, Date startDate,
                                                   Period observationLag, Rate cap, Rate floor)
    : CPICashFlow(underlying->notional(), underlying->cpiIndex(),
                  startDate - observationLag, underlying->baseFixing(), underlying->observationDate(),
                  underlying->observationLag(), underlying->interpolation(), 
                  underlying->date(),
                  underlying->growthOnly()),
      underlying_(underlying), startDate_(startDate), observationLag_(observationLag), isFloored_(false),
      isCapped_(false) {

    setCommon(cap, floor);
    registerWith(underlying);

    auto index = QuantLib::ext::dynamic_pointer_cast<ZeroInflationIndex>(underlying->index());
    Calendar cal = index->fixingCalendar();     // not used by the CPICapFloor engine
    BusinessDayConvention conv = Unadjusted; // not used by the CPICapFloor engine

    if (isCapped_) {
        cpiCap_ = QuantLib::ext::make_shared<CPICapFloor>(Option::Call, underlying_->notional(), startDate_,
                                                  underlying_->baseFixing(), underlying_->date(), cal, conv, cal, conv,
                                                  cap_, index, observationLag_, underlying_->interpolation());
    }
    if (isFloored_) {
        cpiFloor_ = QuantLib::ext::make_shared<CPICapFloor>(Option::Put, underlying_->notional(), startDate_,
                                                    underlying_->baseFixing(), underlying_->date(), cal, conv, cal,
                                                    conv, floor_, index, observationLag_, underlying_->interpolation());
    }
}

void CappedFlooredCPICashFlow::setCommon(Rate cap, Rate floor) {
    isCapped_ = false;
    isFloored_ = false;

    if (cap != Null<Rate>()) {
        isCapped_ = true;
        cap_ = cap;
    }
    if (floor != Null<Rate>()) {
        floor_ = floor;
        isFloored_ = true;
    }
    if (isCapped_ && isFloored_) {
        QL_REQUIRE(cap >= floor, "cap level (" << cap << ") less than floor level (" << floor << ")");
    }
}

void CappedFlooredCPICashFlow::setPricer(const ext::shared_ptr<InflationCashFlowPricer>& pricer) {
    if (pricer_)
        unregisterWith(pricer_);
    pricer_ = pricer;
    if (pricer_)
        registerWith(pricer_);
    update();
}

Real CappedFlooredCPICashFlow::amount() const {
    QL_REQUIRE(pricer_, "pricer not set for capped/floored CPI cashflow");
    Real capValue = 0.0, floorValue = 0.0;
    if (isCapped_) {
        cpiCap_->setPricingEngine(pricer_->engine());
        capValue = cpiCap_->NPV();
    }
    if (isFloored_) {
        cpiFloor_->setPricingEngine(pricer_->engine());
        floorValue = cpiFloor_->NPV();
    }
    Real discount = pricer_->yieldCurve()->discount(underlying_->date());
    Real capAmount = capValue / discount;
    Real floorAmount = floorValue / discount;
    Real underlyingAmount = underlying_->amount();
    Real totalAmount = underlyingAmount - capAmount + floorAmount;
    return totalAmount;
}

CPILeg::CPILeg(const Schedule& schedule, const ext::shared_ptr<ZeroInflationIndex>& index,
               const Handle<YieldTermStructure>& rateCurve, const Real baseCPI, const Period& observationLag)
    : schedule_(schedule), index_(index), rateCurve_(rateCurve), baseCPI_(baseCPI), observationLag_(observationLag),
      paymentDayCounter_(Thirty360(Thirty360::BondBasis)), paymentAdjustment_(ModifiedFollowing), paymentCalendar_(schedule.calendar()),
      fixingDays_(std::vector<Natural>(1, 0)), observationInterpolation_(CPI::AsIndex), subtractInflationNominal_(true),
      finalFlowCap_(Null<Real>()), finalFlowFloor_(Null<Real>()), subtractInflationNominalAllCoupons_(false),
      startDate_(schedule_.dates().front()) {
    QL_REQUIRE(schedule_.dates().size() > 0, "empty schedule passed to CPILeg");
}

CPILeg& CPILeg::withObservationInterpolation(CPI::InterpolationType interp) {
    observationInterpolation_ = interp;
    return *this;
}

CPILeg& CPILeg::withFixedRates(Real fixedRate) {
    fixedRates_ = std::vector<Real>(1, fixedRate);
    return *this;
}

CPILeg& CPILeg::withFixedRates(const std::vector<Real>& fixedRates) {
    fixedRates_ = fixedRates;
    return *this;
}

CPILeg& CPILeg::withNotionals(Real notional) {
    notionals_ = std::vector<Real>(1, notional);
    return *this;
}

CPILeg& CPILeg::withNotionals(const std::vector<Real>& notionals) {
    notionals_ = notionals;
    return *this;
}

CPILeg& CPILeg::withSubtractInflationNominal(bool growthOnly) {
    subtractInflationNominal_ = growthOnly;
    return *this;
}

CPILeg& CPILeg::withPaymentDayCounter(const DayCounter& dayCounter) {
    paymentDayCounter_ = dayCounter;
    return *this;
}

CPILeg& CPILeg::withPaymentAdjustment(BusinessDayConvention convention) {
    paymentAdjustment_ = convention;
    return *this;
}

CPILeg& CPILeg::withPaymentCalendar(const Calendar& cal) {
    paymentCalendar_ = cal;
    return *this;
}

CPILeg& CPILeg::withPaymentLag(Natural lag) {
    paymentLag_ = lag;
    return *this;
}

CPILeg& CPILeg::withFixingDays(Natural fixingDays) {
    fixingDays_ = std::vector<Natural>(1, fixingDays);
    return *this;
}

CPILeg& CPILeg::withFixingDays(const std::vector<Natural>& fixingDays) {
    fixingDays_ = fixingDays;
    return *this;
}

CPILeg& CPILeg::withCaps(Rate cap) {
    caps_ = std::vector<Rate>(1, cap);
    return *this;
}

CPILeg& CPILeg::withCaps(const std::vector<Rate>& caps) {
    caps_ = caps;
    return *this;
}

CPILeg& CPILeg::withFloors(Rate floor) {
    floors_ = std::vector<Rate>(1, floor);
    return *this;
}

CPILeg& CPILeg::withFinalFlowCap(Rate cap) {
    finalFlowCap_ = cap;
    return *this;
}

CPILeg& CPILeg::withFinalFlowFloor(Rate floor) {
    finalFlowFloor_ = floor;
    return *this;
}

CPILeg& CPILeg::withFloors(const std::vector<Rate>& floors) {
    floors_ = floors;
    return *this;
}

CPILeg& CPILeg::withStartDate(const Date& startDate) {
    startDate_ = startDate;
    return *this;
}

CPILeg& CPILeg::withObservationLag(const Period& observationLag) {
    observationLag_ = observationLag;
    return *this;
}

CPILeg& CPILeg::withExCouponPeriod(const Period& period, const Calendar& cal, BusinessDayConvention convention,
                                   bool endOfMonth) {
    exCouponPeriod_ = period;
    exCouponCalendar_ = cal;
    exCouponAdjustment_ = convention;
    exCouponEndOfMonth_ = endOfMonth;
    return *this;
}

CPILeg& CPILeg::withSubtractInflationNominalAllCoupons(bool subtractInflationNominalAllCoupons) {
    subtractInflationNominalAllCoupons_ = subtractInflationNominalAllCoupons;
    return *this;
}

CPILeg::operator Leg() const {

    QL_REQUIRE(!notionals_.empty(), "no notional given");
    Size n = schedule_.size() - 1;
    Leg leg;
    leg.reserve(n + 1); // +1 for notional, we always have some sort ...
    Date baseDate = baseDate_ == Date() ? startDate_ - observationLag_ : baseDate_;
    if (n > 0) {
        QL_REQUIRE(!fixedRates_.empty(), "no fixedRates given");

        Date refStart, start, refEnd, end;

        for (Size i = 0; i < n; ++i) {
            refStart = start = schedule_.date(i);
            refEnd = end = schedule_.date(i + 1);
            Date paymentDate = paymentCalendar_.advance(end, paymentLag_, Days, paymentAdjustment_);

            Date exCouponDate;
            if (exCouponPeriod_ != Period()) {
                exCouponDate =
                    exCouponCalendar_.advance(paymentDate, -exCouponPeriod_, exCouponAdjustment_, exCouponEndOfMonth_);
            }

            if (i == 0 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1)) {
                BusinessDayConvention bdc = schedule_.businessDayConvention();
                refStart = schedule_.calendar().adjust(end - schedule_.tenor(), bdc);
            }
            if (i == n - 1 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1)) {
                BusinessDayConvention bdc = schedule_.businessDayConvention();
                refEnd = schedule_.calendar().adjust(start + schedule_.tenor(), bdc);
            }

            auto coup = ext::make_shared<CPICoupon>(
                baseCPI_, // all have same base for ratio
                baseDate, paymentDate, detail::get(notionals_, i, 0.0), start, end, index_, observationLag_,
                observationInterpolation_, paymentDayCounter_, detail::get(fixedRates_, i, 0.0),
                refStart, refEnd, exCouponDate, subtractInflationNominalAllCoupons_);

            // set a pricer for the underlying coupon straight away because it only provides computation - not data
            auto pricer = ext::make_shared<CPICouponPricer>(Handle<YieldTermStructure>(rateCurve_));
            coup->setPricer(pricer);

            if (detail::noOption(caps_, floors_, i)) { // just swaplet
                leg.push_back(coup);
            } else { // cap/floorlet
                auto cfCoup = ext::make_shared<CappedFlooredCPICoupon>(
                    coup, startDate_, detail::get(caps_, i, Null<Rate>()), detail::get(floors_, i, Null<Rate>()));
                // in this case we need to set the "outer" pricer later that handles cap and floor
                leg.push_back(cfCoup);
            }
        }
    }

    // in CPI legs you always have a notional flow of some sort
    
    // Previous implementations didn't differentiate the observation and payment dates
    Date observationDate = paymentCalendar_.adjust(schedule_.date(n), paymentAdjustment_);
    Date paymentDate = paymentCalendar_.advance(schedule_.date(n), paymentLag_, Days, paymentAdjustment_);

    ext::shared_ptr<CPICashFlow> xnl = ext::make_shared<CPICashFlow>(
        detail::get(notionals_, n, 0.0), index_, baseDate, baseCPI_, observationDate, observationLag_,
        observationInterpolation_, paymentDate, subtractInflationNominal_);

    if (finalFlowCap_ == Null<Real>() && finalFlowFloor_ == Null<Real>()) {
        leg.push_back(xnl);
    } else {
        ext::shared_ptr<CappedFlooredCPICashFlow> cfxnl = ext::make_shared<CappedFlooredCPICashFlow>(
            xnl, startDate_, observationLag_, finalFlowCap_, finalFlowFloor_);
        leg.push_back(cfxnl);
    }

    return leg;
}

} // namespace QuantExt
