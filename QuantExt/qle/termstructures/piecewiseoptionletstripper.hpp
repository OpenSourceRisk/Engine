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

/*! \file qle/termstructures/piecewiseoptionletstripper.hpp
    \brief Strip optionlet volatility surface from cap floor volatility term surface
*/

#ifndef quantext_piecewiseoptionletstripper_hpp
#define quantext_piecewiseoptionletstripper_hpp

#include <qle/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/piecewiseoptionletcurve.hpp>
#include <qle/termstructures/optionletstripper.hpp>

namespace QuantExt {

/*! Helper class to strip optionlet (i.e. caplet/floorlet) volatilities from the cap floor term volatilities of a 
    CapFloorTermVolSurface.
*/
template <class Interpolator, template <class> class Bootstrap = QuantExt::IterativeBootstrap>
class PiecewiseOptionletStripper : public QuantExt::OptionletStripper {

public:
    typedef PiecewiseOptionletCurve<Interpolator, Bootstrap> optionlet_curve;

    PiecewiseOptionletStripper(
        const boost::shared_ptr<QuantExt::CapFloorTermVolSurface>& capFloorSurface,
        const boost::shared_ptr<QuantLib::IborIndex>& index,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
        QuantLib::Real accuracy = 1.0e-12,
        bool flatFirstPeriod = true,
        const QuantLib::VolatilityType capFloorVolType = QuantLib::ShiftedLognormal,
        const QuantLib::Real capFloorVolDisplacement = 0.0,
        const boost::optional<VolatilityType> optionletVolType = boost::none,
        const boost::optional<QuantLib::Real> optionletVolDisplacement = boost::none,
        const Interpolator& i = Interpolator());

    //! \name Inspectors
    //@{
    //! Volatility type for the underlying cap floor matrix
    QuantLib::VolatilityType capFloorVolType() const { return capFloorVolType_; }
    
    //! The applicable shift if the underlying cap floor matrix has shifted lognormal volatility
    QuantLib::Real capFloorVolDisplacement() const { return capFloorVolDisplacement_; }

    //! \name LazyObject interface
    //@{
    void performCalculations() const;
    //@}

private:
    //! The accuracy to be used in the bootstrap
    QuantLib::Real accuracy_;

    //! Flat optionlet volatility before first optionlet fixing date
    bool flatFirstPeriod_;

    //! Volatility type for the underlying cap floor matrix
    QuantLib::VolatilityType capFloorVolType_;

    //! The applicable shift if the underlying cap floor matrix has shifted lognormal volatility
    QuantLib::Real capFloorVolDisplacement_;

    //! The interpolator
    Interpolator interpolator_;

    //! A one-dimensional optionlet curve for each strike in the underlying cap floor matrix
    mutable std::vector<boost::shared_ptr<optionlet_curve> > strikeCurves_;
};

template <class Interpolator, template <class> class Bootstrap>
PiecewiseOptionletStripper<Interpolator, Bootstrap>::PiecewiseOptionletStripper(
    const boost::shared_ptr<QuantExt::CapFloorTermVolSurface>& capFloorSurface,
    const boost::shared_ptr<QuantLib::IborIndex>& index,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
    QuantLib::Real accuracy,
    bool flatFirstPeriod,
    const QuantLib::VolatilityType capFloorVolType,
    const QuantLib::Real capFloorVolDisplacement,
    const boost::optional<VolatilityType> optionletVolType,
    const boost::optional<QuantLib::Real> optionletVolDisplacement,
    const Interpolator& i)
    : OptionletStripper(capFloorSurface, index, discount,
        optionletVolType ? *optionletVolType : capFloorVolType,
        optionletVolDisplacement ? *optionletVolDisplacement : 0.0),
      accuracy_(accuracy), flatFirstPeriod_(flatFirstPeriod), capFloorVolType_(capFloorVolType),
      capFloorVolDisplacement_(capFloorVolDisplacement), interpolator_(i),
      strikeCurves_(nStrikes_) {}

template <class Interpolator, template <class> class Bootstrap>
inline void PiecewiseOptionletStripper<Interpolator, Bootstrap>::performCalculations() const {

    // Some localised typedefs and using declarations to make the code more readable
    typedef QuantLib::BootstrapHelper<QuantLib::OptionletVolatilityStructure> helper;
    using QuantLib::Rate;
    using QuantLib::Size;
    using QuantLib::Handle;
    using QuantLib::Quote;
    using QuantLib::SimpleQuote;
    using QuantLib::Period;
    using std::vector;

    // Update dates
    populateDates();

    // Initialise a strike curve for each strike column 
    vector<Rate> strikes = termVolSurface_->strikes();
    vector<Period> tenors = termVolSurface_->optionTenors();
    for (Size j = 0; j < strikes.size(); j++) {
        vector<boost::shared_ptr<helper> > helpers(tenors.size());
        for (Size i = 0; i < tenors.size(); i++) {
            Handle<Quote> quote(boost::make_shared<SimpleQuote>(termVolSurface_->volatility(tenors[i], strikes[j])));
            helpers[i] = boost::make_shared<CapFloorHelper>(CapFloorHelper::Automatic, tenors[i], strikes[j],
                quote, iborIndex_, discount_, CapFloorHelper::Volatility, capFloorVolType_, capFloorVolDisplacement_);
        }
        strikeCurves_[j] = boost::make_shared<optionlet_curve>(
            termVolSurface_->referenceDate(), helpers, termVolSurface_->calendar(),
            termVolSurface_->businessDayConvention(), termVolSurface_->dayCounter(), volatilityType_,
            displacement_, flatFirstPeriod_, accuracy_, interpolator_);
    }

    // Populate the optionlet volatilities and standard deviations
    for (Size j = 0; j < strikes.size(); j++) {
        for (Size i = 0; i < nOptionletTenors_; ++i) {
            optionletVolatilities_[i][j] = strikeCurves_[j]->volatility(optionletDates_[i], strikes[j]);
        }
    }
}

}

#endif
