/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*
 Copyright (C) 2009 Ferdinando Ametrano

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

/*! \file interpolatedsurvivalprobabilitycurve.hpp
    \brief interpolated survival-probability term structure (with the option to
           disable the check for negative hazard rates)
*/

#ifndef quantext_interpolated_survival_probability_curve_hpp
#define quantext_interpolated_survival_probability_curve_hpp

#include <ql/termstructures/credit/survivalprobabilitystructure.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <utility>

namespace QuantExt {
using namespace QuantLib;

    //! DefaultProbabilityTermStructure based on interpolation of survival probabilities
    /*! \ingroup defaultprobabilitytermstructures */
    template <class Interpolator>
    class InterpolatedSurvivalProbabilityCurve
        : public SurvivalProbabilityStructure,
          protected InterpolatedCurve<Interpolator> {
      public:
        InterpolatedSurvivalProbabilityCurve(
            const std::vector<Date>& dates,
            const std::vector<Probability>& probabilities,
            const DayCounter& dayCounter,
            const Calendar& calendar = Calendar(),
            const std::vector<Handle<Quote> >& jumps = std::vector<Handle<Quote> >(),
            const std::vector<Date>& jumpDates = std::vector<Date>(),
            const Interpolator& interpolator = Interpolator(),
            const bool allowNegativeRates = false);
        //! \name TermStructure interface
        //@{
        Date maxDate() const;
        //@}
        //! \name other inspectors
        //@{
        const std::vector<Time>& times() const;
        const std::vector<Date>& dates() const;
        const std::vector<Real>& data() const;
        const std::vector<Probability>& survivalProbabilities() const;
        std::vector<std::pair<Date, Real> > nodes() const;
        //@}
      protected:
        InterpolatedSurvivalProbabilityCurve(
            const DayCounter&,
            const std::vector<Handle<Quote> >& jumps = std::vector<Handle<Quote> >(),
            const std::vector<Date>& jumpDates = std::vector<Date>(),
            const Interpolator& interpolator = Interpolator(),
            const bool allowNegativeRates = false);
        InterpolatedSurvivalProbabilityCurve(
            const Date& referenceDate,
            const DayCounter&,
            const std::vector<Handle<Quote> >& jumps = std::vector<Handle<Quote> >(),
            const std::vector<Date>& jumpDates = std::vector<Date>(),
            const Interpolator& interpolator = Interpolator(),
            const bool allowNegativeRates = false);
        InterpolatedSurvivalProbabilityCurve(
            Natural settlementDays,
            const Calendar&,
            const DayCounter&,
            const std::vector<Handle<Quote> >& jumps = std::vector<Handle<Quote> >(),
            const std::vector<Date>& jumpDates = std::vector<Date>(),
            const Interpolator& interpolator = Interpolator(),
            const bool allowNegativeRates = false);
        //! \name DefaultProbabilityTermStructure implementation
        //@{
        Probability survivalProbabilityImpl(Time) const;
        Real defaultDensityImpl(Time) const;
        //@}
        mutable std::vector<Date> dates_;
    private:
        bool allowNegativeRates_;
    };

    // inline definitions

    template <class T>
    inline Date InterpolatedSurvivalProbabilityCurve<T>::maxDate() const {
        return dates_.back();
    }

    template <class T>
    inline const std::vector<Time>&
    InterpolatedSurvivalProbabilityCurve<T>::times() const {
        return this->times_;
    }

    template <class T>
    inline const std::vector<Date>&
    InterpolatedSurvivalProbabilityCurve<T>::dates() const {
        return dates_;
    }

    template <class T>
    inline const std::vector<Real>&
    InterpolatedSurvivalProbabilityCurve<T>::data() const {
        return this->data_;
    }

    template <class T>
    inline const std::vector<Probability>&
    InterpolatedSurvivalProbabilityCurve<T>::survivalProbabilities() const {
        return this->data_;
    }

    template <class T>
    inline std::vector<std::pair<Date,Real> >
    InterpolatedSurvivalProbabilityCurve<T>::nodes() const {
        std::vector<std::pair<Date,Real> > results(dates_.size());
        for (Size i=0; i<dates_.size(); ++i)
            results[i] = std::make_pair(dates_[i],this->data_[i]);
        return results;
    }

    #ifndef __DOXYGEN__

    // template definitions

    template <class T>
    Probability
    InterpolatedSurvivalProbabilityCurve<T>::survivalProbabilityImpl(Time t)
                                                                        const {
        if (t <= this->times_.back())
            return this->interpolation_(t, true);

        // flat hazard rate extrapolation
        Time tMax = this->times_.back();
        Probability sMax = this->data_.back();
        Rate hazardMax = - this->interpolation_.derivative(tMax) / sMax;
        return sMax * std::exp(- hazardMax * (t-tMax));
    }

