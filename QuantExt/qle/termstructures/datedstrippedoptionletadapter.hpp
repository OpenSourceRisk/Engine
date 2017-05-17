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

/*! \file qle/termstructures/datedstrippedoptionletadapter.hpp
    \brief StrippedOptionlet Adapter
    \ingroup termstructures
*/

#pragma once

#include <qle/termstructures/datedstrippedoptionletbase.hpp>

#include <ql/math/interpolation.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Adapter class for turning a DatedStrippedOptionletBase object into an OptionletVolatilityStructure
/*! Takes a DatedStrippedOptionletBase and converts it into an OptionletVolatilityStructure with a fixed
    reference date

            \ingroup termstructures
*/
class DatedStrippedOptionletAdapter : public OptionletVolatilityStructure, public LazyObject {
public:
    DatedStrippedOptionletAdapter(const boost::shared_ptr<DatedStrippedOptionletBase>& s);

    //! \name TermStructure interface
    //@{
    Date maxDate() const;
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    Rate minStrike() const;
    Rate maxStrike() const;
    //@}
    //! \name LazyObject interface
    //@{
    void update();
    void performCalculations() const;
    //@}

    VolatilityType volatilityType() const;
    Real displacement() const;

protected:
    //! \name OptionletVolatilityStructure interface
    //@{
    boost::shared_ptr<SmileSection> smileSectionImpl(Time optionTime) const;
    Volatility volatilityImpl(Time length, Rate strike) const;
    //@}

private:
    const boost::shared_ptr<DatedStrippedOptionletBase> optionletStripper_;
    Size nInterpolations_;
    mutable vector<boost::shared_ptr<Interpolation> > strikeInterpolations_;
};

inline void DatedStrippedOptionletAdapter::update() {
    TermStructure::update();
    LazyObject::update();
}
} // namespace QuantExt
