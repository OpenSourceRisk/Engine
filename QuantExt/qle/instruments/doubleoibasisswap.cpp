/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <qle/instruments/doubleoibasisswap.hpp>

using namespace QuantLib;

namespace QuantExt {

DoubleOvernightIndexedBasisSwap::DoubleOvernightIndexedBasisSwap(Real nominal, 
    const Schedule& paySchedule, const boost::shared_ptr<OvernightIndex>& payIndex, 
    const Schedule& recSchedule, const boost::shared_ptr<OvernightIndex>& recIndex, 
    Spread paySpread, Spread recSpread, const bool telescopicValueDates)
    : Swap(2), nominals_(std::vector<Real>(1, nominal)), paySchedule_(paySchedule),
    payIndex_(payIndex), recSchedule_(recSchedule), recIndex_(recIndex),
    paySpread_(paySpread), recSpread_(recSpread), telescopicValueDates_(telescopicValueDates) {

    initialize();
}

DoubleOvernightIndexedBasisSwap::DoubleOvernightIndexedBasisSwap(std::vector<Real> nominals,
    const Schedule& paySchedule, const boost::shared_ptr<OvernightIndex>& payIndex, 
    const Schedule& recSchedule, const boost::shared_ptr<OvernightIndex>& recIndex, 
    Spread paySpread, Spread recSpread, const bool telescopicValueDates)
    : Swap(2), nominals_(nominals), paySchedule_(paySchedule),
    payIndex_(payIndex), recSchedule_(recSchedule), recIndex_(recIndex),
    paySpread_(paySpread), recSpread_(recSpread), telescopicValueDates_(telescopicValueDates) {

    initialize();
}

void DoubleOvernightIndexedBasisSwap::initialize() {
    legs_[0] = OvernightLeg(paySchedule_, payIndex_)
        .withNotionals(nominals_)
        .withSpreads(paySpread_)
        .withTelescopicValueDates(telescopicValueDates_);

    legs_[1] = OvernightLeg(recSchedule_, recIndex_)
                   .withNotionals(nominals_)
                   .withSpreads(recSpread_)
                   .withTelescopicValueDates(telescopicValueDates_);

    for (Size j = 0; j < 2; ++j) {
        for (Leg::iterator i = legs_[j].begin(); i != legs_[j].end(); ++i)
            registerWith(*i);
    }

    payer_[0] = -1.0;
    payer_[1] = +1.0;
}

Spread DoubleOvernightIndexedBasisSwap::fairPaySpread() const {
    static Spread basisPoint = 1.0e-4;
    calculate();
    return paySpread_ - NPV_ / (payBPS() / basisPoint);
}

Spread DoubleOvernightIndexedBasisSwap::fairRecSpread() const {
    static Spread basisPoint = 1.0e-4;
    calculate();
    return recSpread_ - NPV_ / (recBPS() / basisPoint);
}

Real DoubleOvernightIndexedBasisSwap::payBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[0] != Null<Real>(), "result not available");
    return legBPS_[0];
}

Real DoubleOvernightIndexedBasisSwap::recBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[1] != Null<Real>(), "result not available");
    return legBPS_[1];
}

Real DoubleOvernightIndexedBasisSwap::payNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[0] != Null<Real>(), "result not available");
    return legNPV_[0];
}

Real DoubleOvernightIndexedBasisSwap::recNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[1] != Null<Real>(), "result not available");
    return legNPV_[1];
}
} // namespace QuantExt
