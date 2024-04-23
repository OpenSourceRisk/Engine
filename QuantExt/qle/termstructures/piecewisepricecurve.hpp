/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/piecewisepricecurve.hpp
    \brief Piecewise interpolated price term structure
    \ingroup termstructures
*/

#ifndef quantext_piecewise_price_hpp
#define quantext_piecewise_price_hpp

#include <qle/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/pricecurve.hpp>
#include <ql/termstructures/localbootstrap.hpp>
#include <ql/termstructures/yield/bootstraptraits.hpp>
#include <ql/patterns/lazyobject.hpp>

namespace QuantExt {

//! Traits class that is needed for Bootstrap classes to work
struct PriceTraits {

    //! Helper type
    typedef QuantLib::BootstrapHelper<PriceTermStructure> helper;

    //! Start date of the term structure
    static QuantLib::Date initialDate(const PriceTermStructure* ts) {
        return ts->referenceDate();
    }

    //! Dummy value at reference date. Updated below along with first guess.
    static QuantLib::Real initialValue(const PriceTermStructure* ts) {
        return 1.0;
    }

    //! Guesses
    template <class C>
    static QuantLib::Real guess(QuantLib::Size i, const C* c, bool validData, QuantLib::Size j) {
        // Take the market value from the helper. Here, we rely on the all the helpers being alive and sorted.
        // We will check this in the PiecewisePriceCurve constructor.
        return c->instrument(i-1)->quote()->value();
    }

    //! Minimum value after a given iteration.
    template <class C>
    static QuantLib::Real minValueAfter(QuantLib::Size i, const C* c, bool validData, QuantLib::Size j) {
        // Bounds around the guess. Can widen these and try again within the bootstrap.
        Real g = guess(i, c, validData, j);
        return g < 0.0 ? g * 5.0 / 4.0 : g * 3.0 / 4.0;
    }

    //! Maximum value after a given iteration.
    template <class C>
    static QuantLib::Real maxValueAfter(QuantLib::Size i, const C* c, bool validData, QuantLib::Size j) {
        // Bounds around the guess. Can widen these and try again within the bootstrap.
        Real g = guess(i, c, validData, j);
        return g < 0.0 ? g * 3.0 / 4.0 : g * 5.0 / 4.0;
    }

    //! Root finding update
    static void updateGuess(std::vector<QuantLib::Real>& data, QuantLib::Real price, QuantLib::Size i) {
        data[i] = price;
        // Update the value at the reference date also.
        if (i == 1)
            data[0] = price;
    }

    //! Maximum number of iterations to perform in search for root
    static QuantLib::Size maxIterations() { return 100; }
};

//! Piecewise price term structure
/*! This term structure is bootstrapped on a number of instruments which are passed as a vector of handles to
    PriceHelper instances. Their maturities mark the boundaries of the interpolated segments.

    Each segment is determined sequentially starting from the earliest period to the latest and is chosen so that 
    the instrument whose maturity marks the end of such segment is correctly repriced on the curve.

    \warning The bootstrapping algorithm raises an exception if any two instruments have the same maturity date.

    \ingroup termstructures
*/
template <class Interpolator, template <class> class Bootstrap = IterativeBootstrap>
class PiecewisePriceCurve : public InterpolatedPriceCurve<Interpolator> {

private:
    typedef InterpolatedPriceCurve<Interpolator> base_curve;

public:
    typedef PiecewisePriceCurve<Interpolator, Bootstrap> this_curve;
    typedef QuantLib::BootstrapHelper<PriceTermStructure> helper;
    typedef PriceTraits traits_type;
    typedef Interpolator interpolator_type;

    //! \name Constructors
    //@{
    PiecewisePriceCurve(const QuantLib::Date& referenceDate,
        const std::vector<QuantLib::ext::shared_ptr<helper> >& instruments,
        const QuantLib::DayCounter& dayCounter,
        const QuantLib::Currency& currency,
        const Interpolator& i = Interpolator(),
        const Bootstrap<this_curve>& bootstrap = Bootstrap<this_curve>());
    //@}

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override;
    QuantLib::Time maxTime() const override;
    //@}

    //! \name PriceTermStructure interface
    //@{
    QuantLib::Time minTime() const override;
    std::vector<QuantLib::Date> pillarDates() const override;
    //@}

