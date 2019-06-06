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

/*! \file optionpricesurface.hpp
    \brief Surface to store option prices
 */

#ifndef quantext_option_price_surface_hpp
#define quantext_option_price_surface_hpp

#include <qle/interpolators/optioninterpolator2d.hpp>
#include <ql/termstructure.hpp>
#include <ql/time/date.hpp>

namespace QuantExt {

//! Option Price Surface based on sparse matrix.
//!  \ingroup termstructures
class OptionPriceSurface : public QuantLib::TermStructure,
                           public OptionInterpolator2d {

public:
    OptionPriceSurface(const QuantLib::Date& referenceDate, 
        const std::vector<QuantLib::Date>& dates, const std::vector<QuantLib::Real>& strikes, 
        const std::vector<QuantLib::Real>& prices, const QuantLib::DayCounter& dayCounter) :
        OptionInterpolator2d(referenceDate, dayCounter, dates, strikes, prices) {};

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const { return QuantLib::Date::maxDate(); }
    //@}

    QuantLib::Real price(QuantLib::Time t, QuantLib::Real strike) const { return getValue(t, strike); };
    QuantLib::Real price(QuantLib::Date d, QuantLib::Real strike) const { return getValue(d, strike); };

};

} // namespace QuantExt

#endif
#pragma once
