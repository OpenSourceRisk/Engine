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

/*! \file qle/termstructures/piecewiseatmoptionletcurve.hpp
    \brief Create optionlet volatility structure from at-the-money cap floor term volatility curve
*/

#ifndef quantext_piecewiseatmoptionletcurve_hpp
#define quantext_piecewiseatmoptionletcurve_hpp

#include <qle/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/piecewiseoptionletcurve.hpp>
#include <qle/termstructures/capfloortermvolcurve.hpp>

namespace QuantExt {

/*! Helper class to strip caplet/floorlet volatilities from the cap floor term volatilities of a 
    CapFloorTermVolCurve.
*/
template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap = QuantExt::IterativeBootstrap>
class PiecewiseAtmOptionletCurve : public QuantLib::OptionletVolatilityStructure, public QuantLib::LazyObject {

public:
    typedef typename PiecewiseOptionletCurve<Interpolator, Bootstrap>::this_curve optionlet_curve;

    PiecewiseAtmOptionletCurve(
        QuantLib::Natural settlementDays,
        const boost::shared_ptr<QuantExt::CapFloorTermVolCurve<TermVolInterpolator> >& cftvc,
        const boost::shared_ptr<QuantLib::IborIndex>& index,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
        QuantLib::Real accuracy = 1.0e-12,
        bool flatFirstPeriod = true,
        const QuantLib::VolatilityType capFloorVolType = QuantLib::ShiftedLognormal,
        const QuantLib::Real capFloorVolDisplacement = 0.0,
        const boost::optional<QuantLib::VolatilityType> optionletVolType = boost::none,
        const boost::optional<QuantLib::Real> optionletVolDisplacement = boost::none,
        const Interpolator& i = Interpolator(),
        const Bootstrap<optionlet_curve>& bootstrap = Bootstrap<optionlet_curve>());

    PiecewiseAtmOptionletCurve(
        const QuantLib::Date& settlementDate,
        const boost::shared_ptr<QuantExt::CapFloorTermVolCurve<TermVolInterpolator> >& cftvc,
        const boost::shared_ptr<QuantLib::IborIndex>& index,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
        QuantLib::Real accuracy = 1.0e-12,
        bool flatFirstPeriod = true,
        const QuantLib::VolatilityType capFloorVolType = QuantLib::ShiftedLognormal,
        const QuantLib::Real capFloorVolDisplacement = 0.0,
        const boost::optional<QuantLib::VolatilityType> optionletVolType = boost::none,
        const boost::optional<QuantLib::Real> optionletVolDisplacement = boost::none,
        const Interpolator& i = Interpolator(),
        const Bootstrap<optionlet_curve>& bootstrap = Bootstrap<optionlet_curve>());

    //! \name Inspectors
    //@{
    //! Volatility type for the underlying ATM cap floor curve
    QuantLib::VolatilityType capFloorVolType() const { return capFloorVolType_; }
    
    //! The applicable shift if the underlying ATM cap floor curve has shifted lognormal volatility
    QuantLib::Real capFloorVolDisplacement() const { return capFloorVolDisplacement_; }

    //! \name Observer interface
    //@{
    void update();
    //@}

    //! \name LazyObject interface
    //@{
    void performCalculations() const;
    //@}

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const;
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    QuantLib::Rate minStrike() const;
    QuantLib::Rate maxStrike() const;
    //@}

    //! \name OptionletVolatilityStructure interface
    //@{
    QuantLib::VolatilityType volatilityType() const;
    QuantLib::Real displacement() const;
    //@}

protected:
    //! \name OptionletVolatilityStructure interface
    //@{
    boost::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime) const;
    QuantLib::Volatility volatilityImpl(QuantLib::Time optionTime, QuantLib::Rate strike) const;
    //@}

private:
    //! The underlying ATM cap floor term volatility curve
    boost::shared_ptr<QuantExt::CapFloorTermVolCurve<TermVolInterpolator> > cftvc_;

    //! The accuracy to be used in the bootstrap
    QuantLib::Real accuracy_;

    //! Flat optionlet volatility before first optionlet fixing date
    bool flatFirstPeriod_;

    //! Volatility type for the underlying ATM cap floor volatility curve
    QuantLib::VolatilityType capFloorVolType_;

