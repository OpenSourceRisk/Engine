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

#include <boost/bind/bind.hpp>
#include <ql/functional.hpp>
#include <ql/instruments/makecapfloor.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/quotes/derivedquote.hpp>
#include <ql/utilities/null_deleter.hpp>
#include <qle/termstructures/capfloorhelper.hpp>

using namespace QuantLib;
using std::ostream;

namespace QuantExt {

// The argument to RelativeDateBootstrapHelper below is not simply the `quote`. Instead we create a DerivedQuote that,
// each time it is asked for its value, it returns a premium by calling CapFloorHelper::npv with the `quote` value. In
// this way, the `BootstrapHelper<T>::quoteError()` is always based on the cap floor premium and we do not have imply
// the volatility. This leads to all kinds of issues when deciding on max and min values in the iterative bootstrap
// where the quote volatility type input is of one type and the optionlet structure that we are trying to build is of
// another different type.
CapFloorHelper::CapFloorHelper(Type type, const Period& tenor, Rate strike, const Handle<Quote>& quote,
                               const QuantLib::ext::shared_ptr<IborIndex>& iborIndex,
                               const Handle<YieldTermStructure>& discountingCurve, bool moving,
                               const QuantLib::Date& effectiveDate, QuoteType quoteType,
                               QuantLib::VolatilityType quoteVolatilityType, QuantLib::Real quoteDisplacement,
                               bool endOfMonth, bool firstCapletExcluded)
    : RelativeDateBootstrapHelper<OptionletVolatilityStructure>(
          Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<QuantLib::ext::function<Real(Real)> > >(
              quote, QuantLib::ext::bind(&CapFloorHelper::npv, this, QuantLib::ext::placeholders::_1)))),
      type_(type), tenor_(tenor), strike_(strike), iborIndex_(iborIndex), discountHandle_(discountingCurve),
      moving_(moving), effectiveDate_(effectiveDate), quoteType_(quoteType), quoteVolatilityType_(quoteVolatilityType),
      quoteDisplacement_(quoteDisplacement), endOfMonth_(endOfMonth), firstCapletExcluded_(firstCapletExcluded),
      rawQuote_(quote), initialised_(false) {

    if (quoteType_ == Premium) {
        QL_REQUIRE(type_ != Automatic, "Cannot have CapFloorHelper type 'Automatic' with quote type of Premium");
    }

    QL_REQUIRE(!(moving_ && effectiveDate_ != Date()),
               "A fixed effective date does not make sense for a moving helper");

    registerWith(iborIndex_);
    registerWith(discountHandle_);

    initializeDates();
    initialised_ = true;
}

void CapFloorHelper::initializeDates() {

    if (!initialised_ || moving_) {
        CapFloor::Type capFloorType = CapFloor::Cap;
        if (type_ == CapFloorHelper::Floor) {
            capFloorType = CapFloor::Floor;
        }

        // Initialise the instrument and a copy
        // The strike can be Null<Real>() to indicate an ATM cap floor helper
        Rate dummyStrike = strike_ == Null<Real>() ? 0.01 : strike_;
        capFloor_ = MakeCapFloor(capFloorType, tenor_, iborIndex_, dummyStrike, 0 * Days)
                        .withEndOfMonth(endOfMonth_)
                        .withEffectiveDate(effectiveDate_, firstCapletExcluded_);
        capFloorCopy_ = MakeCapFloor(capFloorType, tenor_, iborIndex_, dummyStrike, 0 * Days)
                            .withEndOfMonth(endOfMonth_)
                            .withEffectiveDate(effectiveDate_, firstCapletExcluded_);

        // Maturity date is just the maturity date of the cap floor
        maturityDate_ = capFloor_->maturityDate();

        // We need the leg underlying the cap floor to determine the remaining date members
        const Leg& leg = capFloor_->floatingLeg();

        // Earliest date is the first optionlet fixing date
        QuantLib::ext::shared_ptr<CashFlow> cf = leg.front();
        QuantLib::ext::shared_ptr<FloatingRateCoupon> frc = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(cf);
        QL_REQUIRE(frc, "Expected the first cashflow on the cap floor instrument to be a FloatingRateCoupon");
        earliestDate_ = frc->fixingDate();

        // Remaining dates are each equal to the fixing date on the final optionlet
        cf = leg.back();
        frc = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(cf);
        QL_REQUIRE(frc, "Expected the final cashflow on the cap floor instrument to be a FloatingRateCoupon");
        pillarDate_ = latestDate_ = latestRelevantDate_ = frc->fixingDate();
    }
}

