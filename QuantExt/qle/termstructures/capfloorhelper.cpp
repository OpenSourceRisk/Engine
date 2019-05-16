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

#include <qle/termstructures/capfloorhelper.hpp>
#include <ql/instruments/makecapfloor.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>

using namespace QuantLib;

namespace {
void no_deletion(OptionletVolatilityStructure*) {}
}

namespace QuantExt {

CapFloorHelper::CapFloorHelper(
    CapFloor::Type type,
    const Period& tenor,
    Rate strike,
    const Handle<Quote>& quote,
    const boost::shared_ptr<IborIndex>& iborIndex,
    const Handle<YieldTermStructure>& discountingCurve,
    QuoteType quoteType,
    QuantLib::VolatilityType quoteVolatilityType,
    QuantLib::Real quoteDisplacement,
    bool endOfMonth)
    : RelativeDateBootstrapHelper<OptionletVolatilityStructure>(quote),
      type_(type),
      tenor_(tenor),
      strike_(strike),
      iborIndex_(iborIndex),
      discountHandle_(discountingCurve),
      quoteType_(quoteType),
      quoteVolatilityType_(quoteVolatilityType),
      quoteDisplacement_(quoteDisplacement),
      endOfMonth_(endOfMonth) {

    registerWith(iborIndex_);
    registerWith(discountHandle_);
    
    initializeDates();
}

void CapFloorHelper::initializeDates() {

    capFloor_ = MakeCapFloor(type_, tenor_, iborIndex_, strike_, 0 * Days).withEndOfMonth(endOfMonth_);

    // Maturity date is just the maturity date of the cap floor
    maturityDate_ = capFloor_->maturityDate();

    // We need the leg underlying the cap floor to determine the remaining date members
    const Leg& leg = capFloor_->floatingLeg();

    // Earliest date is the first optionlet fixing date
    boost::shared_ptr<CashFlow> cf = leg.front();
    boost::shared_ptr<FloatingRateCoupon> frc = boost::dynamic_pointer_cast<FloatingRateCoupon>(cf);
    QL_REQUIRE(frc, "Expected the first cashflow on the cap floor instrument to be a FloatingRateCoupon");
    earliestDate_ = frc->fixingDate();
    
    // Remaining dates are each equal to the fixing date on the final optionlet
    cf = leg.back();
    frc = boost::dynamic_pointer_cast<FloatingRateCoupon>(cf);
    QL_REQUIRE(frc, "Expected the final cashflow on the cap floor instrument to be a FloatingRateCoupon");
    pillarDate_ = latestDate_ = latestRelevantDate_ = frc->fixingDate();
}

void CapFloorHelper::setTermStructure(OptionletVolatilityStructure* ovts) {
    
    // Set this helper's optionlet volatility structure
    boost::shared_ptr<OptionletVolatilityStructure> temp(ovts, no_deletion);
    ovtsHandle_.linkTo(temp, false);

    // Set the term structure pointer member variable in the base class
    RelativeDateBootstrapHelper<OptionletVolatilityStructure>::setTermStructure(ovts);

    // Set this helper's pricing engine depending on the type of the optionlet volatilities
    if (ovts->volatilityType() == ShiftedLognormal) {
        capFloor_->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(discountHandle_, ovtsHandle_));
    } else {
        capFloor_->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(discountHandle_, ovtsHandle_));
    }
}

Real CapFloorHelper::impliedQuote() const {
    
    QL_REQUIRE(termStructure_ != 0, "CapFloorHelper's optionlet volatility term structure has not been set");
    capFloor_->recalculate();

    Real npv = capFloor_->NPV();
    if (quoteType_ == Premium) {
        return npv;
    } else {
        return capFloor_->impliedVolatility(npv, discountHandle_, quote_->value(), 
            1e-12, 100, 1e-8, 5.0, quoteVolatilityType_, quoteDisplacement_);
    }
}

void CapFloorHelper::accept(AcyclicVisitor& v) {
    if (Visitor<CapFloorHelper>* v1 = dynamic_cast<Visitor<CapFloorHelper>*>(&v))
        v1->visit(*this);
    else
        RelativeDateBootstrapHelper<OptionletVolatilityStructure>::accept(v);
}

}
