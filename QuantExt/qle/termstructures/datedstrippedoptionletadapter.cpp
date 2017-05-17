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

#include <qle/termstructures/datedstrippedoptionletadapter.hpp>

#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/termstructures/volatility/interpolatedsmilesection.hpp>

#include <algorithm>
#include <boost/make_shared.hpp>

using std::min;
using std::max;

namespace QuantExt {

DatedStrippedOptionletAdapter::DatedStrippedOptionletAdapter(const boost::shared_ptr<DatedStrippedOptionletBase>& s)
    : OptionletVolatilityStructure(s->referenceDate(), s->calendar(), s->businessDayConvention(), s->dayCounter()),
      optionletStripper_(s), nInterpolations_(s->optionletMaturities()), strikeInterpolations_(nInterpolations_) {
    registerWith(optionletStripper_);
}

boost::shared_ptr<SmileSection> DatedStrippedOptionletAdapter::smileSectionImpl(Time t) const {
    QL_FAIL("Smile section not yet implemented for DatedStrippedOptionletAdapter");
    // Arbitrarily choose the first row of strikes for the smile section independent variable
    // Generally a reasonable choice since:
    // 1) OptionletStripper1: all strike rows are the same
    // 2) OptionletStripper2: optionletStrikes(i) is a decreasing sequence
    // Still possibility of arbitrary externally provided strike rows where (0) does not include all
    vector<Rate> optionletStrikes = optionletStripper_->optionletStrikes(0);
    vector<Real> stdDevs(optionletStrikes.size());
    for (Size i = 0; i < optionletStrikes.size(); ++i)
        stdDevs[i] = volatilityImpl(t, optionletStrikes[i]) * sqrt(t);

    // Use a linear interpolated smile section.
    // TODO: possibly make this configurable?
    return boost::make_shared<InterpolatedSmileSection<Linear> >(t, optionletStrikes, stdDevs, Null<Real>(), Linear(),
                                                                 Actual365Fixed(), volatilityType(), displacement());
}

Volatility DatedStrippedOptionletAdapter::volatilityImpl(Time length, Rate strike) const {
    calculate();

    vector<Volatility> vol(nInterpolations_);
    for (Size i = 0; i < nInterpolations_; ++i)
        vol[i] = strikeInterpolations_[i]->operator()(strike, true);

    const vector<Time>& optionletTimes = optionletStripper_->optionletFixingTimes();
    boost::shared_ptr<LinearInterpolation> timeInterpolator =
        boost::make_shared<LinearInterpolation>(optionletTimes.begin(), optionletTimes.end(), vol.begin());
    return timeInterpolator->operator()(length, true);
}

void DatedStrippedOptionletAdapter::performCalculations() const {
    for (Size i = 0; i < nInterpolations_; ++i) {
        const vector<Rate>& optionletStrikes = optionletStripper_->optionletStrikes(i);
        const vector<Volatility>& optionletVolatilities = optionletStripper_->optionletVolatilities(i);
        strikeInterpolations_[i] = boost::make_shared<LinearInterpolation>(
            optionletStrikes.begin(), optionletStrikes.end(), optionletVolatilities.begin());
    }
}

Rate DatedStrippedOptionletAdapter::minStrike() const {
    Rate minStrike = optionletStripper_->optionletStrikes(0).front();
    for (Size i = 1; i < nInterpolations_; ++i) {
        minStrike = min(optionletStripper_->optionletStrikes(i).front(), minStrike);
    }
    return minStrike;
}

Rate DatedStrippedOptionletAdapter::maxStrike() const {
    Rate maxStrike = optionletStripper_->optionletStrikes(0).back();
    for (Size i = 1; i < nInterpolations_; ++i) {
        maxStrike = max(optionletStripper_->optionletStrikes(i).back(), maxStrike);
    }
    return maxStrike;
}

Date DatedStrippedOptionletAdapter::maxDate() const { return optionletStripper_->optionletFixingDates().back(); }

VolatilityType DatedStrippedOptionletAdapter::volatilityType() const { return optionletStripper_->volatilityType(); }

Real DatedStrippedOptionletAdapter::displacement() const { return optionletStripper_->displacement(); }
} // namespace QuantExt
