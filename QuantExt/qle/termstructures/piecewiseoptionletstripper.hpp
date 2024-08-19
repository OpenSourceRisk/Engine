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
#include <qle/termstructures/optionletstripper.hpp>
#include <qle/termstructures/piecewiseoptionletcurve.hpp>
#include <qle/termstructures/oiscapfloorhelper.hpp>

namespace QuantExt {

/*! Helper class to strip optionlet (i.e. caplet/floorlet) volatilities from the cap floor term volatilities of a
    CapFloorTermVolSurface.
*/
template <class Interpolator, template <class> class Bootstrap = QuantExt::IterativeBootstrap>
class PiecewiseOptionletStripper : public QuantExt::OptionletStripper {

public:
    typedef typename PiecewiseOptionletCurve<Interpolator, Bootstrap>::this_curve optionlet_curve;

    PiecewiseOptionletStripper(const QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface>& capFloorSurface,
                               const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index,
                               const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
                               bool flatFirstPeriod = true,
                               const QuantLib::VolatilityType capFloorVolType = QuantLib::ShiftedLognormal,
                               const QuantLib::Real capFloorVolDisplacement = 0.0,
                               const boost::optional<VolatilityType> optionletVolType = boost::none,
                               const boost::optional<QuantLib::Real> optionletVolDisplacement = boost::none,
                               bool interpOnOptionlets = true, const Interpolator& i = Interpolator(),
                               const Bootstrap<optionlet_curve>& bootstrap = Bootstrap<optionlet_curve>(),
                               const Period& rateComputationPeriod = 0 * Days, const Size onCapSettlementDays = 0);

    //! \name Inspectors
    //@{
    //! Volatility type for the underlying cap floor matrix
    QuantLib::VolatilityType capFloorVolType() const { return capFloorVolType_; }

    //! The applicable shift if the underlying cap floor matrix has shifted lognormal volatility
    QuantLib::Real capFloorVolDisplacement() const { return capFloorVolDisplacement_; }

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

private:
    //! Flat optionlet volatility before first optionlet fixing date
    bool flatFirstPeriod_;

    //! Volatility type for the underlying cap floor matrix
    QuantLib::VolatilityType capFloorVolType_;

    //! The applicable shift if the underlying cap floor matrix has shifted lognormal volatility
    QuantLib::Real capFloorVolDisplacement_;

    //! True to interpolate on optionlet volatilities, false to interpolate on cap floor term volatilities
    bool interpOnOptionlets_;

    //! The interpolator
    Interpolator interpolator_;

    //! The bootstrapper
    Bootstrap<optionlet_curve> bootstrap_;

    //! A one-dimensional optionlet curve for each strike in the underlying cap floor matrix
    mutable std::vector<QuantLib::ext::shared_ptr<optionlet_curve> > strikeCurves_;

    //! Store the vector of helpers for each strike column. The first dimension is strike and second is option tenor.
    typedef QuantLib::BootstrapHelper<QuantLib::OptionletVolatilityStructure> helper;
    std::vector<std::vector<QuantLib::ext::shared_ptr<helper> > > helpers_;

    //! Store the cap floor surface quotes. The first dimension is option tenor and second is strike.
    std::vector<std::vector<QuantLib::ext::shared_ptr<QuantLib::SimpleQuote> > > quotes_;
};

template <class Interpolator, template <class> class Bootstrap>
PiecewiseOptionletStripper<Interpolator, Bootstrap>::PiecewiseOptionletStripper(
    const QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface>& capFloorSurface,
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index, const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
    bool flatFirstPeriod, const QuantLib::VolatilityType capFloorVolType, const QuantLib::Real capFloorVolDisplacement,
    const boost::optional<VolatilityType> optionletVolType,
    const boost::optional<QuantLib::Real> optionletVolDisplacement, bool interpOnOptionlets, const Interpolator& i,
    const Bootstrap<optionlet_curve>& bootstrap, const Period& rateComputationPeriod, const Size onCapSettlementDays)
    : OptionletStripper(capFloorSurface, index, discount, optionletVolType ? *optionletVolType : capFloorVolType,
                        optionletVolDisplacement ? *optionletVolDisplacement : 0.0, rateComputationPeriod,
                        onCapSettlementDays),
      flatFirstPeriod_(flatFirstPeriod), capFloorVolType_(capFloorVolType),
      capFloorVolDisplacement_(capFloorVolDisplacement), interpOnOptionlets_(interpOnOptionlets), interpolator_(i),
      bootstrap_(bootstrap), strikeCurves_(nStrikes_), helpers_(nStrikes_) {

    // Readability
    // typedef QuantLib::BootstrapHelper<QuantLib::OptionletVolatilityStructure> helper;
    using QuantLib::Handle;
    using QuantLib::Period;
    using QuantLib::Quote;
    using QuantLib::SimpleQuote;
    using QuantLib::Size;
    using std::vector;

    vector<Rate> strikes = termVolSurface_->strikes();

    bool isOis = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index) != nullptr;

