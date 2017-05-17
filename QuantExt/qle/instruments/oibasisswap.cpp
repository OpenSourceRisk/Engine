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

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <qle/instruments/oibasisswap.hpp>

using namespace QuantLib;

namespace QuantExt {

OvernightIndexedBasisSwap::OvernightIndexedBasisSwap(Type type, Real nominal, const Schedule& oisSchedule,
                                                     const boost::shared_ptr<OvernightIndex>& overnightIndex,
                                                     const Schedule& iborSchedule,
                                                     const boost::shared_ptr<IborIndex>& iborIndex, Spread oisSpread,
                                                     Spread iborSpread)
    : Swap(2), type_(type), nominals_(std::vector<Real>(1, nominal)), oisSchedule_(oisSchedule),
      overnightIndex_(overnightIndex), iborSchedule_(iborSchedule), iborIndex_(iborIndex), oisSpread_(oisSpread),
      iborSpread_(iborSpread) {

    initialize();
}

OvernightIndexedBasisSwap::OvernightIndexedBasisSwap(Type type, std::vector<Real> nominals, const Schedule& oisSchedule,
                                                     const boost::shared_ptr<OvernightIndex>& overnightIndex,
                                                     const Schedule& iborSchedule,
                                                     const boost::shared_ptr<QuantLib::IborIndex>& iborIndex,
                                                     Spread oisSpread, Spread iborSpread)
    : Swap(2), type_(type), nominals_(nominals), oisSchedule_(oisSchedule), overnightIndex_(overnightIndex),
      iborSchedule_(iborSchedule), iborIndex_(iborIndex), oisSpread_(oisSpread), iborSpread_(iborSpread) {

    initialize();
}

void OvernightIndexedBasisSwap::initialize() {
    legs_[0] = IborLeg(iborSchedule_, iborIndex_).withNotionals(nominals_).withSpreads(iborSpread_);

    legs_[1] = OvernightLeg(oisSchedule_, overnightIndex_).withNotionals(nominals_).withSpreads(oisSpread_);

    for (Size j = 0; j < 2; ++j) {
        for (Leg::iterator i = legs_[j].begin(); i != legs_[j].end(); ++i)
            registerWith(*i);
    }

    switch (type_) {
    case Payer:
        payer_[0] = -1.0;
        payer_[1] = +1.0;
        break;
    case Receiver:
        payer_[0] = +1.0;
        payer_[1] = -1.0;
        break;
    default:
        QL_FAIL("Unknown overnight-swap type");
    }
}

Spread OvernightIndexedBasisSwap::fairOvernightSpread() const {
    static Spread basisPoint = 1.0e-4;
    calculate();
    return oisSpread_ - NPV_ / (overnightLegBPS() / basisPoint);
}

Spread OvernightIndexedBasisSwap::fairIborSpread() const {
    static Spread basisPoint = 1.0e-4;
    calculate();
    return iborSpread_ - NPV_ / (iborLegBPS() / basisPoint);
}

Real OvernightIndexedBasisSwap::overnightLegBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[1] != Null<Real>(), "result not available");
    return legBPS_[1];
}

Real OvernightIndexedBasisSwap::iborLegBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[0] != Null<Real>(), "result not available");
    return legBPS_[0];
}

Real OvernightIndexedBasisSwap::iborLegNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[0] != Null<Real>(), "result not available");
    return legNPV_[0];
}

Real OvernightIndexedBasisSwap::overnightLegNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[1] != Null<Real>(), "result not available");
    return legNPV_[1];
}
} // namespace QuantExt
