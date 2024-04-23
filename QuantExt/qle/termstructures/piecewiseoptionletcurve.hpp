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

/*! \file qle/termstructures/piecewiseoptionletcurve.hpp
    \brief One-dimensional curve of bootstrapped optionlet volatilities
*/

#ifndef quantext_piecewise_optionlet_curve_hpp
#define quantext_piecewise_optionlet_curve_hpp

#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/localbootstrap.hpp>
#include <ql/termstructures/yield/bootstraptraits.hpp>
#include <qle/termstructures/capfloorhelper.hpp>
#include <qle/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/optionletcurve.hpp>

namespace QuantExt {

//! \e Traits class that is needed for Bootstrap classes to work
struct OptionletTraits {

    //! Helper type
    typedef QuantLib::BootstrapHelper<QuantLib::OptionletVolatilityStructure> helper;

    //! Start date of the optionlet volatility term structure
    static QuantLib::Date initialDate(const QuantLib::OptionletVolatilityStructure* ovts) {
        return ovts->referenceDate();
    }

    //! The value at the reference date of the term structure
    static QuantLib::Real initialValue(const QuantLib::OptionletVolatilityStructure* ovts) { return 0.0; }

    //! Guesses
    template <class C> static QuantLib::Real guess(QuantLib::Size i, const C* c, bool validData, QuantLib::Size) {

        // Previous value
        if (validData) {
            return c->data()[i];
        }

        // First iteration, not sure if we can do any better here.
        if (i == 1) {
            if (c->volatilityType() == QuantLib::Normal) {
                return 0.0020;
            } else {
                return 0.20;
            }
        }

        // Flat extrapolation
        return c->data()[i - 1];
    }

    //! Minimum value after a given iteration. Lower bound for optionlet volatility is 0.
    template <class C>
    static QuantLib::Real minValueAfter(QuantLib::Size i, const C* c, bool validData, QuantLib::Size) {
        // Choose arbitrarily small positive number
        if (c->volatilityType() == QuantLib::Normal) {
            return 1e-8;
        } else {
            return 1e-4;
        }
    }

    //! Maximum value after a given iteration.
    template <class C>
    static QuantLib::Real maxValueAfter(QuantLib::Size i, const C* c, bool validData, QuantLib::Size) {
        // Choose large and reasonable positive number
        // Not sure if it makes sense to attempt to use value at previous pillar
        if (c->volatilityType() == QuantLib::Normal) {
            return 0.50;
        } else {
            return 5;
        }
    }

    //! Root finding update
    static void updateGuess(std::vector<QuantLib::Real>& data, QuantLib::Real vol, QuantLib::Size i) { data[i] = vol; }

    //! Maximum number of iterations to perform in search for root
    static QuantLib::Size maxIterations() { return 100; }
};

template <class Interpolator, template <class> class Bootstrap = QuantExt::IterativeBootstrap>
class PiecewiseOptionletCurve : public InterpolatedOptionletCurve<Interpolator>, public QuantLib::LazyObject {

public:
    typedef PiecewiseOptionletCurve<Interpolator, Bootstrap> this_curve;
    typedef QuantLib::BootstrapHelper<QuantLib::OptionletVolatilityStructure> helper;

    //! Bootstrap needs these typedefs
    typedef OptionletTraits traits_type;
    typedef Interpolator interpolator_type;

    //! \name Constructors
    //@{
    PiecewiseOptionletCurve(const QuantLib::Date& referenceDate,
                            const std::vector<QuantLib::ext::shared_ptr<helper> >& instruments,
                            const QuantLib::Calendar& calendar, QuantLib::BusinessDayConvention bdc,
                            const QuantLib::DayCounter& dayCounter,
                            QuantLib::VolatilityType volatilityType = QuantLib::Normal,
                            QuantLib::Real displacement = 0.0, bool flatFirstPeriod = true,
                            const Interpolator& i = Interpolator(),
                            const Bootstrap<this_curve>& bootstrap = Bootstrap<this_curve>());

    PiecewiseOptionletCurve(QuantLib::Natural settlementDays,
                            const std::vector<QuantLib::ext::shared_ptr<helper> >& instruments,
                            const QuantLib::Calendar& calendar, QuantLib::BusinessDayConvention bdc,
                            const QuantLib::DayCounter& dayCounter,
                            QuantLib::VolatilityType volatilityType = QuantLib::Normal,
                            QuantLib::Real displacement = 0.0, bool flatFirstPeriod = true,
                            const Interpolator& i = Interpolator(),
                            const Bootstrap<this_curve>& bootstrap = Bootstrap<this_curve>());
    //@}

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    //@}

    //! \name InterpolatedOptionletCurve interface
    //@{
    const std::vector<QuantLib::Time>& times() const;
    const std::vector<QuantLib::Date>& dates() const;
    const std::vector<QuantLib::Real>& volatilities() const;
    std::vector<std::pair<QuantLib::Date, QuantLib::Real> > nodes() const;
    //@}

    //! \name Observer interface
    //@{
    void update() override;
    //@}

private:
    typedef InterpolatedOptionletCurve<Interpolator> base_curve;

    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

