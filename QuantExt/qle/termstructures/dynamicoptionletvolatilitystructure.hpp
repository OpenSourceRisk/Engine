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

/*! \file qle/termstructures/dynamicoptionletvolatilitystructure.hpp
    \brief dynamic optionlet volatility structure
    \ingroup termstructures
*/

#pragma once

#include <qle/termstructures/dynamicstype.hpp>

#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Converts OptionletVolatilityStructure with fixed reference date into a floating reference date term structure.
/*! Different ways of reacting to time decay can be specified.

    \warning No checks are performed that the supplied OptionletVolatilityStructure has a fixed reference date

        \ingroup termstructures
*/

class DynamicOptionletVolatilityStructure : public OptionletVolatilityStructure {
public:
    DynamicOptionletVolatilityStructure(const boost::shared_ptr<OptionletVolatilityStructure>& source,
                                        Natural settlementDays, const Calendar& calendar,
                                        ReactionToTimeDecay decayMode = ConstantVariance);

protected:
    //! \name OptionletVolatilityStructure interface
    //@{
    boost::shared_ptr<SmileSection> smileSectionImpl(Time optionTime) const;
    Volatility volatilityImpl(Time optionTime, Rate strike) const;
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    Rate minStrike() const;
    Rate maxStrike() const;
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    Date maxDate() const;
    //@}

    //! \name Observer interface
    //@{
    void update();
    //@}

    //! Override the default implementations in OptionletVolatilityStructure
    VolatilityType volatilityType() const;
    Real displacement() const;

private:
    const boost::shared_ptr<OptionletVolatilityStructure> source_;
    ReactionToTimeDecay decayMode_;
    const Date originalReferenceDate_;
    const VolatilityType volatilityType_;
    const Real displacement_;
};

inline VolatilityType DynamicOptionletVolatilityStructure::volatilityType() const { return volatilityType_; }

inline Real DynamicOptionletVolatilityStructure::displacement() const { return displacement_; }
} // namespace QuantExt
