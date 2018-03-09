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

#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/interpolations/sabrinterpolation.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolsurface.hpp>
#include <ql/termstructures/volatility/interpolatedsmilesection.hpp>
#include <ql/termstructures/volatility/optionlet/optionletstripper.hpp>
#include <qle/termstructures/strippedoptionletadapter2.hpp>

using namespace QuantLib;

namespace QuantExt {

StrippedOptionletAdapter2::StrippedOptionletAdapter2(const boost::shared_ptr<StrippedOptionletBase>& s)
    : OptionletVolatilityStructure(s->settlementDays(), s->calendar(), s->businessDayConvention(), s->dayCounter()),
      optionletStripper_(s), nInterpolations_(s->optionletMaturities()), strikeInterpolations_(nInterpolations_) {
    registerWith(optionletStripper_);
}

boost::shared_ptr<SmileSection> StrippedOptionletAdapter2::smileSectionImpl(Time t) const {
    std::vector<Rate> optionletStrikes =
        optionletStripper_->optionletStrikes(0); // strikes are the same for all times ?!
    std::vector<Real> stddevs;
    for (Size i = 0; i < optionletStrikes.size(); i++) {
        stddevs.push_back(volatilityImpl(t, optionletStrikes[i]) * std::sqrt(t));
    }
    // Extrapolation may be a problem with splines, but since minStrike()
    // and maxStrike() are set, we assume that no one will use stddevs for
    // strikes outside these strikes
    CubicInterpolation::BoundaryCondition bc =
        optionletStrikes.size() >= 4 ? CubicInterpolation::Lagrange : CubicInterpolation::SecondDerivative;
    return boost::make_shared<InterpolatedSmileSection<Cubic> >(
        t, optionletStrikes, stddevs, Null<Real>(), Cubic(CubicInterpolation::Spline, false, bc, 0.0, bc, 0.0),
        Actual365Fixed(), volatilityType(), displacement());
}

Volatility StrippedOptionletAdapter2::volatilityImpl(Time length, Rate strike) const {
    calculate();

    std::vector<Volatility> vol(nInterpolations_);
    for (Size i = 0; i < nInterpolations_; ++i)
        vol[i] = strikeInterpolations_[i]->operator()(strike, true);

    const std::vector<Time>& optionletTimes = optionletStripper_->optionletFixingTimes();
    boost::shared_ptr<LinearInterpolation> timeInterpolator(
        new LinearInterpolation(optionletTimes.begin(), optionletTimes.end(), vol.begin()));
    return timeInterpolator->operator()(length, true);
}

void StrippedOptionletAdapter2::performCalculations() const {

    // const std::vector<Rate>& atmForward = optionletStripper_->atmOptionletRate();
    // const std::vector<Time>& optionletTimes = optionletStripper_->optionletTimes();

    for (Size i = 0; i < nInterpolations_; ++i) {
        const std::vector<Rate>& optionletStrikes = optionletStripper_->optionletStrikes(i);
        const std::vector<Volatility>& optionletVolatilities = optionletStripper_->optionletVolatilities(i);
        // strikeInterpolations_[i] = boost::shared_ptr<SABRInterpolation>(new
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
        strikeInterpolations_[i] = boost::shared_ptr<LinearInterpolation>(
            new LinearInterpolation(optionletStrikes.begin(), optionletStrikes.end(), optionletVolatilities.begin()));

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