    //! \name InterpolatedPriceCurve interface
    //@{
    const std::vector<QuantLib::Time>& times() const;
    const std::vector<QuantLib::Real>& prices() const;
    //@}

    //! Return the i-th instrument
    const QuantLib::ext::shared_ptr<helper>& instrument(QuantLib::Size i) const;

private:
    //! \name LazyObject interface
    //@{
    void performCalculations() const override;
    //@}

    //! \name PriceTermStructure implementation
    //@{
    QuantLib::Real priceImpl(QuantLib::Time t) const override;
    //@}

    std::vector<QuantLib::ext::shared_ptr<helper> > instruments_;
    Real accuracy_;

    friend class Bootstrap<this_curve>;
    friend class BootstrapError<this_curve>;
    friend class PenaltyFunction<this_curve>;
    Bootstrap<this_curve> bootstrap_;
};

template <class Interpolator, template <class> class Bootstrap>
PiecewisePriceCurve<Interpolator, Bootstrap>::PiecewisePriceCurve(
    const QuantLib::Date& referenceDate,
    const std::vector<QuantLib::ext::shared_ptr<helper> >& instruments,
    const QuantLib::DayCounter& dayCounter,
    const QuantLib::Currency& currency,
    const Interpolator& i,
    const Bootstrap<this_curve>& bootstrap)
    : base_curve(referenceDate, dayCounter, currency, i), instruments_(instruments),
      accuracy_(1e-12), bootstrap_(bootstrap) {

    // Ensure that the instruments are sorted and that they are all alive i.e. pillar > reference date.
    // Need this because of the way that we have set up PriceTraits.
    std::sort(instruments_.begin(), instruments_.end(), QuantLib::detail::BootstrapHelperSorter());

    auto it = std::find_if(instruments_.begin(), instruments_.end(), 
        [&referenceDate](const QuantLib::ext::shared_ptr<helper>& inst) { return inst->pillarDate() > referenceDate; });
    QL_REQUIRE(it != instruments_.end(), "PiecewisePriceCurve: all instruments are expired.");
    if (it != instruments_.begin()) {
        instruments_.erase(instruments_.begin(), it);
    }

    bootstrap_.setup(this);
}

template <class Interpolator, template <class> class Bootstrap>
QuantLib::Date PiecewisePriceCurve<Interpolator, Bootstrap>::maxDate() const {
    this->calculate();
    return base_curve::maxDate();
}

template <class Interpolator, template <class> class Bootstrap>
QuantLib::Time PiecewisePriceCurve<Interpolator, Bootstrap>::maxTime() const {
    this->calculate();
    return base_curve::maxTime();
}

template <class Interpolator, template <class> class Bootstrap>
QuantLib::Time PiecewisePriceCurve<Interpolator, Bootstrap>::minTime() const {
    this->calculate();
    return base_curve::minTime();
}

template <class Interpolator, template <class> class Bootstrap>
std::vector<QuantLib::Date> PiecewisePriceCurve<Interpolator, Bootstrap>::pillarDates() const {
    this->calculate();
    return base_curve::pillarDates();
}

template <class Interpolator, template <class> class Bootstrap>
const std::vector<QuantLib::Time>& PiecewisePriceCurve<Interpolator, Bootstrap>::times() const {
    this->calculate();
    return base_curve::times();
}

template <class Interpolator, template <class> class Bootstrap>
const std::vector<QuantLib::Real>& PiecewisePriceCurve<Interpolator, Bootstrap>::prices() const {
    this->calculate();
    return base_curve::prices();
}

template <class Interpolator, template <class> class Bootstrap>
const QuantLib::ext::shared_ptr<QuantLib::BootstrapHelper<PriceTermStructure> >& 
PiecewisePriceCurve<Interpolator, Bootstrap>::instrument(QuantLib::Size i) const {
    QL_REQUIRE(i < instruments_.size(), "Index (" << i << ") greater than the number of instruments (" <<
        instruments_.size() << ").");
    return instruments_[i];
}

template <class Interpolator, template <class> class Bootstrap>
void PiecewisePriceCurve<Interpolator, Bootstrap>::performCalculations() const {
    bootstrap_.calculate();
    base_curve::performCalculations();
}

template <class Interpolator, template <class> class Bootstrap>
QuantLib::Real PiecewisePriceCurve<Interpolator, Bootstrap>::priceImpl(QuantLib::Time t) const {
    this->calculate();
    return base_curve::priceImpl(t);
}

}

#endif