    // If we interpolate on term volatility surface first and then bootstrap, we have a cap floor helper for every
    // optionlet maturity.
    vector<Period> tenors = interpOnOptionlets_ ? termVolSurface_->optionTenors() : capFloorLengths_;
    quotes_.resize(tenors.size());

    // Initialise the quotes and helpers
    for (Size j = 0; j < strikes.size(); j++) {
        for (Size i = 0; i < tenors.size(); i++) {
            quotes_[i].push_back(QuantLib::ext::make_shared<SimpleQuote>(termVolSurface_->volatility(tenors[i], strikes[j])));
	    if(isOis) {
                Date effDate = index_->fixingCalendar().advance(
                    index_->fixingCalendar().adjust(capFloorSurface->referenceDate()), onCapSettlementDays_ * Days);
                helpers_[j].push_back(QuantLib::ext::make_shared<OISCapFloorHelper>(
                    CapFloorHelper::Automatic, tenors[i], rateComputationPeriod_, strikes[j],
                    Handle<Quote>(quotes_[i].back()), QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index_), discount_,
                    false, effDate, CapFloorHelper::Volatility, capFloorVolType_, capFloorVolDisplacement_));
            } else {
                helpers_[j].push_back(QuantLib::ext::make_shared<CapFloorHelper>(
                    CapFloorHelper::Automatic, tenors[i], strikes[j], Handle<Quote>(quotes_[i].back()), index_,
                    discount_, true, Date(), CapFloorHelper::Volatility, capFloorVolType_, capFloorVolDisplacement_));
            }
        }
    }
}

template <class Interpolator, template <class> class Bootstrap>
inline void PiecewiseOptionletStripper<Interpolator, Bootstrap>::performCalculations() const {

    // Some localised typedefs and using declarations to make the code more readable
    using QuantLib::Period;
    using QuantLib::Rate;
    using QuantLib::Size;
    using std::vector;

    // Update dates
    populateDates();

    // Readability
    vector<Rate> strikes = termVolSurface_->strikes();
    vector<Period> tenors = interpOnOptionlets_ ? termVolSurface_->optionTenors() : capFloorLengths_;

    // Update the quotes from the cap floor term volatility surface
    for (Size i = 0; i < tenors.size(); i++) {
        for (Size j = 0; j < strikes.size(); j++) {
            quotes_[i][j]->setValue(termVolSurface_->volatility(tenors[i], strikes[j]));
        }
    }

    // Populate the strike curves
    for (Size j = 0; j < strikes.size(); j++) {
        strikeCurves_[j] = QuantLib::ext::make_shared<optionlet_curve>(
            termVolSurface_->referenceDate(), helpers_[j], termVolSurface_->calendar(),
            termVolSurface_->businessDayConvention(), termVolSurface_->dayCounter(), volatilityType_, displacement_,
            flatFirstPeriod_, interpolator_, bootstrap_);
    }

    // Populate the optionlet volatilities and standard deviations
    for (Size j = 0; j < strikes.size(); j++) {
        for (Size i = 0; i < nOptionletTenors_; ++i) {
            optionletVolatilities_[i][j] = strikeCurves_[j]->volatility(optionletDates_[i], strikes[j]);
        }
    }
}

} // namespace QuantExt

#endif
