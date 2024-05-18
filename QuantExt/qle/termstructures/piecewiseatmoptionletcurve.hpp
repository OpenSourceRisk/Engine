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

#include <qle/termstructures/capfloortermvolcurve.hpp>
#include <qle/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/piecewiseoptionletcurve.hpp>

namespace QuantExt {

/*! Helper class to strip caplet/floorlet volatilities from the cap floor term volatilities of a
    CapFloorTermVolCurve.
*/
template <class Interpolator, template <class> class Bootstrap = QuantExt::IterativeBootstrap>
class PiecewiseAtmOptionletCurve : public QuantLib::OptionletVolatilityStructure, public QuantLib::LazyObject {

public:
    typedef typename PiecewiseOptionletCurve<Interpolator, Bootstrap>::this_curve optionlet_curve;

    PiecewiseAtmOptionletCurve(QuantLib::Natural settlementDays, const QuantLib::ext::shared_ptr<CapFloorTermVolCurve>& cftvc,
                               const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index,
                               const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
                               bool flatFirstPeriod = true,
                               const QuantLib::VolatilityType capFloorVolType = QuantLib::ShiftedLognormal,
                               const QuantLib::Real capFloorVolDisplacement = 0.0,
                               const boost::optional<QuantLib::VolatilityType> optionletVolType = boost::none,
                               const boost::optional<QuantLib::Real> optionletVolDisplacement = boost::none,
                               bool interpOnOptionlets = true, const Interpolator& i = Interpolator(),
                               const Bootstrap<optionlet_curve>& bootstrap = Bootstrap<optionlet_curve>());

    PiecewiseAtmOptionletCurve(const QuantLib::Date& referenceDate,
                               const QuantLib::ext::shared_ptr<CapFloorTermVolCurve>& cftvc,
                               const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index,
                               const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
                               bool flatFirstPeriod = true,
                               const QuantLib::VolatilityType capFloorVolType = QuantLib::ShiftedLognormal,
                               const QuantLib::Real capFloorVolDisplacement = 0.0,
                               const boost::optional<QuantLib::VolatilityType> optionletVolType = boost::none,
                               const boost::optional<QuantLib::Real> optionletVolDisplacement = boost::none,
                               bool interpOnOptionlets = true, const Interpolator& i = Interpolator(),
                               const Bootstrap<optionlet_curve>& bootstrap = Bootstrap<optionlet_curve>());

    //! \name Inspectors
    //@{
    //! Volatility type for the underlying ATM cap floor curve
    QuantLib::VolatilityType capFloorVolType() const { return capFloorVolType_; }

    //! The applicable shift if the underlying ATM cap floor curve has shifted lognormal volatility
    QuantLib::Real capFloorVolDisplacement() const { return capFloorVolDisplacement_; }

    //! \name Observer interface
    //@{
    void update() override;
    //@}

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    QuantLib::Rate minStrike() const override;
    QuantLib::Rate maxStrike() const override;
    //@}

    //! \name OptionletVolatilityStructure interface
    //@{
    QuantLib::VolatilityType volatilityType() const override;
    QuantLib::Real displacement() const override;
    //@}

    //! The underlying optionlet curve
    QuantLib::ext::shared_ptr<optionlet_curve> curve() const;

protected:
    //! \name OptionletVolatilityStructure interface
    //@{
    QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(QuantLib::Time optionTime) const override;
    QuantLib::Volatility volatilityImpl(QuantLib::Time optionTime, QuantLib::Rate strike) const override;
    //@}

private:
    //! The underlying ATM cap floor term volatility curve
    QuantLib::ext::shared_ptr<CapFloorTermVolCurve> cftvc_;

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

    //! True to interpolate on optionlet volatilities, false to interpolate on cap floor term volatilities
    bool interpOnOptionlets_;

    //! The interpolator
    Interpolator interpolator_;

    //! The bootstrapper
    Bootstrap<optionlet_curve> bootstrap_;

    //! The stripped optionlet curve
    QuantLib::ext::shared_ptr<optionlet_curve> curve_;

    //! Store the helper tenors
    std::vector<QuantLib::Period> tenors_;

    //! Store the vector of ATM cap floor helpers that are used in the bootstrap
    typedef QuantLib::BootstrapHelper<QuantLib::OptionletVolatilityStructure> helper;
    std::vector<QuantLib::ext::shared_ptr<helper> > helpers_;