void CapFloorHelper::setTermStructure(OptionletVolatilityStructure* ovts) {

    if (strike_ == Null<Real>()) {
        // If the strike is Null<Real>(), we want an ATM helper
        Rate atm = capFloor_->atmRate(**discountHandle_);
        capFloor_ = MakeCapFloor(capFloor_->type(), tenor_, iborIndex_, atm, 0 * Days)
                        .withEndOfMonth(endOfMonth_)
                        .withEffectiveDate(effectiveDate_, firstCapletExcluded_);
        capFloorCopy_ = MakeCapFloor(capFloor_->type(), tenor_, iborIndex_, atm, 0 * Days)
                            .withEndOfMonth(endOfMonth_)
                            .withEffectiveDate(effectiveDate_, firstCapletExcluded_);

    } else if (type_ == CapFloorHelper::Automatic && quoteType_ != Premium) {
        // If the helper is set to automatically choose the underlying instrument type, do it now based on the ATM rate
        Rate atm = capFloor_->atmRate(**discountHandle_);
        CapFloor::Type capFloorType = atm > strike_ ? CapFloor::Floor : CapFloor::Cap;
        if (capFloor_->type() != capFloorType) {
            capFloor_ = MakeCapFloor(capFloorType, tenor_, iborIndex_, strike_, 0 * Days)
                            .withEndOfMonth(endOfMonth_)
                            .withEffectiveDate(effectiveDate_, firstCapletExcluded_);
            capFloorCopy_ = MakeCapFloor(capFloorType, tenor_, iborIndex_, strike_, 0 * Days)
                                .withEndOfMonth(endOfMonth_)
                                .withEffectiveDate(effectiveDate_, firstCapletExcluded_);
        }
    }

    // Set this helper's optionlet volatility structure
    QuantLib::ext::shared_ptr<OptionletVolatilityStructure> temp(ovts, null_deleter());
    ovtsHandle_.linkTo(temp, false);

    // Set the term structure pointer member variable in the base class
    RelativeDateBootstrapHelper<OptionletVolatilityStructure>::setTermStructure(ovts);

    // Set this helper's pricing engine depending on the type of the optionlet volatilities
    if (ovts->volatilityType() == ShiftedLognormal) {
        capFloor_->setPricingEngine(QuantLib::ext::make_shared<BlackCapFloorEngine>(discountHandle_, ovtsHandle_));
    } else {
        capFloor_->setPricingEngine(QuantLib::ext::make_shared<BachelierCapFloorEngine>(discountHandle_, ovtsHandle_));
    }

    // If the quote type is not a premium, we will need to use capFloorCopy_ to return the premium from the volatility
    // quote
    if (quoteType_ != Premium) {
        if (quoteVolatilityType_ == ShiftedLognormal) {
            capFloorCopy_->setPricingEngine(QuantLib::ext::make_shared<BlackCapFloorEngine>(
                discountHandle_, rawQuote_, ovtsHandle_->dayCounter(), quoteDisplacement_));
        } else {
            capFloorCopy_->setPricingEngine(
                QuantLib::ext::make_shared<BachelierCapFloorEngine>(discountHandle_, rawQuote_, ovtsHandle_->dayCounter()));
        }
    }
}

Real CapFloorHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_ != 0, "CapFloorHelper's optionlet volatility term structure has not been set");
    capFloor_->deepUpdate();
    return capFloor_->NPV();
}

void CapFloorHelper::accept(AcyclicVisitor& v) {
    if (Visitor<CapFloorHelper>* v1 = dynamic_cast<Visitor<CapFloorHelper>*>(&v))
        v1->visit(*this);
    else
        RelativeDateBootstrapHelper<OptionletVolatilityStructure>::accept(v);
}

Real CapFloorHelper::npv(Real quoteValue) {
    if (quoteType_ == Premium) {
        return quoteValue;
    } else {
        // If the quote value is a volatility, return the premium
        return capFloorCopy_->NPV();
    }
}

ostream& operator<<(ostream& out, CapFloorHelper::Type type) {
    switch (type) {
    case CapFloorHelper::Cap:
        return out << "Cap";
    case CapFloorHelper::Floor:
        return out << "Floor";
    case CapFloorHelper::Automatic:
        return out << "Automatic";
    default:
        QL_FAIL("Unknown CapFloorHelper::Type (" << Integer(type) << ")");
    }
}

ostream& operator<<(ostream& out, CapFloorHelper::QuoteType type) {
    switch (type) {
    case CapFloorHelper::Volatility:
        return out << "Volatility";
    case CapFloorHelper::Premium:
        return out << "Premium";
    default:
        QL_FAIL("Unknown CapFloorHelper::QuoteType (" << Integer(type) << ")");
    }
}

} // namespace QuantExt