    //! The applicable shift if the underlying ATM cap floor volatility curve has shifted lognormal volatility
    QuantLib::Real capFloorVolDisplacement_;

    //! This optionlet structure's volatility type
    QuantLib::VolatilityType volatilityType_;
    
    //! This optionlet structure's shift if its volatility type is shifted lognormal
    QuantLib::Real displacement_;

    //! The interpolator
    Interpolator interpolator_;

    //! The bootstrapper
    Bootstrap<optionlet_curve> bootstrap_;

    //! The stripped optionlet curve
    boost::shared_ptr<optionlet_curve> curve_;

    //! Store the vector of ATM cap floor helpers that are used in the bootstrap
    typedef QuantLib::BootstrapHelper<QuantLib::OptionletVolatilityStructure> helper;
    std::vector<boost::shared_ptr<helper> > helpers_;

    //! Store the ATM cap floor curve quotes
    std::vector<boost::shared_ptr<QuantLib::SimpleQuote> > quotes_;

    //! Shared initialisation
    void initialise(const boost::shared_ptr<QuantLib::IborIndex>& index,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& discount);
};

template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap>
PiecewiseAtmOptionletCurve<Interpolator, TermVolInterpolator, Bootstrap>::PiecewiseAtmOptionletCurve(
    QuantLib::Natural settlementDays,
    const boost::shared_ptr<QuantExt::CapFloorTermVolCurve<TermVolInterpolator> >& cftvc,
    const boost::shared_ptr<QuantLib::IborIndex>& index,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
    QuantLib::Real accuracy,
    bool flatFirstPeriod,
    const QuantLib::VolatilityType capFloorVolType,
    const QuantLib::Real capFloorVolDisplacement,
    const boost::optional<QuantLib::VolatilityType> optionletVolType,
    const boost::optional<QuantLib::Real> optionletVolDisplacement,
    const Interpolator& i,
    const Bootstrap<optionlet_curve>& bootstrap)
    : QuantLib::OptionletVolatilityStructure(settlementDays, cftvc->calendar(), cftvc->businessDayConvention(), 
      cftvc->dayCounter()), cftvc_(cftvc), accuracy_(accuracy), flatFirstPeriod_(flatFirstPeriod), 
      capFloorVolType_(capFloorVolType), capFloorVolDisplacement_(capFloorVolDisplacement),
      volatilityType_(optionletVolType ? *optionletVolType : capFloorVolType),
      displacement_(optionletVolDisplacement ? *optionletVolDisplacement : 0.0),
      interpolator_(i), bootstrap_(bootstrap), helpers_(cftvc_->optionTenors().size()), 
      quotes_(cftvc_->optionTenors().size()) {

    initialise(index, discount);

    curve_ = boost::make_shared<optionlet_curve>(
        settlementDays, helpers_, cftvc_->calendar(), cftvc_->businessDayConvention(), cftvc_->dayCounter(), 
        volatilityType_, displacement_, flatFirstPeriod_, accuracy_, interpolator_, bootstrap_);
}

template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap>
PiecewiseAtmOptionletCurve<Interpolator, TermVolInterpolator, Bootstrap>::PiecewiseAtmOptionletCurve(
    const QuantLib::Date& settlementDate,
    const boost::shared_ptr<QuantExt::CapFloorTermVolCurve<TermVolInterpolator> >& cftvc,
    const boost::shared_ptr<QuantLib::IborIndex>& index,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
    QuantLib::Real accuracy,
    bool flatFirstPeriod,
    const QuantLib::VolatilityType capFloorVolType,
    const QuantLib::Real capFloorVolDisplacement,
    const boost::optional<QuantLib::VolatilityType> optionletVolType,
    const boost::optional<QuantLib::Real> optionletVolDisplacement,
    const Interpolator& i,
    const Bootstrap<optionlet_curve>& bootstrap)
    : QuantLib::OptionletVolatilityStructure(settlementDate, cftvc->calendar(), cftvc->businessDayConvention(),
      cftvc->dayCounter()), cftvc_(cftvc), accuracy_(accuracy), flatFirstPeriod_(flatFirstPeriod), 
      capFloorVolType_(capFloorVolType), capFloorVolDisplacement_(capFloorVolDisplacement),
      volatilityType_(optionletVolType ? *optionletVolType : capFloorVolType),
      displacement_(optionletVolDisplacement ? *optionletVolDisplacement : 0.0),
      interpolator_(i), bootstrap_(bootstrap), helpers_(cftvc_->optionTenors().size()), 
      quotes_(cftvc_->optionTenors().size()) {

    initialise(index, discount);

    curve_ = boost::make_shared<optionlet_curve>(
        settlementDate, helpers_, cftvc_->calendar(), cftvc_->businessDayConvention(), cftvc_->dayCounter(), 
        volatilityType_, displacement_, flatFirstPeriod_, accuracy_, interpolator_, bootstrap_);
}

