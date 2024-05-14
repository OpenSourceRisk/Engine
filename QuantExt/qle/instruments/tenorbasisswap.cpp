/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/indexes/ibor/libor.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>

#include <qle/instruments/tenorbasisswap.hpp>

using namespace QuantLib;

namespace QuantExt {

namespace {
class FairSpreadHelper {
public:
    FairSpreadHelper(const TenorBasisSwap& swap, const Handle<YieldTermStructure>& discountCurve, Real nonSpreadLegNPV)
        : nonSpreadLegNPV_(nonSpreadLegNPV) {

        engine_ = QuantLib::ext::shared_ptr<PricingEngine>(new DiscountingSwapEngine(discountCurve));
        arguments_ = dynamic_cast<Swap::arguments*>(engine_->getArguments());
        swap.setupArguments(arguments_);
        spreadLeg_ = arguments_->legs[0];
        results_ = dynamic_cast<const Swap::results*>(engine_->getResults());
    }

    Real operator()(Spread tempSpread) const {
        // Change the spread on the leg and recalculate
        Leg::const_iterator it;
        for (it = spreadLeg_.begin(); it != spreadLeg_.end(); ++it) {
            QuantLib::ext::shared_ptr<QuantExt::SubPeriodsCoupon1> c = QuantLib::ext::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(*it);
            c->spread() = tempSpread;
        }
        engine_->calculate();
        return results_->legNPV[0] + nonSpreadLegNPV_;
    }

private:
    QuantLib::ext::shared_ptr<PricingEngine> engine_;
    Real nonSpreadLegNPV_;
    const Swap::results* results_;
    Swap::arguments* arguments_;
    Leg spreadLeg_;
};
} // namespace

TenorBasisSwap::TenorBasisSwap(const Date& effectiveDate, Real nominal, const Period& swapTenor,
                               const QuantLib::ext::shared_ptr<IborIndex>& payIndex, Spread paySpread,
                               const Period& payFrequency, const QuantLib::ext::shared_ptr<IborIndex>& recIndex,
                               Spread recSpread, const Period& recFrequency, DateGeneration::Rule rule,
                               bool includeSpread, bool spreadOnRec, QuantExt::SubPeriodsCoupon1::Type type,
                               const bool telescopicValueDates)
    : Swap(2), nominals_(std::vector<Real>(1, nominal)), payIndex_(payIndex), paySpread_(paySpread),
      payFrequency_(payFrequency), recIndex_(recIndex), recSpread_(recSpread), recFrequency_(recFrequency),
      includeSpread_(includeSpread), spreadOnRec_(spreadOnRec), type_(type),
      telescopicValueDates_(telescopicValueDates) {

    // Create the default pay and rec schedules
    Date terminationDate = effectiveDate + swapTenor;

    QuantLib::ext::shared_ptr<Libor> payIndexAsLibor = QuantLib::ext::dynamic_pointer_cast<Libor>(payIndex_);
    payIndexCalendar_ = payIndexAsLibor != NULL ? payIndexAsLibor->jointCalendar() : payIndex_->fixingCalendar();
    QuantLib::ext::shared_ptr<Libor> recIndexAsLibor = QuantLib::ext::dynamic_pointer_cast<Libor>(recIndex_);
    recIndexCalendar_ =
        recIndexAsLibor != NULL ? recIndexAsLibor->jointCalendar() : recIndex_->fixingCalendar();

    paySchedule_ = MakeSchedule()
                        .from(effectiveDate)
                        .to(terminationDate)
                        .withTenor(payFrequency_)
                        .withCalendar(payIndexCalendar_)
                        .withConvention(payIndex_->businessDayConvention())
                        .withTerminationDateConvention(payIndex_->businessDayConvention())
                        .withRule(rule)
                        .endOfMonth(payIndex_->endOfMonth());

    recSchedule_ = MakeSchedule()
                        .from(effectiveDate)
                        .to(terminationDate)
                        .withTenor(recFrequency_)
                        .withCalendar(recIndexCalendar_)
                        .withConvention(recIndex_->businessDayConvention())
                        .withTerminationDateConvention(recIndex_->businessDayConvention())
                        .withRule(rule)
                        .endOfMonth(recIndex_->endOfMonth());

    // Create legs
    initializeLegs();
}

TenorBasisSwap::TenorBasisSwap(Real nominal, const Schedule& paySchedule,
                               const QuantLib::ext::shared_ptr<IborIndex>& payIndex, Spread paySpread,
                               const Schedule& recSchedule, const QuantLib::ext::shared_ptr<IborIndex>& recIndex,
                               Spread recSpread, bool includeSpread, bool spreadOnRec, QuantExt::SubPeriodsCoupon1::Type type,
                               const bool telescopicValueDates)
    : Swap(2), nominals_(std::vector<Real>(1, nominal)), paySchedule_(paySchedule), payIndex_(payIndex),
      paySpread_(paySpread), recSchedule_(recSchedule), recIndex_(recIndex), recSpread_(recSpread),
      includeSpread_(includeSpread), spreadOnRec_(spreadOnRec), type_(type), telescopicValueDates_(telescopicValueDates) {

    // Create legs
    initializeLegs();
}

TenorBasisSwap::TenorBasisSwap(const std::vector<Real>& nominals, const Schedule& paySchedule,
                               const QuantLib::ext::shared_ptr<IborIndex>& payIndex, Spread paySpread,
                               const Schedule& recSchedule, const QuantLib::ext::shared_ptr<IborIndex>& recIndex,
                               Spread recSpread, bool includeSpread, bool spreadOnRec, QuantExt::SubPeriodsCoupon1::Type type,
                               const bool telescopicValueDates)
    : Swap(2), nominals_(nominals), paySchedule_(paySchedule), payIndex_(payIndex),
      paySpread_(paySpread), recSchedule_(recSchedule), recIndex_(recIndex), recSpread_(recSpread),
      includeSpread_(includeSpread), spreadOnRec_(spreadOnRec), type_(type), telescopicValueDates_(telescopicValueDates) {

    // Create legs
    initializeLegs();
}

void TenorBasisSwap::initializeLegs() {

    // Checks
    QL_REQUIRE(paySchedule_.tenor() >= payIndex_->tenor(), "Expected paySchedule tenor to exceed/equal payIndex tenor");
    QL_REQUIRE(recSchedule_.tenor() >= recIndex_->tenor(), "Expected recSchedule tenor to exceed/equal recIndex tenor");

    noSubPeriod_ = true;

    // pay leg
    QuantLib::ext::shared_ptr<OvernightIndex> payIndexON = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(payIndex_);
    Leg payLeg;

    if (payIndexON) {
        payLeg = OvernightLeg(paySchedule_, payIndexON)
                     .withNotionals(nominals_)
                     .withSpreads(paySpread_)
                     .withTelescopicValueDates(telescopicValueDates_);
    } else {
        if (paySchedule_.tenor() == payIndex_->tenor()) {
            payLeg = IborLeg(paySchedule_, payIndex_)
                         .withNotionals(nominals_)
                         .withSpreads(paySpread_)
                         .withPaymentAdjustment(payIndex_->businessDayConvention())
                         .withPaymentDayCounter(payIndex_->dayCounter())
                         .withPaymentCalendar(payIndexCalendar_);
        } else {
            if (!spreadOnRec_) {
                //if spread leg and no overnight, the leg may be a subperiod leg
                payLeg = QuantExt::SubPeriodsLeg1(paySchedule_, payIndex_)
                             .withNotionals(nominals_)
                             .withSpread(paySpread_)
                             .withPaymentAdjustment(payIndex_->businessDayConvention())
                             .withPaymentDayCounter(payIndex_->dayCounter())
                             .withPaymentCalendar(payIndexCalendar_)
                             .includeSpread(includeSpread_)
                             .withType(type_);
                noSubPeriod_ = false;
            } else {
                QL_FAIL(
                    "Pay Leg could not be created. Neither overnight nor schedule index tenor match nor spread leg.");
            } // spreadOnRec
        }     // paySchedule_.tenor() == payIndex_->tenor()
    }         // payIndexON

    // receive leg
    QuantLib::ext::shared_ptr<OvernightIndex> recIndexON = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(recIndex_);
    Leg recLeg;

    if (recIndexON) {
        recLeg = OvernightLeg(recSchedule_, recIndexON)
                     .withNotionals(nominals_)
                     .withSpreads(recSpread_)
                     .withTelescopicValueDates(telescopicValueDates_);
    } else {
        if (recSchedule_.tenor() == recIndex_->tenor()) {
            recLeg = IborLeg(recSchedule_, recIndex_)
                         .withNotionals(nominals_)
                         .withSpreads(recSpread_)
                         .withPaymentAdjustment(recIndex_->businessDayConvention())
                         .withPaymentDayCounter(recIndex_->dayCounter())
                         .withPaymentCalendar(recIndexCalendar_);
        } else {
            if (spreadOnRec_) {
                //if spread leg and no overnight, the leg may be a subperiod leg
                recLeg = QuantExt::SubPeriodsLeg1(recSchedule_, recIndex_)
                             .withNotionals(nominals_)
                             .withSpread(recSpread_)
                             .withPaymentAdjustment(recIndex_->businessDayConvention())
                             .withPaymentDayCounter(recIndex_->dayCounter())
                             .withPaymentCalendar(recIndexCalendar_)
                             .includeSpread(includeSpread_)
                             .withType(type_);
                noSubPeriod_ = false;
            } else {
                QL_FAIL(
                    "Rec Leg could not be created. Neither overnight nor schedule index tenor match nor spread leg.");
            } //!spreadOnRec
        } //recSchedule_.tenor() == recIndex_->tenor()
    } //recIndexON

    //Allocate leg idx : spread leg = 0
    idxPay_ = 0;
    idxRec_ = 1;
    if(spreadOnRec_){
        idxPay_ = 1;
        idxRec_ = 0;
    }

    payer_[idxPay_] = -1.0;
    payer_[idxRec_] = +1.0;
    legs_[idxPay_] = payLeg;
    legs_[idxRec_] = recLeg;

    // Register this instrument with its coupons
    Leg::const_iterator it;
    for (it = legs_[0].begin(); it != legs_[0].end(); ++it)
        registerWith(*it);
    for (it = legs_[1].begin(); it != legs_[1].end(); ++it)
        registerWith(*it);


}

Real TenorBasisSwap::payLegBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[idxPay_] != Null<Real>(), "Pay leg BPS not available");
    return legBPS_[idxPay_];
}

