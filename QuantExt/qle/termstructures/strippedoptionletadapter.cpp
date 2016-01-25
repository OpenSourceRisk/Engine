/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This file is supposed to be part of the QuantLib library eventually,
   in the meantime we provide is as part of the QuantExt library. */

/*
 Copyright (C) 2007 Giorgio Facchinetti
 Copyright (C) 2007 Katiuscia Manzoni
 Copyright (C) 2015 Peter Caspers

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <qle/termstructures/strippedoptionletadapter.hpp>
#include <qle/termstructures/optionletstripper.hpp>
#include <qle/termstructures/interpolatedsmilesection.hpp>

#include <ql/termstructures/volatility/capfloor/capfloortermvolsurface.hpp>
#include <ql/termstructures/volatility/sabrsmilesection.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/interpolations/sabrinterpolation.hpp>
#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/math/interpolations/sabrinterpolation.hpp>

#include <boost/assign/list_of.hpp>

namespace QuantExt {

    StrippedOptionletAdapter::StrippedOptionletAdapter(
                const boost::shared_ptr<StrippedOptionletBase>& s)
    : OptionletVolatilityStructure(s->settlementDays(),
                                   s->calendar(),
                                   s->businessDayConvention(),
                                   s->dayCounter()),
      optionletStripper_(s),
      nInterpolations_(s->optionletMaturities()),
      strikeInterpolations_(nInterpolations_) {
        registerWith(optionletStripper_);
    }

    boost::shared_ptr<SmileSection>
    StrippedOptionletAdapter::smileSectionImpl(Time t) const {
        std::vector<Rate> optionletStrikes =
            optionletStripper_->optionletStrikes(
                0); // strikes are the same for all times ?!
        std::vector<Real> stddevs;
        std::vector<Real> vols;
        for (Size i = 0; i < optionletStrikes.size(); i++) {
            vols.push_back(volatilityImpl(t, optionletStrikes[i]));
            stddevs.push_back(vols.back() * std::sqrt(t));
        }
        const std::vector<Time>& optionletTimes =
                                    optionletStripper_->optionletFixingTimes();
        Real atm = (*atmInterpolation_)(std::max(
            std::min(t, optionletTimes.back()), optionletTimes.front()));
        // ----------------------
        // cubic splines
        // ----------------------
        // Extrapolation may be a problem with splines, but since minStrike()
        // and maxStrike() are set, we assume that no one will use stddevs for
        // strikes outside these strikes
        CubicInterpolation::BoundaryCondition bc =
            optionletStrikes.size() >= 4 ? CubicInterpolation::Lagrange
                                         : CubicInterpolation::SecondDerivative;
        return boost::shared_ptr<SmileSection>(
            new InterpolatedSmileSection<Cubic>(
                t, optionletStrikes, stddevs, atm,
                Cubic(CubicInterpolation::Spline, false, bc, 0.0, bc, 0.0),
                Actual365Fixed(), volatilityType(), displacement()));
        // ----------------------
        // SABR
        // ----------------------
        // SABRInterpolation sabr(optionletStrikes.begin(), optionletStrikes.end(),
        //                        vols.begin(), t, atm, 0.03, 0.8, 0.10, 0.0,
        //                        false, false, false, false);
        // return boost::shared_ptr<SmileSection>(new SabrSmileSection(
        //     t, atm, boost::assign::list_of(sabr.alpha())(sabr.beta())(
        //                 sabr.nu())(sabr.rho())));
    }

    Volatility StrippedOptionletAdapter::volatilityImpl(Time length,
                                                        Rate strike) const {
        calculate();

        std::vector<Volatility> vol(nInterpolations_);
        for (Size i=0; i<nInterpolations_; ++i)
            vol[i] = strikeInterpolations_[i]->operator()(strike, true);

        const std::vector<Time>& optionletTimes =
                                    optionletStripper_->optionletFixingTimes();
        boost::shared_ptr<LinearInterpolation> timeInterpolator(new
            LinearInterpolation(optionletTimes.begin(), optionletTimes.end(),
                                vol.begin()));
        return timeInterpolator->operator()(length, true);
    }

    void StrippedOptionletAdapter::performCalculations() const {

        const std::vector<Rate>& atmForward = optionletStripper_->atmOptionletRates();
        const std::vector<Time>& optionletTimes = optionletStripper_->optionletFixingTimes();

        for (Size i=0; i<nInterpolations_; ++i) {
            const std::vector<Rate>& optionletStrikes =
                optionletStripper_->optionletStrikes(i);
            const std::vector<Volatility>& optionletVolatilities =
                optionletStripper_->optionletVolatilities(i);
            //strikeInterpolations_[i] = boost::shared_ptr<SABRInterpolation>(new
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
            strikeInterpolations_[i] = boost::shared_ptr<LinearInterpolation>(new
                LinearInterpolation(optionletStrikes.begin(),
                                    optionletStrikes.end(),
                                    optionletVolatilities.begin()));

            //QL_ENSURE(strikeInterpolations_[i]->endCriteria()!=EndCriteria::MaxIterations,
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
        atmInterpolation_ = boost::shared_ptr<LinearInterpolation>(
            new LinearInterpolation(optionletTimes.begin(),
                                    optionletTimes.end(), atmForward.begin()));
    }

    Rate StrippedOptionletAdapter::minStrike() const {
        return optionletStripper_->optionletStrikes(0).front(); //FIX
    }

    Rate StrippedOptionletAdapter::maxStrike() const {
        return optionletStripper_->optionletStrikes(0).back(); //FIX
    }

    Date StrippedOptionletAdapter::maxDate() const {
        return optionletStripper_->optionletFixingDates().back();
    }

    const VolatilityType StrippedOptionletAdapter::volatilityType() const {
        return optionletStripper_->volatilityType();
    }

    const Real StrippedOptionletAdapter::displacement() const {
        return optionletStripper_->displacement();
    }

}
