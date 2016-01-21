/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

#include <qle/instruments/oibasisswap.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>

namespace QuantLib {

    OvernightIndexedBasisSwap::OvernightIndexedBasisSwap(
                    Type type,
                    Real nominal,
                    const Schedule& oisSchedule,
                    const boost::shared_ptr<OvernightIndex>& overnightIndex,
                    const Schedule& iborSchedule,
                    const boost::shared_ptr<IborIndex>& iborIndex,
                    Spread oisSpread,
                    Spread iborSpread)
    : Swap(2), type_(type),
      nominals_(std::vector<Real>(1, nominal)),
      oisSchedule_(oisSchedule), overnightIndex_(overnightIndex), 
      iborSchedule_(iborSchedule), iborIndex_(iborIndex), 
      oisSpread_(oisSpread), iborSpread_(iborSpread) {
        
        initialize();
    }

    OvernightIndexedBasisSwap::OvernightIndexedBasisSwap(
                    Type type,
                    std::vector<Real> nominals,
                    const Schedule& oisSchedule,
                    const boost::shared_ptr<OvernightIndex>& overnightIndex,
                    const Schedule& iborSchedule,
                    const boost::shared_ptr<IborIndex>& iborIndex,
                    Spread oisSpread,
                    Spread iborSpread)
    : Swap(2), type_(type), nominals_(nominals),
      oisSchedule_(oisSchedule), overnightIndex_(overnightIndex), 
      iborSchedule_(iborSchedule), iborIndex_(iborIndex), 
      oisSpread_(oisSpread), iborSpread_(iborSpread) {
        
        initialize();
    }

    void OvernightIndexedBasisSwap::initialize() {
        legs_[0] = IborLeg(iborSchedule_, iborIndex_)
            .withNotionals(nominals_)
            .withSpreads(iborSpread_);

        legs_[1] = OvernightLeg(oisSchedule_, overnightIndex_)
            .withNotionals(nominals_)
            .withSpreads(oisSpread_);

        for (Size j=0; j<2; ++j) {
            for (Leg::iterator i = legs_[j].begin(); i!= legs_[j].end(); ++i)
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
        return oisSpread_ - NPV_/(overnightLegBPS()/basisPoint);
    }

    Spread OvernightIndexedBasisSwap::fairIborSpread() const {
        static Spread basisPoint = 1.0e-4;
        calculate();
        return iborSpread_ - NPV_/(iborLegBPS()/basisPoint);
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

}
