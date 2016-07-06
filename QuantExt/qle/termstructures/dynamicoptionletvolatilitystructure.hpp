/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file qle/termstructures/dynamicoptionletvolatilitystructure.hpp
    \brief dynamic optionlet volatility structure
*/

#pragma once

#include <qle/termstructures/dynamicstype.hpp>

#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! Converts OptionletVolatilityStructure with fixed reference date into a floating reference date term structure.
    Different ways of reacting to time decay can be specified.
    
    \warning No checks are performed that the supplied OptionletVolatilityStructure has a fixed reference date
*/

class DynamicOptionletVolatilityStructure : public OptionletVolatilityStructure {
public:
    DynamicOptionletVolatilityStructure(const boost::shared_ptr<OptionletVolatilityStructure> &source,
        Natural settlementDays, const Calendar &calendar, ReactionToTimeDecay decayMode = ConstantVariance);

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

inline VolatilityType DynamicOptionletVolatilityStructure::volatilityType() const {
    return volatilityType_;
}

inline Real DynamicOptionletVolatilityStructure::displacement() const {
    return displacement_;
}
}