template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap>
void PiecewiseAtmOptionletCurve<Interpolator, TermVolInterpolator, Bootstrap>::update() {
    
    LazyObject::update();

    if (this->moving_)
        this->updated_ = false;
}

template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap>
void PiecewiseAtmOptionletCurve<Interpolator, TermVolInterpolator, Bootstrap>::performCalculations() const {

    // Update the quotes from the cap floor term volatility surface
    for (QuantLib::Size i = 0; i < cftvc_->optionTenors().size(); i++) {
        quotes_[i]->setValue(cftvc_->volatility(cftvc_->optionTenors()[i], 0.01));
    }
}

template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap>
QuantLib::Date PiecewiseAtmOptionletCurve<Interpolator, TermVolInterpolator, Bootstrap>::maxDate() const {
    calculate();
    return curve_->maxDate();
}

template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap>
QuantLib::Rate PiecewiseAtmOptionletCurve<Interpolator, TermVolInterpolator, Bootstrap>::minStrike() const {
    calculate();
    return curve_->minStrike();
}

template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap>
QuantLib::Rate PiecewiseAtmOptionletCurve<Interpolator, TermVolInterpolator, Bootstrap>::maxStrike() const {
    calculate();
    return curve_->maxStrike();
}

template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap>
QuantLib::VolatilityType PiecewiseAtmOptionletCurve<Interpolator, TermVolInterpolator, Bootstrap>::volatilityType() const {
    return volatilityType_;
}

template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap>
QuantLib::Real PiecewiseAtmOptionletCurve<Interpolator, TermVolInterpolator, Bootstrap>::displacement() const {
    return displacement_;
}

template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap>
boost::shared_ptr<QuantLib::SmileSection> PiecewiseAtmOptionletCurve<Interpolator, TermVolInterpolator, Bootstrap>::
smileSectionImpl(QuantLib::Time optionTime) const {
    calculate();
    return curve_->smileSection(optionTime, true);
}

template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap>
QuantLib::Volatility PiecewiseAtmOptionletCurve<Interpolator, TermVolInterpolator, Bootstrap>::
volatilityImpl(QuantLib::Time optionTime, QuantLib::Rate strike) const {
    calculate();
    return curve_->volatility(optionTime, 0.01, true);
}

template <class Interpolator, class TermVolInterpolator, template <class> class Bootstrap>
void PiecewiseAtmOptionletCurve<Interpolator, TermVolInterpolator, Bootstrap>::initialise(
    const boost::shared_ptr<QuantLib::IborIndex>& index,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discount) {
    
    // Readability
    using QuantLib::Size;
    using QuantLib::SimpleQuote;
    using QuantLib::Period;
    using QuantLib::Handle;
    using QuantLib::Quote;
    using QuantLib::Rate;
    using QuantLib::Null;
    using QuantLib::Real;
    using std::vector;

    // Observe the underlying cap floor term volatility curve
    registerWith(cftvc_);

    // Initialise the quotes and helpers
    vector<Period> tenors = cftvc_->optionTenors();
    for (Size i = 0; i < tenors.size(); i++) {
        quotes_[i] = boost::make_shared<SimpleQuote>(cftvc_->volatility(tenors[i], 0.01));
        helpers_[i] = boost::make_shared<CapFloorHelper>(CapFloorHelper::Cap, tenors[i], Null<Real>(),
            Handle<Quote>(quotes_[i]), index, discount, CapFloorHelper::Volatility,
            capFloorVolType_, capFloorVolDisplacement_);
    }
}

}

#endif
