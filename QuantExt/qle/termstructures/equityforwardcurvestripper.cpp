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
    const boost::shared_ptr<OptionPriceSurface>& callSurface,
    const boost::shared_ptr<OptionPriceSurface>& putSurface,
    Handle<YieldTermStructure>& forecastCurve, Handle<QuantLib::Quote>& equitySpot,
    QuantLib::Exercise::Type) : 
    callSurface_(callSurface), putSurface_(putSurface), forecastCurve_(forecastCurve), equitySpot_(equitySpot) {

    // the calla nd put surfaces should have the same expiries/strikes, some checks to ensure this
    QL_REQUIRE(callSurface_->strikes() == putSurface_->strikes(), "Mismatch between Call and Put strikes in EquityForwardCurveStripper");
    QL_REQUIRE(callSurface_->expiries() == putSurface_->expiries(), "Mismatch between Call and Put expiries in EquityForwardCurveStripper");


    // register with all market data
    registerWith(callSurface);
    registerWith(putSurface);
    registerWith(forecastCurve);
    registerWith(equitySpot);
    registerWith(Settings::instance().evaluationDate());
}

void EquityForwardCurveStripper::performCalculations() const {

    expiries_ = callSurface_->expiries();
    vector<vector<Real> > allStrikes = callSurface_->strikes();
    forwards_.resize(expiries_.size());

    // at each option expiry time we calulate a forward
    for (Size i = 0; i < expiries_.size(); i++) {

        // get the relevant strikes at this expiry
        vector<Real> strikes = allStrikes.at(i);
        
        // if we only have one strike we just use that to get the forward
        if (strikes.size() == 1) {
            forwards_[i] = forwardFromPutCallParity(expiries_[i], strikes.front());
            continue;
        }

        // we make a first guess at the forward price, we take this halfway between the spot
        // price and forward assuming no dividends i.e (S + S * e^r*t)/2
        Real forward = (equitySpot_->value() + (equitySpot_->value() / forecastCurve_->discount(expiries_[i]))) / 2;
        
        Size maxIter = 100;
        Size j = 0;
        bool isForward = false;
        while (!isForward && j < maxIter) {
            Real newForward;
            // if our guess is below the first strike or after the last strike we just take the relevant strike
            if (forward <= strikes.front()) {
                newForward = forwardFromPutCallParity(expiries_[i], strikes.front());
                // if forward is still less than first strike we accept this
                isForward = newForward <= strikes.front();
            } else if (forward >= strikes.back()) {
                newForward = forwardFromPutCallParity(expiries_[i], strikes.back());
                // if forward is still greater than last strike we accept this
                isForward = newForward >= strikes.front();
            } else {
                // get the strikes either side of our guess
                ptrdiff_t ind1, ind2;       // index for strike before/after requested
                ind2 = distance(strikes.begin(), lower_bound(strikes.begin(), strikes.end(), forward));
                ind1 = (ind2 != 0) ? ind2 - 1 : 0;
                Real x1 = strikes[ind1];
                Real x2 = strikes[ind2];

                // get the forward at each strike
                Real f1 = forwardFromPutCallParity(expiries_[i], x1);
                Real f2 = forwardFromPutCallParity(expiries_[i], x2);

                // take our new forward as the average of the 2
                newForward = (f1 + f2) / 2;

                // if the new forward lies between the same 2 strikes we accept the new forward
                isForward = ind2 == distance(strikes.begin(), lower_bound(strikes.begin(), strikes.end(), newForward));                    
            }
            
            forward = newForward;
            j++;
        }
        forwards_[i] = forward;
    }
}

Real EquityForwardCurveStripper::forwardFromPutCallParity(Date d, Real strike) const {
    Real C = callSurface_->price(d, strike);
    Real P = putSurface_->price(d, strike);
    Real D = forecastCurve_->discount(d);

    return strike + (C - P) / D;
}

const vector<Date>& EquityForwardCurveStripper::expiries() const {
    calculate();
    return expiries_;
}

const vector<Real>& EquityForwardCurveStripper::forwards() const {
    calculate();
    return forwards_;
}

}
