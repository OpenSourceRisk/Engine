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
#include <ql/cashflows/cpicouponpricer.hpp>
#include <qle/cashflows/strippedcapflooredcpicoupon.hpp>

namespace QuantExt {
using namespace QuantLib;

StrippedCappedFlooredCPICoupon::StrippedCappedFlooredCPICoupon(
    const ext::shared_ptr<CappedFlooredCPICoupon>& underlying)
    : CPICoupon(underlying->baseCPI(), underlying->date(), underlying->nominal(), underlying->accrualStartDate(),
                underlying->accrualEndDate(), underlying->fixingDays(), underlying->cpiIndex(),
                underlying->observationLag(), underlying->observationInterpolation(), underlying->dayCounter(),
                underlying->fixedRate(), underlying->spread(), underlying->referencePeriodStart(),
                underlying->referencePeriodEnd(), underlying->exCouponDate()),
      underlying_(underlying) {
    registerWith(underlying);
}

Rate StrippedCappedFlooredCPICoupon::rate() const {

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

Rate StrippedCappedFlooredCPICoupon::cap() const { return underlying_->cap(); }

Rate StrippedCappedFlooredCPICoupon::floor() const { return underlying_->floor(); }

Rate StrippedCappedFlooredCPICoupon::effectiveCap() const { return underlying_->effectiveCap(); }

Rate StrippedCappedFlooredCPICoupon::effectiveFloor() const { return underlying_->effectiveFloor(); }

void StrippedCappedFlooredCPICoupon::update() { notifyObservers(); }

void StrippedCappedFlooredCPICoupon::accept(AcyclicVisitor& v) {
    underlying_->accept(v);
    Visitor<StrippedCappedFlooredCPICoupon>* v1 = dynamic_cast<Visitor<StrippedCappedFlooredCPICoupon>*>(&v);
    if (v1 != NULL)
        v1->visit(*this);
    else
        CPICoupon::accept(v);
}

bool StrippedCappedFlooredCPICoupon::isCap() const { return underlying_->isCapped(); }

bool StrippedCappedFlooredCPICoupon::isFloor() const { return underlying_->isFloored(); }

bool StrippedCappedFlooredCPICoupon::isCollar() const { return isCap() && isFloor(); }

void StrippedCappedFlooredCPICoupon::setPricer(const ext::shared_ptr<CPICouponPricer>& pricer) {
    CPICoupon::setPricer(pricer);
    underlying_->setPricer(pricer);
}

StrippedCappedFlooredCPICouponLeg::StrippedCappedFlooredCPICouponLeg(const Leg& underlyingLeg)
    : underlyingLeg_(underlyingLeg) {}

StrippedCappedFlooredCPICouponLeg::operator Leg() const {
    Leg resultLeg;
    resultLeg.reserve(underlyingLeg_.size());
    ext::shared_ptr<CappedFlooredCPICoupon> c;
    for (Leg::const_iterator i = underlyingLeg_.begin(); i != underlyingLeg_.end(); ++i) {
        if ((c = ext::dynamic_pointer_cast<CappedFlooredCPICoupon>(*i)) != NULL) {
            resultLeg.push_back(ext::make_shared<StrippedCappedFlooredCPICoupon>(c));
        }
	// we don't want the following, just strip caps/floors and ignore the rest
	// else {
        //     resultLeg.push_back(*i);
        // }
    }
    return resultLeg;
}
} // namespace QuantExt