Real TenorBasisSwap::payLegNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[idxPay_] != Null<Real>(), "Pay leg NPV not available");
    return legNPV_[idxPay_];
}

Rate TenorBasisSwap::fairPayLegSpread() const {
    calculate();
    QL_REQUIRE(fairSpread_[idxPay_] != Null<Spread>(), "Pay leg fair spread not available");
    return fairSpread_[idxPay_];
}

Real TenorBasisSwap::recLegBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[idxRec_] != Null<Real>(), "Receive leg BPS not available");
    return legBPS_[idxRec_];
}

Real TenorBasisSwap::recLegNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[idxRec_] != Null<Real>(), "Receive leg NPV not available");
    return legNPV_[idxRec_];
}

Spread TenorBasisSwap::fairRecLegSpread() const {
    calculate();
    QL_REQUIRE(fairSpread_[idxRec_] != Null<Spread>(), "Receive leg fair spread not available");
    return fairSpread_[idxRec_];
}

void TenorBasisSwap::setupExpired() const {
    Swap::setupExpired();
    fairSpread_ = { Null<Spread>(),Null<Spread>() };
}

void TenorBasisSwap::fetchResults(const PricingEngine::results* r) const {

    static const Spread basisPoint = 1.0e-4;
    Swap::fetchResults(r);

    const TenorBasisSwap::results* results = dynamic_cast<const TenorBasisSwap::results*>(r);

    if (results) {
        fairSpread_ = results->fairSpread;
    } else {
        fairSpread_ = { Null<Spread>(),Null<Spread>() };
    }

    //non spread leg (idx 1) should be fine - no averaging or compounding
    if (fairSpread_[1] == Null<Spread>()) {
        if (legBPS_[1] != Null<Real>()) {
            double s = spreadOnRec_ ? recSpread_ : paySpread_;
            fairSpread_[1] = s - NPV_ / (legBPS_[1] / basisPoint);
        }
    }

    /* spread leg (idx 0) fair spread calculation ok if no averaging/compounding OR
       if there is averaging/compounding and the spread is added after */
    if (fairSpread_[0] == Null<Spread>()) {
        if (noSubPeriod_ || !includeSpread_) {
            if (legBPS_[0] != Null<Real>()) {
                double s = spreadOnRec_ ? paySpread_ : recSpread_;
                fairSpread_[0] = s - NPV_ / (legBPS_[0] / basisPoint);
            }
        } else {
            // Need the discount curve
            Handle<YieldTermStructure> discountCurve;
            QuantLib::ext::shared_ptr<DiscountingSwapEngine> engine =
                QuantLib::ext::dynamic_pointer_cast<DiscountingSwapEngine>(engine_);
            if (engine) {
                discountCurve = engine->discountCurve();
                // Calculate a guess
                Spread guess = 0.0;
                if (legBPS_[0] != Null<Real>()) {
                    double s = spreadOnRec_ ? paySpread_ : recSpread_;
                    guess = s - NPV_ / (legBPS_[0] / basisPoint);
                }
                // Attempt to solve for fair spread
                Spread step = 1e-4;
                Real accuracy = 1e-8;
                FairSpreadHelper f(*this, discountCurve, legNPV_[1]);
                Brent solver;
                fairSpread_[0] = solver.solve(f, accuracy, guess, step);
            }
        }
    }
}

void TenorBasisSwap::results::reset() {
    Swap::results::reset();
    fairSpread = { Null<Spread>(),Null<Spread>() };
}
} // namespace QuantExt