    template <class T>
    Real
    InterpolatedSurvivalProbabilityCurve<T>::defaultDensityImpl(Time t) const {
        if (t <= this->times_.back())
            return -this->interpolation_.derivative(t, true);

        // flat hazard rate extrapolation
        Time tMax = this->times_.back();
        Probability sMax = this->data_.back();
        Rate hazardMax = - this->interpolation_.derivative(tMax) / sMax;
        return sMax * hazardMax * std::exp(- hazardMax * (t-tMax));
    }

    template <class T>
    InterpolatedSurvivalProbabilityCurve<T>::InterpolatedSurvivalProbabilityCurve(
                                    const DayCounter& dayCounter,
                                    const std::vector<Handle<Quote> >& jumps,
                                    const std::vector<Date>& jumpDates,
                                    const T& interpolator,
                                    const bool allowNegativeRates)
    : SurvivalProbabilityStructure(dayCounter, jumps, jumpDates),
      InterpolatedCurve<T>(interpolator), allowNegativeRates_(allowNegativeRates) {}

    template <class T>
    InterpolatedSurvivalProbabilityCurve<T>::InterpolatedSurvivalProbabilityCurve(
                                    const Date& referenceDate,
                                    const DayCounter& dayCounter,
                                    const std::vector<Handle<Quote> >& jumps,
                                    const std::vector<Date>& jumpDates,
                                    const T& interpolator,
                                    const bool allowNegativeRates)
    : SurvivalProbabilityStructure(referenceDate, Calendar(), dayCounter, jumps, jumpDates),
      InterpolatedCurve<T>(interpolator), allowNegativeRates_(allowNegativeRates) {}

    template <class T>
    InterpolatedSurvivalProbabilityCurve<T>::InterpolatedSurvivalProbabilityCurve(
                                    Natural settlementDays,
                                    const Calendar& calendar,
                                    const DayCounter& dayCounter,
                                    const std::vector<Handle<Quote> >& jumps,
                                    const std::vector<Date>& jumpDates,
                                    const T& interpolator,
                                    const bool allowNegativeRates)
    : SurvivalProbabilityStructure(settlementDays, calendar, dayCounter, jumps, jumpDates),
      InterpolatedCurve<T>(interpolator), allowNegativeRates_(allowNegativeRates) {}

    template <class T>
    InterpolatedSurvivalProbabilityCurve<T>::InterpolatedSurvivalProbabilityCurve(
                                    const std::vector<Date>& dates,
                                    const std::vector<Probability>& probabilities,
                                    const DayCounter& dayCounter,
                                    const Calendar& calendar,
                                    const std::vector<Handle<Quote> >& jumps,
                                    const std::vector<Date>& jumpDates,
                                    const T& interpolator,
                                    const bool allowNegativeRates)
    : SurvivalProbabilityStructure(dates.at(0), calendar, dayCounter, jumps, jumpDates),
      InterpolatedCurve<T>(std::vector<Time>(), probabilities, interpolator),
      dates_(dates), allowNegativeRates_(allowNegativeRates)
    {
        QL_REQUIRE(dates_.size() >= T::requiredPoints,
                   "not enough input dates given");
        QL_REQUIRE(this->data_.size() == dates_.size(),
                   "dates/data count mismatch");
        QL_REQUIRE(this->data_[0] == 1.0,
                   "the first probability must be == 1.0 "
                   "to flag the corresponding date as reference date");

        this->times_.resize(dates_.size());
        this->times_[0] = 0.0;
        for (Size i=1; i<dates_.size(); ++i) {
            QL_REQUIRE(dates_[i] > dates_[i-1],
                       "invalid date (" << dates_[i] << ", vs "
                       << dates_[i-1] << ")");
            this->times_[i] = dayCounter.yearFraction(dates_[0], dates_[i]);
            QL_REQUIRE(!close(this->times_[i],this->times_[i-1]),
                       "two dates correspond to the same time "
                       "under this curve's day count convention");
            QL_REQUIRE(this->data_[i] > 0.0, "negative probability");
            QL_REQUIRE(allowNegativeRates_ || this->data_[i] <= this->data_[i-1],
                       "negative hazard rate implied by the survival "
                       "probability " <<
                       this->data_[i] << " at " << dates_[i] <<
                       " (t=" << this->times_[i] << ") after the survival "
                       "probability " <<
                       this->data_[i-1] << " at " << dates_[i-1] <<
                       " (t=" << this->times_[i-1] << ")");
        }

        this->interpolation_ =
            this->interpolator_.interpolate(this->times_.begin(),
                                            this->times_.end(),
                                            this->data_.begin());
        this->interpolation_.update();
    }

    #endif
}

#endif
