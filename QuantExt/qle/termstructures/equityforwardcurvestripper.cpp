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

#include <qle/termstructures/equityforwardcurvestripper.hpp>

using std::vector;
using namespace QuantLib;

namespace QuantExt {

EquityForwardCurveStripper::EquityForwardCurveStripper(
    const boost::shared_ptr<BlackVarianceSurfaceSparse>& callSurface,
    const boost::shared_ptr<BlackVarianceSurfaceSparse>& putSurface,
    Handle<YieldTermStructure>& forecastCurve, Handle<QuantLib::Quote>& equitySpot,
    QuantLib::Exercise::Type = QuantLib::Exercise::Type::European) : 
    callSurface_(callSurface), putSurface_(putSurface), forecastCurve_(forecastCurve), equitySpot_(equitySpot) {

    // the calla nd put surfaces should have the same expiries/strikes, some checks to ensure this
    QL_REQUIRE(callSurface_->strikes() == putSurface_->strikes(), "Mismatch between Call and Put strikes in EquityForwardCurveStripper");
    QL_REQUIRE(callSurface_->expiries() == putSurface_->expiries(), "Mismatch between Call and Put expiries in EquityForwardCurveStripper");

    registerWith(callSurface);
    registerWith(putSurface);
    registerWith(forecastCurve);
    registerWith(equitySpot);
    registerWith(Settings::instance().evaluationDate());
}

void EquityForwardCurveStripper::performCalculations() const {

    vector<Date> expiries = callSurface_->expiries();
    vector<vector<Real> > allStrikes = callSurface_->strikes();
    
    // at each option expiry time we calulate a forward
    for (Size i = 0; i < expiries.size(); i++) {

        // get the relevant strikes at this expiry
        vector<Real> strikes = allStrikes.at(i);
 
        // we make a first guess at the forward price, we take this halfway between the spot
        // price and forward assuming no dividends i.e (S + S * e^r*t)/2
        Real guess = equitySpot_->value() / forecastCurve_->discount(expiries[i]);

        // if we only have one stike we just use that to get the forward
        // if our guess is below the first strike or after the last strike we just take the best strike
        Real strike;
        if (strikes.size() == 1 || guess <= strikes.front()) {
            strike = strikes.front();
        } else if (guess >= strikes.back()) {
            strike = strikes.back();
        }

        bool isForward = false;
        while (!isForward) {
            // get the strikes either side of our guess
            Real x1 = 0, x2 = 0;
            vector<Real>::iterator it = strikes.begin();
            while (it != strikes.end() && x2 <= guess) {
                x1 = x2;
                x2 = *it;
            }




        }


    }



}


}
