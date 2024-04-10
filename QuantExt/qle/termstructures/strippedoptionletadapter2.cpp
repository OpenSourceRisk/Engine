/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/strippedoptionletadapter2.hpp>

#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/interpolations/sabrinterpolation.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolsurface.hpp>
#include <ql/termstructures/volatility/interpolatedsmilesection.hpp>
#include <ql/termstructures/volatility/optionlet/optionletstripper.hpp>

using namespace QuantLib;
using std::max;
using std::min;
using std::vector;

namespace QuantExt {

StrippedOptionletAdapter2::StrippedOptionletAdapter2(const QuantLib::ext::shared_ptr<StrippedOptionletBase>& s,
                                                     const bool flatExtrapolation)
    : OptionletVolatilityStructure(s->settlementDays(), s->calendar(), s->businessDayConvention(), s->dayCounter()),
      optionletStripper_(s), nInterpolations_(s->optionletMaturities()), strikeInterpolations_(nInterpolations_),
      flatExtrapolation_(flatExtrapolation) {
    registerWith(optionletStripper_);
}

QuantLib::ext::shared_ptr<SmileSection> StrippedOptionletAdapter2::smileSectionImpl(Time t) const {
    std::vector<Rate> optionletStrikes =
        optionletStripper_->optionletStrikes(0); // strikes are the same for all times ?!
    std::vector<Real> stddevs;
    Real tEff = flatExtrapolation_ ? std::min(t, optionletStripper_->optionletFixingTimes().back()) : t;
    for (Size i = 0; i < optionletStrikes.size(); i++) {
        stddevs.push_back(volatilityImpl(tEff, optionletStrikes[i]) * std::sqrt(tEff));
    }
    // Use a linear interpolated smile section.
    // TODO: possibly make this configurable?
    if (flatExtrapolation_)
        return QuantLib::ext::make_shared<InterpolatedSmileSection<LinearFlat> >(t, optionletStrikes, stddevs, Null<Real>(),
                                                                         LinearFlat(), Actual365Fixed(),
                                                                         volatilityType(), displacement());
    else
        return QuantLib::ext::make_shared<InterpolatedSmileSection<Linear> >(
            t, optionletStrikes, stddevs, Null<Real>(), Linear(), Actual365Fixed(), volatilityType(), displacement());
}

Volatility StrippedOptionletAdapter2::volatilityImpl(Time length, Rate strike) const {

    calculate();

    vector<Volatility> vol(nInterpolations_);
    for (Size i = 0; i < nInterpolations_; ++i)
        vol[i] = strikeInterpolations_[i]->operator()(strike, true);

    vector<Time> optionletTimes = optionletStripper_->optionletFixingTimes();
    LinearInterpolation timeInterpolator(optionletTimes.begin(), optionletTimes.end(), vol.begin());

    // If flat extrapolation is turned on, extrapolate flat after last expiry _and_ before first expiry
    if (flatExtrapolation_) {
        length = max(min(length, optionletTimes.back()), optionletTimes.front());
    }

    return timeInterpolator(length, true);
}

void StrippedOptionletAdapter2::performCalculations() const {

    // const std::vector<Rate>& atmForward = optionletStripper_->atmOptionletRate();
    // const std::vector<Time>& optionletTimes = optionletStripper_->optionletTimes();

    for (Size i = 0; i < nInterpolations_; ++i) {
        const std::vector<Rate>& optionletStrikes = optionletStripper_->optionletStrikes(i);
        const std::vector<Volatility>& optionletVolatilities = optionletStripper_->optionletVolatilities(i);
        // strikeInterpolations_[i] = QuantLib::ext::shared_ptr<SABRInterpolation>(new
        //            SABRInterpolation(optionletStrikes.begin(), optionletStrikes.end(),
        //                              optionletVolatilities.begin(),
        //                              optionletTimes[i], atmForward[i],
        //                              0.02,0.5,0.2,0.,
        //                              false, true, false, false
        //                              //alphaGuess_, betaGuess_,
        //                              //nuGuess_, rhoGuess_,
        //                              //isParameterFixed_[0],
        //                              //isParameterFixed_[1],
        //                              //isParameterFixed_[2],
        //                              //isParameterFixed_[3]
        //                              ////,
        //                              //vegaWeightedSmileFit_,
        //                              //endCriteria_,
        //                              //optMethod_
        //                              ));
        QuantLib::ext::shared_ptr<Interpolation> tmp = QuantLib::ext::shared_ptr<LinearInterpolation>(
            new LinearInterpolation(optionletStrikes.begin(), optionletStrikes.end(), optionletVolatilities.begin()));
        if (flatExtrapolation_)
            strikeInterpolations_[i] = QuantLib::ext::make_shared<FlatExtrapolation>(tmp);
        else
            strikeInterpolations_[i] = tmp;

        // QL_ENSURE(strikeInterpolations_[i]->endCriteria()!=EndCriteria::MaxIterations,
        //          "section calibration failed: "
        //          "option time " << optionletTimes[i] <<
        //          ": " <<
        //              ", alpha " <<  strikeInterpolations_[i]->alpha()<<
        //              ", beta "  <<  strikeInterpolations_[i]->beta() <<
        //              ", nu "    <<  strikeInterpolations_[i]->nu()   <<
        //              ", rho "   <<  strikeInterpolations_[i]->rho()  <<
        //              ", error " <<  strikeInterpolations_[i]->interpolationError()
        //              );
    }
}

Rate StrippedOptionletAdapter2::minStrike() const {
    return optionletStripper_->optionletStrikes(0).front(); // FIX
}

Rate StrippedOptionletAdapter2::maxStrike() const {
    return optionletStripper_->optionletStrikes(0).back(); // FIX
}

Date StrippedOptionletAdapter2::maxDate() const { return optionletStripper_->optionletFixingDates().back(); }

VolatilityType StrippedOptionletAdapter2::volatilityType() const { return optionletStripper_->volatilityType(); }

Real StrippedOptionletAdapter2::displacement() const { return optionletStripper_->displacement(); }
} // namespace QuantExt
