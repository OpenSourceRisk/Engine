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

/*! \file blackvariancesurfacesparse.hpp
 \brief Black volatility surface modelled as variance surface
 */

#ifndef quantext_option_interpolator_2d_hpp
#define quantext_option_interpolator_2d_hpp

#include <ql/math/interpolation.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {

namespace detail {
struct CloseEnoughComparator {
    explicit CloseEnoughComparator(const QuantLib::Real v) : v_(v) {}
    bool operator()(const QuantLib::Real w) const { return QuantLib::close_enough(v_, w); }
    QuantLib::Real v_;
};
} // namespace detail


//! Option surface intepolator
//!  \ingroup interpolators
class OptionInterpolator2d {

public:
    OptionInterpolator2d(const QuantLib::Date& referenceDate,
        const std::vector<QuantLib::Date>& dates, const std::vector<QuantLib::Real>& strikes, 
        const std::vector<QuantLib::Real>& values, const QuantLib::DayCounter& dayCounter);

    //! \name Getters
    //@{
    DayCounter dayCounter() const { return dayCounter_; }
    std::vector<QuantLib::Time> times() { return times_; };
    std::vector<QuantLib::Date> expiries() { return expiries_; };
    std::vector<std::vector<QuantLib::Real> > strikes() { return strikes_; };
    QuantLib::Real getValue(QuantLib::Time t, QuantLib::Real strike) const;
    QuantLib::Real getValue(QuantLib::Date d, QuantLib::Real strike) const;

    //@}

protected:
    QuantLib::DayCounter dayCounter_;
    std::vector<QuantLib::Date> expiries_;                            // expiries
    std::vector<QuantLib::Time> times_;                               // times
    std::vector<QuantLib::Interpolation> interpolations_;             // strike interpolations for each expiry
    std::vector<std::vector<QuantLib::Real> > strikes_;               // strikes for each expiry
    std::vector<std::vector<QuantLib::Real> > values_;

private:
    QuantLib::Date referenceDate_;
    QuantLib::Real getValueForStrike(QuantLib::Real strike, const std::vector<QuantLib::Real>& strks, 
        const std::vector<QuantLib::Real>& vars, const QuantLib::Interpolation& intrp) const;

};
} //namespace QuantExt

#endif
