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

#include <algorithm>

#include <qle/termstructures/pricetermstructure.hpp>

#include <ql/quote.hpp>
#include <ql/math/comparison.hpp>
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
    class InterpolatedPriceCurve: public PriceTermStructure, public QuantLib::LazyObject, 
        protected QuantLib::InterpolatedCurve<Interpolator> {
    public:
        //! \name Constructors
        //@{
        //! Curve constructed from times and prices
        InterpolatedPriceCurve(const std::vector<QuantLib::Time>& times,
            const std::vector<QuantLib::Real>& prices, 
            const QuantLib::DayCounter& dc, 
            const Interpolator& interpolator = Interpolator());

        //! Curve constructed from times and quotes
        InterpolatedPriceCurve(const std::vector<QuantLib::Time>& times,
            const std::vector<QuantLib::Handle<QuantLib::Quote> >& quotes,
            const QuantLib::DayCounter& dc, 
            const Interpolator& interpolator = Interpolator());

        //! Curve constructed from dates and prices
        InterpolatedPriceCurve(const std::vector<QuantLib::Date>& dates,
            const std::vector<QuantLib::Real>& prices,
            const QuantLib::DayCounter& dc,
            const Interpolator& interpolator = Interpolator());

        //! Curve constructed from dates and quotes
        InterpolatedPriceCurve(const std::vector<QuantLib::Date>& dates,
            const std::vector<QuantLib::Handle<QuantLib::Quote> >& quotes,
            const QuantLib::DayCounter& dc,
            const Interpolator& interpolator = Interpolator());
        //@}

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
        //! This is not used by this class and returns the maximum date
        QuantLib::Date maxDate() const { return QuantLib::Date::maxDate(); }
        QuantLib::Time maxTime() const;
        //@}

        //! \name PriceTermStructure interface
        //@{
        QuantLib::Time minTime() const;
        //@}

        //! \name Inspectors
        //@{
        const std::vector<QuantLib::Time>& times() const { return this->times_; }
        const std::vector<QuantLib::Real>& prices() const { return this->data_; }
        //@}
        
    protected:
        //! \name PriceTermStructure implementation
        //@{
        QuantLib::Real priceImpl(QuantLib::Time t) const;
        //@}

    private:
        std::vector<QuantLib::Handle<QuantLib::Quote> > quotes_;
        std::vector<QuantLib::Date> dates_;

        void initialise();
        void convertDatesToTimes();
        void getPricesFromQuotes() const;
    };

    template <class Interpolator>
    InterpolatedPriceCurve<Interpolator>::InterpolatedPriceCurve(const std::vector<QuantLib::Time>& times,
        const std::vector<QuantLib::Real>& prices, const QuantLib::DayCounter& dc, const Interpolator& interpolator)
        : PriceTermStructure(0, QuantLib::NullCalendar(), dc), 
          QuantLib::InterpolatedCurve<Interpolator>(times, prices, interpolator) {

        initialise();
    }

    template <class Interpolator>
    InterpolatedPriceCurve<Interpolator>::InterpolatedPriceCurve(const std::vector<QuantLib::Time>& times,
        const std::vector<QuantLib::Handle<QuantLib::Quote> >& quotes, 
        const QuantLib::DayCounter& dc, const Interpolator& interpolator)
        : PriceTermStructure(0, QuantLib::NullCalendar(), dc),
          QuantLib::InterpolatedCurve<Interpolator>(times, std::vector<QuantLib::Real>(quotes.size()), interpolator),
          quotes_(quotes) {

        initialise();

        // Observe the quotes
        for (QuantLib::Size i = 0; i < quotes_.size(); ++i) {
            registerWith(quotes[i]);
        }
    }

    template <class Interpolator>
    InterpolatedPriceCurve<Interpolator>::InterpolatedPriceCurve(const std::vector<QuantLib::Date>& dates,
        const std::vector<QuantLib::Real>& prices, const QuantLib::DayCounter& dc, const Interpolator& interpolator)
        : PriceTermStructure(dates.at(0), QuantLib::NullCalendar(), dc),
          QuantLib::InterpolatedCurve<Interpolator>(std::vector<QuantLib::Time>(dates.size()), prices, interpolator),
          dates_(dates) {

        initialise();
    }

    template <class Interpolator>
    InterpolatedPriceCurve<Interpolator>::InterpolatedPriceCurve(const std::vector<QuantLib::Date>& dates,
        const std::vector<QuantLib::Handle<QuantLib::Quote> >& quotes,
        const QuantLib::DayCounter& dc, const Interpolator& interpolator)
        : PriceTermStructure(dates.at(0), QuantLib::NullCalendar(), dc), 
          QuantLib::InterpolatedCurve<Interpolator>(std::vector<QuantLib::Time>(dates.size()),
              std::vector<QuantLib::Real>(quotes.size()), interpolator),
          quotes_(quotes), dates_(dates) {

        initialise();

        // Observe the quotes
        for (QuantLib::Size i = 0; i < quotes_.size(); ++i) {
            registerWith(quotes[i]);
        }
    }

    template <class Interpolator>
    void InterpolatedPriceCurve<Interpolator>::update() {

        QuantLib::LazyObject::update();

        // TermStructure::update() update part
        if (moving_) {
            updated_ = false;
        }
    }

    template <class Interpolator>
    void InterpolatedPriceCurve<Interpolator>::performCalculations() const {
        // Calculations only need to be performed if the curve depends on quotes
        if (!quotes_.empty()) {
            getPricesFromQuotes();
            this->interpolation_.update();
        }
    }

    template <class Interpolator>
    QuantLib::Time InterpolatedPriceCurve<Interpolator>::maxTime() const {
        return this->times_.back();
    }

    template <class Interpolator>
    QuantLib::Time InterpolatedPriceCurve<Interpolator>::minTime() const {
        return this->times_.front();
    }

    template <class Interpolator>
    QuantLib::Real InterpolatedPriceCurve<Interpolator>::priceImpl(QuantLib::Time t) const {

        // Make sure interpolation is up to date
        QuantLib::LazyObject::calculate();

        // Return interpolated/extrapolated price
        return this->interpolation_(t, true);
    }

    template <class Interpolator>
    void InterpolatedPriceCurve<Interpolator>::initialise() {

        QL_REQUIRE(this->data_.size() >= Interpolator::requiredPoints, "not enough times for the interpolation method");

        // If we are dates based, populate times_ from dates_
        if (!this->dates_.empty()) {
            convertDatesToTimes();
        }

        // If we are quotes based, get prices from quotes
        if (!quotes_.empty()) {
            getPricesFromQuotes();
        }

        QL_REQUIRE(this->data_.size() == this->times_.size(), "Number of times must equal number of prices");
        QL_REQUIRE(std::is_sorted(this->times_.begin(), this->times_.end()), "Times must be sorted");
        QL_REQUIRE(*std::min_element(this->data_.begin(), this->data_.end()) >= 0.0, "Prices must be positive");

        InterpolatedCurve<Interpolator>::setupInterpolation();
        this->interpolation_.update();
    }

    template <class Interpolator>
    void InterpolatedPriceCurve<Interpolator>::convertDatesToTimes() {

        this->times_[0] = 0.0;
        for (QuantLib::Size i = 1; i < this->dates_.size(); ++i) {
            QL_REQUIRE(this->dates_[i] > this->dates_[i - 1], "invalid date (" << this->dates_[i] << ", vs " << this->dates_[i - 1] << ")");
            this->times_[i] = dayCounter().yearFraction(this->dates_[0], this->dates_[i]);
            QL_REQUIRE(!QuantLib::close(this->times_[i], this->times_[i - 1]), "two dates correspond to the same time " 
                "under this curve's day count convention");
        }
    }

    template <class Interpolator>
    void InterpolatedPriceCurve<Interpolator>::getPricesFromQuotes() const {

        for (QuantLib::Size i = 0; i < quotes_.size(); ++i) {
            QL_REQUIRE(!this->quotes_[i].empty(), "price quote at index " << i << " is empty");
            this->data_[i] = quotes_[i]->value();
        }
    }

}

#endif