    //! \name OptionletVolatilityStructure interface
    //@{
    QuantLib::Real volatilityImpl(QuantLib::Time optionTime, QuantLib::Rate strike) const override;
    //@}

    //! Vector of helper instruments to be matched
    std::vector<QuantLib::ext::shared_ptr<helper> > instruments_;

    //! Accuracy of the match
    QuantLib::Real accuracy_;

    // Bootstrapper classes are declared as friend to manipulate the curve data
    friend class Bootstrap<this_curve>;
    friend class BootstrapError<this_curve>;
    friend class PenaltyFunction<this_curve>;

    Bootstrap<this_curve> bootstrap_;
};

template <class Interpolator, template <class> class Bootstrap>
PiecewiseOptionletCurve<Interpolator, Bootstrap>::PiecewiseOptionletCurve(
    const QuantLib::Date& referenceDate, const std::vector<QuantLib::ext::shared_ptr<helper> >& instruments,
    const QuantLib::Calendar& calendar, QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dayCounter,
    QuantLib::VolatilityType volatilityType, QuantLib::Real displacement, bool flatFirstPeriod, const Interpolator& i,
    const Bootstrap<this_curve>& bootstrap)
    : base_curve(referenceDate, calendar, bdc, dayCounter, volatilityType, displacement, flatFirstPeriod, i),
      instruments_(instruments), accuracy_(1e-12), bootstrap_(bootstrap) {
    bootstrap_.setup(this);
}

template <class Interpolator, template <class> class Bootstrap>
PiecewiseOptionletCurve<Interpolator, Bootstrap>::PiecewiseOptionletCurve(
    QuantLib::Natural settlementDays, const std::vector<QuantLib::ext::shared_ptr<helper> >& instruments,
    const QuantLib::Calendar& calendar, QuantLib::BusinessDayConvention bdc, const QuantLib::DayCounter& dayCounter,
    QuantLib::VolatilityType volatilityType, QuantLib::Real displacement, bool flatFirstPeriod, const Interpolator& i,
    const Bootstrap<this_curve>& bootstrap)
    : base_curve(settlementDays, calendar, bdc, dayCounter, volatilityType, displacement, flatFirstPeriod, i),
      instruments_(instruments), accuracy_(1e-12), bootstrap_(bootstrap) {
    bootstrap_.setup(this);
}

template <class Interpolator, template <class> class Bootstrap>
inline QuantLib::Date PiecewiseOptionletCurve<Interpolator, Bootstrap>::maxDate() const {
    calculate();
    return base_curve::maxDate();
}

template <class Interpolator, template <class> class Bootstrap>
inline const std::vector<QuantLib::Time>& PiecewiseOptionletCurve<Interpolator, Bootstrap>::times() const {
    calculate();
    return base_curve::times();
}

template <class Interpolator, template <class> class Bootstrap>
inline const std::vector<QuantLib::Date>& PiecewiseOptionletCurve<Interpolator, Bootstrap>::dates() const {
    calculate();
    return base_curve::dates();
}

template <class Interpolator, template <class> class Bootstrap>
inline const std::vector<QuantLib::Real>& PiecewiseOptionletCurve<Interpolator, Bootstrap>::volatilities() const {
    calculate();
    return base_curve::volatilities();
}

template <class Interpolator, template <class> class Bootstrap>
inline std::vector<std::pair<QuantLib::Date, QuantLib::Real> >
PiecewiseOptionletCurve<Interpolator, Bootstrap>::nodes() const {
    calculate();
    return base_curve::nodes();
}

template <class Interpolator, template <class> class Bootstrap>
inline void PiecewiseOptionletCurve<Interpolator, Bootstrap>::update() {

    // Derives from InterpolatedOptionletCurve and LazyObject, both of which are Observers and have their own
    // implementation of the virtual update() method.

    // Call LazyObject::update() to notify Observers but only when this PiecewiseOptionletCurve has been "calculated"
    // and it has not been "frozen"
    LazyObject::update();

    // Do not want to call TermStructure::update() here because it will notify all Observers regardless of whether
    // this PiecewiseOptionletCurve's "calculated" or "frozen" status i.e. defeating the purpose of LazyObject

    // We do not want to miss changes in Settings::instance().evaluationDate() if this TermStructure has a floating
    // reference date so we make sure that TermStructure::updated_ is set to false.
    if (this->moving_)
        this->updated_ = false;
}

template <class Interpolator, template <class> class Bootstrap>
inline QuantLib::Real PiecewiseOptionletCurve<Interpolator, Bootstrap>::volatilityImpl(QuantLib::Time optionTime,
                                                                                       QuantLib::Rate strike) const {
    calculate();
    return base_curve::volatilityImpl(optionTime, strike);
}

template <class Interpolator, template <class> class Bootstrap>
inline void PiecewiseOptionletCurve<Interpolator, Bootstrap>::performCalculations() const {
    // Perform the bootstrap
    bootstrap_.calculate();
}

} // namespace QuantExt

#endif
