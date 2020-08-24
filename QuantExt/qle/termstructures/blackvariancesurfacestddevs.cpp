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

#include <cmath>
#include <ql/quotes/all.hpp>
#include <qle/termstructures/blackvariancesurfacestddevs.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

BlackVarianceSurfaceStdDevs::BlackVarianceSurfaceStdDevs(
    const Calendar& cal, const Handle<Quote>& spot, const std::vector<Time>& times, const std::vector<Real>& stdDevs,
    const std::vector<std::vector<Handle<Quote> > >& blackVolMatrix, const DayCounter& dayCounter,
    const boost::shared_ptr<EqFxIndexBase>& index, bool stickyStrike,
    bool flatExtrapMoneyness)
    : BlackVarianceSurfaceMoneyness(cal, spot, times, stdDevs, blackVolMatrix, dayCounter, stickyStrike),
    index_(index), flatExtrapolateMoneyness_(flatExtrapMoneyness) {
    
    // set up atm variance curve - maybe just take ATM vols in
    vector<Real>::const_iterator it = find(stdDevs.begin(), stdDevs.end(), 0.0);
    QL_REQUIRE(it != stdDevs.end(), "atm D is required."); // this might fail
    atmVariances_.push_back(Real(0.0));
    atmTimes_.push_back(0.0);
    Size atmIndex = distance(stdDevs.begin(), it);
    for (Size i = 0; i < times.size(); i++) {
        atmVariances_.push_back(blackVolMatrix[atmIndex][i]->value() * blackVolMatrix[atmIndex][i]->value() * times[i]);
        atmTimes_.push_back(times[i]);
    }
    atmVarCurve_ = Linear().interpolate(atmTimes_.begin(), atmTimes_.end(), atmVariances_.begin());

    if (!stickyStrike) {
        QL_REQUIRE(index_ != nullptr, "index required for vol surface");
        registerWith(index_);
    } else {
        for (Size i = 0; i < times_.size(); i++) {
            Time t = times_[i];
            Real fwd = index_->forecastFixing(t);
            forwards_.push_back(fwd);
        }
        forwardCurve_ = Linear().interpolate(times_.begin(), times_.end(), forwards_.begin());
    }
}

Real BlackVarianceSurfaceStdDevs::moneyness(Time t, Real strike) const {
    Real fwd;
    Real reqD; // for flat extrapolation
    Real atmVolAtT;
    if (t == 0) {
        atmVolAtT = 0;
    } else {
        atmVolAtT = sqrt(atmVarCurve_(t, true) / t);
    }

    if (strike == Null<Real>() || strike == 0) {
        return 0.0;
    } else {
        if (stickyStrike_)
            fwd = forwardCurve_(t, true);
        else
            fwd = index_->forecastFixing(t);
        Real num = log(strike / fwd);
        Real denom = atmVolAtT * sqrt(t);
        reqD = num / denom;

        if (flatExtrapolateMoneyness_) {
            if (reqD < moneyness_.front()) {
                reqD = moneyness_.front();
            } else if (reqD > moneyness_.back()) {
                reqD = moneyness_.back();
            }
        }
        return reqD;
    }
}

void BlackVarianceSurfaceStdDevs::populateVolMatrix(
    const QuantLib::Handle<QuantLib::BlackVolTermStructure>& termStructre,
    vector<vector<Handle<Quote> > >& quotesToPopulate, const std::vector<QuantLib::Period>& expiries,
    const std::vector<Real>& stdDevPoints, const QuantLib::Interpolation& forwardCurve,
    const QuantLib::Interpolation atmVolCurve) {

    for (Size j = 0; j < expiries.size(); j++) {
        Date tmpDate = termStructre->referenceDate() +
                       expiries[j]; // todo: is the reference date of this termstructure as the asof date
        Time tmpTime = termStructre->timeFromReference(tmpDate);
        for (Size i = 0; i < stdDevPoints.size(); i++) {
            Real tmpStrike = forwardCurve(tmpTime) * exp(atmVolCurve(tmpTime) * sqrt(tmpTime) * stdDevPoints[i]);
            Volatility vol = termStructre->blackVol(tmpDate, tmpStrike, true);
            boost::shared_ptr<QuantLib::SimpleQuote> q(new SimpleQuote(vol));
            quotesToPopulate[i][j] = Handle<Quote>(q);
        }
    }
}

} // namespace QuantExt
