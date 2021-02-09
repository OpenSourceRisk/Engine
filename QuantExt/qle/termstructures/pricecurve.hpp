/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/pricecurve.hpp
    \brief Interpolated price curve
*/

#ifndef quantext_price_curve_hpp
#define quantext_price_curve_hpp

#include <boost/algorithm/cxx11/is_sorted.hpp>

#include <qle/termstructures/pricetermstructure.hpp>

#include <ql/currency.hpp>
#include <ql/math/comparison.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

namespace QuantExt {

//! Interpolated price curve
/*! Class representing a curve of projected prices in the future.

    \warning for consistency, if curve is constructed by inferring times from dates
             using a given day counter, pass the same day counter to the constructor

    \ingroup termstructures
*/
template <class Interpolator>
class InterpolatedPriceCurve : public PriceTermStructure,
                               public QuantLib::LazyObject,
                               protected QuantLib::InterpolatedCurve<Interpolator> {
public:
    //! \name Constructors
    //@{
    //! Curve constructed from periods and prices. No conventions are applied in getting to a date from a period.
    InterpolatedPriceCurve(const std::vector<QuantLib::Period>& tenors, const std::vector<QuantLib::Real>& prices,
                           const QuantLib::DayCounter& dc, const QuantLib::Currency& currency,
                           const Interpolator& interpolator = Interpolator());

    //! Curve constructed from periods and quotes. No conventions are applied in getting to a date from a period.
    InterpolatedPriceCurve(const std::vector<QuantLib::Period>& tenors,
                           const std::vector<QuantLib::Handle<QuantLib::Quote> >& quotes,
                           const QuantLib::DayCounter& dc, const QuantLib::Currency& currency,
                           const Interpolator& interpolator = Interpolator());

    //! Curve constructed from dates and prices
    InterpolatedPriceCurve(const QuantLib::Date& referenceDate, const std::vector<QuantLib::Date>& dates,
                           const std::vector<QuantLib::Real>& prices, const QuantLib::DayCounter& dc,
                           const QuantLib::Currency& currency, const Interpolator& interpolator = Interpolator());

    //! Curve constructed from dates and quotes
    InterpolatedPriceCurve(const QuantLib::Date& referenceDate, const std::vector<QuantLib::Date>& dates,
                           const std::vector<QuantLib::Handle<QuantLib::Quote> >& quotes,
                           const QuantLib::DayCounter& dc, const QuantLib::Currency& currency,
                           const Interpolator& interpolator = Interpolator());
    //@}

    //! \name Observer interface
    //@{
    void update();
    //@}

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const;
    QuantLib::Time maxTime() const;
    //@}

    //! \name PriceTermStructure interface
    //@{
    QuantLib::Time minTime() const;
    std::vector<QuantLib::Date> pillarDates() const;
    const QuantLib::Currency& currency() const { return currency_; }
    //@}

    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Time>& times() const { return this->times_; }
    const std::vector<QuantLib::Real>& prices() const { return this->data_; }
    //@}

protected:
    //! Used by PiecewisePriceCurve
    InterpolatedPriceCurve(const QuantLib::Date& referenceDate,
        const QuantLib::DayCounter& dc, const QuantLib::Currency& currency,
        const Interpolator& interpolator = Interpolator());

    //! \name LazyObject interface
    //@{
    void performCalculations() const;
    //@}

    //! \name PriceTermStructure implementation
    //@{
    QuantLib::Real priceImpl(QuantLib::Time t) const;
    //@}

    mutable std::vector<QuantLib::Date> dates_;

private:
    const QuantLib::Currency currency_;
    std::vector<QuantLib::Handle<QuantLib::Quote> > quotes_;
    std::vector<QuantLib::Period> tenors_;

    void initialise();
    void populateDatesFromTenors() const;
    void convertDatesToTimes();
    void getPricesFromQuotes() const;
};

template <class Interpolator>
InterpolatedPriceCurve<Interpolator>::InterpolatedPriceCurve(const std::vector<QuantLib::Period>& tenors,
                                                             const std::vector<QuantLib::Real>& prices,
                                                             const QuantLib::DayCounter& dc,
                                                             const QuantLib::Currency& currency,
                                                             const Interpolator& interpolator)
    : PriceTermStructure(0, QuantLib::NullCalendar(), dc), QuantLib::InterpolatedCurve<Interpolator>(
                                                               std::vector<QuantLib::Time>(tenors.size()), prices,
                                                               interpolator),
      dates_(tenors.size()), currency_(currency), tenors_(tenors) {

    QL_REQUIRE(boost::algorithm::is_sorted(tenors_.begin(), tenors_.end()), "Tenors must be sorted");
    populateDatesFromTenors();
    initialise();
}

template <class Interpolator>
InterpolatedPriceCurve<Interpolator>::InterpolatedPriceCurve(
    const std::vector<QuantLib::Period>& tenors, const std::vector<QuantLib::Handle<QuantLib::Quote> >& quotes,
    const QuantLib::DayCounter& dc, const QuantLib::Currency& currency, const Interpolator& interpolator)
    : PriceTermStructure(0, QuantLib::NullCalendar(), dc), QuantLib::InterpolatedCurve<Interpolator>(
                                                               std::vector<QuantLib::Time>(tenors.size()),
                                                               std::vector<QuantLib::Real>(quotes.size()),
                                                               interpolator),
      dates_(tenors.size()), currency_(currency), quotes_(quotes), tenors_(tenors) {

    QL_REQUIRE(boost::algorithm::is_sorted(tenors_.begin(), tenors_.end()), "Tenors must be sorted");
    populateDatesFromTenors();
    initialise();

    // Observe the quotes
    for (QuantLib::Size i = 0; i < quotes_.size(); ++i) {
        registerWith(quotes[i]);
    }
}

template <class Interpolator>
InterpolatedPriceCurve<Interpolator>::InterpolatedPriceCurve(const QuantLib::Date& referenceDate,
                                                             const std::vector<QuantLib::Date>& dates,
                                                             const std::vector<QuantLib::Real>& prices,
                                                             const QuantLib::DayCounter& dc,
                                                             const QuantLib::Currency& currency,
                                                             const Interpolator& interpolator)
    : PriceTermStructure(referenceDate, QuantLib::NullCalendar(), dc), QuantLib::InterpolatedCurve<Interpolator>(
                                                                           std::vector<QuantLib::Time>(dates.size()),
                                                                           prices, interpolator),
      dates_(dates), currency_(currency) {

    convertDatesToTimes();
    initialise();
}

template <class Interpolator>
InterpolatedPriceCurve<Interpolator>::InterpolatedPriceCurve(
    const QuantLib::Date& referenceDate, const std::vector<QuantLib::Date>& dates,
    const std::vector<QuantLib::Handle<QuantLib::Quote> >& quotes, const QuantLib::DayCounter& dc,
    const QuantLib::Currency& currency, const Interpolator& interpolator)
    : PriceTermStructure(referenceDate, QuantLib::NullCalendar(), dc), QuantLib::InterpolatedCurve<Interpolator>(
                                                                           std::vector<QuantLib::Time>(dates.size()),
                                                                           std::vector<QuantLib::Real>(quotes.size()),
                                                                           interpolator),
      dates_(dates), currency_(currency), quotes_(quotes) {

    convertDatesToTimes();
    initialise();

    // Observe the quotes
    for (QuantLib::Size i = 0; i < quotes_.size(); ++i) {
        registerWith(quotes[i]);
    }
}

template <class Interpolator>
InterpolatedPriceCurve<Interpolator>::InterpolatedPriceCurve(
    const QuantLib::Date& referenceDate, const QuantLib::DayCounter& dc,
    const QuantLib::Currency& currency, const Interpolator& interpolator)
    : PriceTermStructure(referenceDate, QuantLib::NullCalendar(), dc),
      QuantLib::InterpolatedCurve<Interpolator>(interpolator), currency_(currency) {}

template <class Interpolator> void InterpolatedPriceCurve<Interpolator>::update() {

    QuantLib::LazyObject::update();

    // TermStructure::update() update part
    if (moving_) {
        updated_ = false;
    }
}

template <class Interpolator> void InterpolatedPriceCurve<Interpolator>::performCalculations() const {
    // Calculations need to be performed if the curve is tenor based
    if (!tenors_.empty()) {
        populateDatesFromTenors();
        this->interpolation_.update();
    }

    // Calculations need to be performed if the curve depends on quotes
    if (!quotes_.empty()) {
        getPricesFromQuotes();
        this->interpolation_.update();
    }
}

template <class Interpolator> QuantLib::Date InterpolatedPriceCurve<Interpolator>::maxDate() const {
    calculate();
    return dates_.back();
}

template <class Interpolator> QuantLib::Time InterpolatedPriceCurve<Interpolator>::maxTime() const {
    calculate();
    return this->times_.back();
}

template <class Interpolator> QuantLib::Time InterpolatedPriceCurve<Interpolator>::minTime() const {
    calculate();
    return this->times_.front();
}

template <class Interpolator> std::vector<QuantLib::Date> InterpolatedPriceCurve<Interpolator>::pillarDates() const {
    calculate();
    return dates_;
}

template <class Interpolator> QuantLib::Real InterpolatedPriceCurve<Interpolator>::priceImpl(QuantLib::Time t) const {
    // Return interpolated/extrapolated price
    calculate();
    return this->interpolation_(t, true);
}

template <class Interpolator> void InterpolatedPriceCurve<Interpolator>::initialise() {

    QL_REQUIRE(this->data_.size() >= Interpolator::requiredPoints, "not enough times for the interpolation method");

    // If we are quotes based, get prices from quotes
    if (!quotes_.empty()) {
        getPricesFromQuotes();
    }

    QL_REQUIRE(this->data_.size() == this->times_.size(), "Number of times must equal number of prices");

    QuantLib::InterpolatedCurve<Interpolator>::setupInterpolation();
    this->interpolation_.update();
}

template <class Interpolator> void InterpolatedPriceCurve<Interpolator>::populateDatesFromTenors() const {
    QuantLib::Date asof = QuantLib::Settings::instance().evaluationDate();
    for (QuantLib::Size i = 0; i < dates_.size(); ++i) {
        dates_[i] = asof + tenors_[i];
        this->times_[i] = timeFromReference(dates_[i]);
    }
}

template <class Interpolator> void InterpolatedPriceCurve<Interpolator>::convertDatesToTimes() {

    QL_REQUIRE(!dates_.empty(), "Dates cannot be empty for InterpolatedPriceCurve");
    this->times_[0] = timeFromReference(dates_[0]);
    for (QuantLib::Size i = 1; i < dates_.size(); ++i) {
        QL_REQUIRE(dates_[i] > dates_[i - 1], "invalid date (" << dates_[i] << ", vs " << dates_[i - 1] << ")");
        this->times_[i] = timeFromReference(dates_[i]);
        QL_REQUIRE(!QuantLib::close(this->times_[i], this->times_[i - 1]), "two dates correspond to the same time "
                                                                           "under this curve's day count convention");
    }
}

template <class Interpolator> void InterpolatedPriceCurve<Interpolator>::getPricesFromQuotes() const {

    for (QuantLib::Size i = 0; i < quotes_.size(); ++i) {
        QL_REQUIRE(!this->quotes_[i].empty(), "price quote at index " << i << " is empty");
        this->data_[i] = quotes_[i]->value();
    }
}

} // namespace QuantExt

#endif