    //! Store the ATM cap floor curve quotes
    std::vector<QuantLib::ext::shared_ptr<QuantLib::SimpleQuote> > quotes_;

    //! Shared initialisation
    void initialise(const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index,
                    const QuantLib::Handle<QuantLib::YieldTermStructure>& discount);
};

template <class Interpolator, template <class> class Bootstrap>
PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::PiecewiseAtmOptionletCurve(
    QuantLib::Natural settlementDays, const QuantLib::ext::shared_ptr<CapFloorTermVolCurve>& cftvc,
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index, const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
    bool flatFirstPeriod, const QuantLib::VolatilityType capFloorVolType, const QuantLib::Real capFloorVolDisplacement,
    const boost::optional<QuantLib::VolatilityType> optionletVolType,
    const boost::optional<QuantLib::Real> optionletVolDisplacement, bool interpOnOptionlets, const Interpolator& i,
    const Bootstrap<optionlet_curve>& bootstrap)
    : QuantLib::OptionletVolatilityStructure(settlementDays, cftvc->calendar(), cftvc->businessDayConvention(),
                                             cftvc->dayCounter()),
      cftvc_(cftvc), flatFirstPeriod_(flatFirstPeriod), capFloorVolType_(capFloorVolType),
      capFloorVolDisplacement_(capFloorVolDisplacement),
      volatilityType_(optionletVolType ? *optionletVolType : capFloorVolType),
      displacement_(optionletVolDisplacement ? *optionletVolDisplacement : 0.0),
      interpOnOptionlets_(interpOnOptionlets), interpolator_(i), bootstrap_(bootstrap), tenors_(cftvc_->optionTenors()),
      helpers_(tenors_.size()), quotes_(tenors_.size()) {

    initialise(index, discount);

    curve_ = QuantLib::ext::make_shared<optionlet_curve>(settlementDays, helpers_, cftvc_->calendar(),
                                                 cftvc_->businessDayConvention(), cftvc_->dayCounter(), volatilityType_,
                                                 displacement_, flatFirstPeriod_, interpolator_, bootstrap_);
}

template <class Interpolator, template <class> class Bootstrap>
PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::PiecewiseAtmOptionletCurve(
    const QuantLib::Date& referenceDate, const QuantLib::ext::shared_ptr<CapFloorTermVolCurve>& cftvc,
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index, const QuantLib::Handle<QuantLib::YieldTermStructure>& discount,
    bool flatFirstPeriod, const QuantLib::VolatilityType capFloorVolType, const QuantLib::Real capFloorVolDisplacement,
    const boost::optional<QuantLib::VolatilityType> optionletVolType,
    const boost::optional<QuantLib::Real> optionletVolDisplacement, bool interpOnOptionlets, const Interpolator& i,
    const Bootstrap<optionlet_curve>& bootstrap)
    : QuantLib::OptionletVolatilityStructure(referenceDate, cftvc->calendar(), cftvc->businessDayConvention(),
                                             cftvc->dayCounter()),
      cftvc_(cftvc), flatFirstPeriod_(flatFirstPeriod), capFloorVolType_(capFloorVolType),
      capFloorVolDisplacement_(capFloorVolDisplacement),
      volatilityType_(optionletVolType ? *optionletVolType : capFloorVolType),
      displacement_(optionletVolDisplacement ? *optionletVolDisplacement : 0.0),
      interpOnOptionlets_(interpOnOptionlets), interpolator_(i), bootstrap_(bootstrap), tenors_(cftvc_->optionTenors()),
      helpers_(tenors_.size()), quotes_(tenors_.size()) {

    initialise(index, discount);

    curve_ = QuantLib::ext::make_shared<optionlet_curve>(referenceDate, helpers_, cftvc_->calendar(),
                                                 cftvc_->businessDayConvention(), cftvc_->dayCounter(), volatilityType_,
                                                 displacement_, flatFirstPeriod_, interpolator_, bootstrap_);
}

template <class Interpolator, template <class> class Bootstrap>
void PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::update() {

    LazyObject::update();

    if (this->moving_)
        this->updated_ = false;
}

template <class Interpolator, template <class> class Bootstrap>
void PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::performCalculations() const {

    // Update the quotes from the cap floor term volatility surface
    for (QuantLib::Size i = 0; i < tenors_.size(); i++) {
        quotes_[i]->setValue(cftvc_->volatility(tenors_[i], 0.01));
    }
}

template <class Interpolator, template <class> class Bootstrap>
QuantLib::Date PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::maxDate() const {
    calculate();
    return curve_->maxDate();
}

template <class Interpolator, template <class> class Bootstrap>
QuantLib::Rate PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::minStrike() const {
    calculate();
    return curve_->minStrike();
}

template <class Interpolator, template <class> class Bootstrap>
QuantLib::Rate PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::maxStrike() const {
    calculate();
    return curve_->maxStrike();
}

template <class Interpolator, template <class> class Bootstrap>
QuantLib::VolatilityType PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::volatilityType() const {
    return volatilityType_;
}

template <class Interpolator, template <class> class Bootstrap>
QuantLib::Real PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::displacement() const {
    return displacement_;
}

template <class Interpolator, template <class> class Bootstrap>
QuantLib::ext::shared_ptr<typename PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::optionlet_curve>
PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::curve() const {
    calculate();
    return curve_;
}

template <class Interpolator, template <class> class Bootstrap>
QuantLib::ext::shared_ptr<QuantLib::SmileSection>
PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::smileSectionImpl(QuantLib::Time optionTime) const {
    calculate();
    return curve_->smileSection(optionTime, true);
}

template <class Interpolator, template <class> class Bootstrap>
QuantLib::Volatility PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::volatilityImpl(QuantLib::Time optionTime,
                                                                                         QuantLib::Rate strike) const {
    calculate();
    return curve_->volatility(optionTime, 0.01, true);
}

template <class Interpolator, template <class> class Bootstrap>
void PiecewiseAtmOptionletCurve<Interpolator, Bootstrap>::initialise(
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discount) {

    // Readability
    using QuantLib::Handle;
    using QuantLib::Null;
    using QuantLib::Period;
    using QuantLib::Quote;
    using QuantLib::Rate;
    using QuantLib::Real;
    using QuantLib::SimpleQuote;
    using QuantLib::Size;
    using std::vector;

    // Observe the underlying cap floor term volatility curve
    registerWith(cftvc_);

    // If the term structure is not moving, ensure that the cap floor helpers are not moving and are set up with the
    // correct effective date relative to the reference date. Mimic logic in MakeCapFloor here.
    Date effectiveDate;
    if (!this->moving_) {
        Calendar cal = index->fixingCalendar();
        Date ref = referenceDate();
        ref = cal.adjust(ref);
        effectiveDate = cal.advance(ref, index->fixingDays() * Days);
    }

    // Number of tenors, helpers and quotes depends on whether we are interpolating on optionlet volatilities or
    // cap floor term volatilities
    if (!interpOnOptionlets_) {

        // We are interpolating on the cap floor term volatilities
        Period indexTenor = index->tenor();
        Period maxCapFloorTenor = tenors_.back();

        // Add tenor of first cap floor - 2 x index tenor since first optionlet is excluded
        tenors_.clear();
        tenors_.push_back(indexTenor + indexTenor);
        QL_REQUIRE(maxCapFloorTenor >= tenors_.back(),
                   "First cap floor tenor, " << tenors_.back()
                                             << ", is greater than cap floor term vol surface's max tenor, "
                                             << maxCapFloorTenor);

        // Add all term cap floor instruments up to max tenor of term vol surface
        Period nextTenor = tenors_.back() + indexTenor;
        while (nextTenor <= maxCapFloorTenor) {
            tenors_.push_back(nextTenor);
            nextTenor += indexTenor;
        }

        // Resize the helpers and quotes vectors
        quotes_.resize(tenors_.size());
        helpers_.resize(tenors_.size());
    }

    // Initialise the quotes and helpers
    for (Size i = 0; i < tenors_.size(); i++) {
        quotes_[i] = QuantLib::ext::make_shared<SimpleQuote>(cftvc_->volatility(tenors_[i], 0.01));
        helpers_[i] = QuantLib::ext::make_shared<CapFloorHelper>(
            CapFloorHelper::Cap, tenors_[i], Null<Real>(), Handle<Quote>(quotes_[i]), index, discount, this->moving_,
            effectiveDate, CapFloorHelper::Volatility, capFloorVolType_, capFloorVolDisplacement_);
    }
}

} // namespace QuantExt

#endif
