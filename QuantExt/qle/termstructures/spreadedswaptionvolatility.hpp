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

/*! \file qle/termstructures/spreadedswaptionvolatility.hpp
    \brief Adds floor to QuantLib::SpreadedSwaptionVolatility
    \ingroup termstructures
*/

#ifndef quantlib_spreaded_swaption_volatility_h
#define quantlib_spreaded_swaption_volatility_h

#include <ql/termstructures/volatility/swaption/spreadedswaptionvol.hpp>

using QuantLib::Date;
using QuantLib::Handle;
using QuantLib::Period;
using QuantLib::Quote;
using QuantLib::Rate;
using QuantLib::Real;
using QuantLib::SmileSection;
using QuantLib::Time;
using QuantLib::Volatility;

namespace QuantExt {

class SpreadedSwaptionVolatility : public QuantLib::SpreadedSwaptionVolatility {
public:
    SpreadedSwaptionVolatility(const Handle<SwaptionVolatilityStructure>&, const Handle<Quote>& spread);

protected:
    //! \name SwaptionVolatilityStructure interface
    //@{
    boost::shared_ptr<SmileSection> smileSectionImpl(const Date& optionDate, const Period& swapTenor) const;
    boost::shared_ptr<SmileSection> smileSectionImpl(Time optionTime, Time swapLength) const;
    Volatility volatilityImpl(const Date& optionDate, const Period& swapTenor, Rate strike) const;
    Volatility volatilityImpl(Time optionTime, Time swapLength, Rate strike) const;
    //@}
};
} // namespace QuantExt

#endif
