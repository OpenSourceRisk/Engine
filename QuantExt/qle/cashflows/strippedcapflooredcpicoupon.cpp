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

#include <iostream>
#include <ql/cashflows/cpicouponpricer.hpp>
#include <ql/cashflows/inflationcoupon.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <qle/cashflows/strippedcapflooredcpicoupon.hpp>

namespace QuantExt {
using namespace QuantLib;

StrippedCappedFlooredCPICashFlow::StrippedCappedFlooredCPICashFlow(
    const ext::shared_ptr<CappedFlooredCPICashFlow>& underlying)
    : CPICashFlow(underlying->notional(), underlying->cpiIndex(), underlying->baseDate(), underlying->baseFixing(),
                  underlying->observationDate(), underlying->observationLag(),
                  underlying->interpolation(), underlying->date(), underlying->growthOnly()),
      underlying_(underlying) {
    registerWith(underlying_);
}

Real StrippedCappedFlooredCPICashFlow::amount() const {
    return underlying_->amount() - underlying_->underlying()->amount();
}

StrippedCappedFlooredCPICoupon::StrippedCappedFlooredCPICoupon(
    const ext::shared_ptr<CappedFlooredCPICoupon>& underlying)
    : CPICoupon(underlying->baseCPI(), underlying->date(), underlying->nominal(), underlying->accrualStartDate(),
                underlying->accrualEndDate(), underlying->cpiIndex(),
                underlying->observationLag(), underlying->observationInterpolation(), underlying->dayCounter(),
                underlying->fixedRate(), underlying->referencePeriodStart(),
                underlying->referencePeriodEnd(), underlying->exCouponDate()),
      underlying_(underlying) {
    registerWith(underlying_);
}

Rate StrippedCappedFlooredCPICoupon::rate() const { return underlying_->rate() - underlying_->underlying()->rate(); }

Rate StrippedCappedFlooredCPICoupon::cap() const {
    QL_FAIL("not implemented");
    return 0.0;
}

Rate StrippedCappedFlooredCPICoupon::floor() const {
    QL_FAIL("not implemented");
    return 0.0;
}

Rate StrippedCappedFlooredCPICoupon::effectiveCap() const {
    QL_FAIL("not implemented");
    return 0.0;
}

Rate StrippedCappedFlooredCPICoupon::effectiveFloor() const {
    QL_FAIL("not implemented");
    return 0.0;
}

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

StrippedCappedFlooredCPICouponLeg::StrippedCappedFlooredCPICouponLeg(const Leg& underlyingLeg)
    : underlyingLeg_(underlyingLeg) {}

StrippedCappedFlooredCPICouponLeg::operator Leg() const {
    Leg resultLeg;
    resultLeg.reserve(underlyingLeg_.size());
    ext::shared_ptr<CappedFlooredCPICoupon> c;
    ext::shared_ptr<CappedFlooredCPICashFlow> f;
    for (Leg::const_iterator i = underlyingLeg_.begin(); i != underlyingLeg_.end(); ++i) {
        if ((c = ext::dynamic_pointer_cast<CappedFlooredCPICoupon>(*i)) != NULL) {
            resultLeg.push_back(ext::make_shared<StrippedCappedFlooredCPICoupon>(c));
        } else if ((f = ext::dynamic_pointer_cast<CappedFlooredCPICashFlow>(*i)) != NULL) {
            resultLeg.push_back(ext::make_shared<StrippedCappedFlooredCPICashFlow>(f));
        }
        // we don't want the following, just strip caps/floors and ignore the rest
        // else {
        //     resultLeg.push_back(*i);
        // }
    }
    return resultLeg;
}
} // namespace QuantExt
