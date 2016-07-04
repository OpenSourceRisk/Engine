/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/termstructures/datedstrippedoptionletadapter.hpp>

#include <ql/math/interpolations/linearinterpolation.hpp>

#include <algorithm>

using std::min;
using std::max;

namespace QuantExt {

    DatedStrippedOptionletAdapter::DatedStrippedOptionletAdapter(
        const boost::shared_ptr<DatedStrippedOptionletBase>& s) 
        : OptionletVolatilityStructure(s->referenceDate(), s->calendar(), s->businessDayConvention(), s->dayCounter()),
          optionletStripper_(s), nInterpolations_(s->optionletMaturities()), strikeInterpolations_(nInterpolations_) {
        registerWith(optionletStripper_);
    }

    boost::shared_ptr<SmileSection> DatedStrippedOptionletAdapter::smileSectionImpl(Time t) const {
        QL_FAIL("Smile section not yet implemented for DatedStrippedOptionletAdapter");
        //vector< Rate > optionletStrikes =
        //    optionletStripper_->optionletStrikes(
        //        0); // strikes are the same for all times ?!
        //vector< Real > stddevs;
        //for (Size i = 0; i < optionletStrikes.size(); i++) {
        //    stddevs.push_back(volatilityImpl(t, optionletStrikes[i]) *
        //                      sqrt(t));
        //}
        //// Extrapolation may be a problem with splines, but since minStrike()
        //// and maxStrike() are set, we assume that no one will use stddevs for
        //// strikes outside these strikes
        //CubicInterpolation::BoundaryCondition bc =
        //    optionletStrikes.size() >= 4 ? CubicInterpolation::Lagrange
        //                                 : CubicInterpolation::SecondDerivative;
        //return boost::make_shared< InterpolatedSmileSection< Cubic > >(
        //    t, optionletStrikes, stddevs, Null< Real >(),
        //    Cubic(CubicInterpolation::Spline, false, bc, 0.0, bc, 0.0),
        //    Actual365Fixed(), volatilityType(), displacement());
    }

    Volatility DatedStrippedOptionletAdapter::volatilityImpl(Time length, Rate strike) const {
        calculate();

        vector<Volatility> vol(nInterpolations_);
        for (Size i = 0; i < nInterpolations_; ++i)
            vol[i] = strikeInterpolations_[i]->operator()(strike, true);

        const vector<Time>& optionletTimes = optionletStripper_->optionletFixingTimes();
        boost::shared_ptr<LinearInterpolation> timeInterpolator = boost::make_shared<LinearInterpolation>(
            optionletTimes.begin(), optionletTimes.end(), vol.begin());
        return timeInterpolator->operator()(length, true);
    }

    void DatedStrippedOptionletAdapter::performCalculations() const {
        for (Size i = 0; i < nInterpolations_; ++i) {
            const vector<Rate>& optionletStrikes = optionletStripper_->optionletStrikes(i);
            const vector<Volatility>& optionletVolatilities = optionletStripper_->optionletVolatilities(i);
            strikeInterpolations_[i] = boost::make_shared<LinearInterpolation>(optionletStrikes.begin(), 
                optionletStrikes.end(), optionletVolatilities.begin());
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

    Date DatedStrippedOptionletAdapter::maxDate() const {
        return optionletStripper_->optionletFixingDates().back();
    }

    VolatilityType DatedStrippedOptionletAdapter::volatilityType() const {
        return optionletStripper_->volatilityType();
    }

    Real DatedStrippedOptionletAdapter::displacement() const {
        return optionletStripper_->displacement();
    }
}
