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

#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <qle/cashflows/strippedcapflooredyoyinflationcoupon.hpp>

namespace QuantExt {
using namespace QuantLib;

StrippedCappedFlooredYoYInflationCoupon::StrippedCappedFlooredYoYInflationCoupon(
    const ext::shared_ptr<CappedFlooredYoYInflationCoupon>& underlying)
    : YoYInflationCoupon(underlying->date(), underlying->nominal(), underlying->accrualStartDate(),
                         underlying->accrualEndDate(), underlying->fixingDays(), underlying->yoyIndex(),
                         underlying->observationLag(), underlying->interpolation(), underlying->dayCounter(),
                         underlying->gearing(), underlying->spread(), underlying->referencePeriodStart(),
                         underlying->referencePeriodEnd()),
      underlying_(underlying) {
    registerWith(underlying);
}

Rate StrippedCappedFlooredYoYInflationCoupon::rate() const {

    QL_REQUIRE(underlying_->pricer() != NULL, "pricer not set");
    underlying_->pricer()->initialize(*underlying_);
    Rate floorletRate = 0.0;
    if (underlying_->isFloored())
        floorletRate = underlying_->pricer()->floorletRate(underlying_->effectiveFloor());
    Rate capletRate = 0.0;
    if (underlying_->isCapped())
        capletRate = underlying_->pricer()->capletRate(underlying_->effectiveCap());

    // if the underlying is collared we return the value of the embedded
    // collar, otherwise the value of a long floor or a long cap respectively

    return (underlying_->isFloored() && underlying_->isCapped()) ? floorletRate - capletRate
                                                                 : floorletRate + capletRate;
}

Rate StrippedCappedFlooredYoYInflationCoupon::cap() const { return underlying_->cap(); }

Rate StrippedCappedFlooredYoYInflationCoupon::floor() const { return underlying_->floor(); }

Rate StrippedCappedFlooredYoYInflationCoupon::effectiveCap() const { return underlying_->effectiveCap(); }

Rate StrippedCappedFlooredYoYInflationCoupon::effectiveFloor() const { return underlying_->effectiveFloor(); }

void StrippedCappedFlooredYoYInflationCoupon::update() { notifyObservers(); }

void StrippedCappedFlooredYoYInflationCoupon::accept(AcyclicVisitor& v) {
    underlying_->accept(v);
    Visitor<StrippedCappedFlooredYoYInflationCoupon>* v1 =
        dynamic_cast<Visitor<StrippedCappedFlooredYoYInflationCoupon>*>(&v);
    if (v1 != NULL)
        v1->visit(*this);
    else
        YoYInflationCoupon::accept(v);
}

bool StrippedCappedFlooredYoYInflationCoupon::isCap() const { return underlying_->isCapped(); }

bool StrippedCappedFlooredYoYInflationCoupon::isFloor() const { return underlying_->isFloored(); }

bool StrippedCappedFlooredYoYInflationCoupon::isCollar() const { return isCap() && isFloor(); }

void StrippedCappedFlooredYoYInflationCoupon::setPricer(const ext::shared_ptr<YoYInflationCouponPricer>& pricer) {
    YoYInflationCoupon::setPricer(pricer);
    underlying_->setPricer(pricer);
}

StrippedCappedFlooredYoYInflationCouponLeg::StrippedCappedFlooredYoYInflationCouponLeg(const Leg& underlyingLeg)
    : underlyingLeg_(underlyingLeg) {}

StrippedCappedFlooredYoYInflationCouponLeg::operator Leg() const {
    Leg resultLeg;
    resultLeg.reserve(underlyingLeg_.size());
    ext::shared_ptr<CappedFlooredYoYInflationCoupon> c;
    for (Leg::const_iterator i = underlyingLeg_.begin(); i != underlyingLeg_.end(); ++i) {
        if ((c = ext::dynamic_pointer_cast<CappedFlooredYoYInflationCoupon>(*i)) != NULL) {
            resultLeg.push_back(ext::make_shared<StrippedCappedFlooredYoYInflationCoupon>(c));
        } else {
            resultLeg.push_back(*i);
        }
    }
    return resultLeg;
}
} // namespace QuantExt
