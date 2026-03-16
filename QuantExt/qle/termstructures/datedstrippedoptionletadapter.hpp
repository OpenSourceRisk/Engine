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

namespace QuantExt {
using namespace QuantLib;

//! Adapter class for turning a DatedStrippedOptionletBase object into an OptionletVolatilityStructure
/*! Takes a DatedStrippedOptionletBase and converts it into an OptionletVolatilityStructure with a fixed
    reference date

            \ingroup termstructures
*/
class DatedStrippedOptionletAdapter : public OptionletVolatilityStructure, public LazyObject {
public:
    DatedStrippedOptionletAdapter(const QuantLib::ext::shared_ptr<DatedStrippedOptionletBase>& s, const bool flatExtrapolation);

    //! \name TermStructure interface
    //@{
    Date maxDate() const override;
    //@}
    //! \name VolatilityTermStructure interface
    //@{
    Rate minStrike() const override;
    Rate maxStrike() const override;
    //@}
    //! \name LazyObject interface
    //@{
    void update() override;
    void performCalculations() const override;
    //@}

    VolatilityType volatilityType() const override;
    Real displacement() const override;

protected:
    //! \name OptionletVolatilityStructure interface
    //@{
    QuantLib::ext::shared_ptr<SmileSection> smileSectionImpl(Time optionTime) const override;
    Volatility volatilityImpl(Time length, Rate strike) const override;
    //@}

private:
    const QuantLib::ext::shared_ptr<DatedStrippedOptionletBase> optionletStripper_;
    Size nInterpolations_;
    mutable vector<QuantLib::ext::shared_ptr<Interpolation> > strikeInterpolations_;
    bool flatExtrapolation_;
};

inline void DatedStrippedOptionletAdapter::update() {
    TermStructure::update();
    LazyObject::update();
}
} // namespace QuantExt
