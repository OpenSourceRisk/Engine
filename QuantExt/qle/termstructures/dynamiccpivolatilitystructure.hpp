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

/*! \file qle/termstructures/dynamiccpivolatilitystructure.hpp
\brief dynamic zero inflation volatility structure
\ingroup termstructures
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>
#include <ql/termstructures/volatility/smilesection.hpp>
#include <qle/termstructures/dynamicstype.hpp>
#include <qle/termstructures/interpolatedcpivolatilitysurface.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Converts a CPIVolatilityStructure with fixed reference date into a floating reference date term structure.
/*! Different ways of reacting to time decay can be specified.

\ingroup termstructures
*/

class DynamicCPIVolatilitySurface : public CPIVolatilitySurface {
public:
    DynamicCPIVolatilitySurface(const QuantLib::ext::shared_ptr<CPIVolatilitySurface>& source,
                                ReactionToTimeDecay decayMode = ConstantVariance);

protected:
    //! \name CPIVolatilitySurface interface
    //@{
    Volatility volatilityImpl(Time length, Rate strike) const override;
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    Rate minStrike() const override;
    Rate maxStrike() const override;
    //@}

    //! \name VolatilityTermStructure interface
    //@{
    Date maxDate() const override;
    //@}

    //! \name Observer interface
    //@{
    void update() override;
    //@}

private:
    const QuantLib::ext::shared_ptr<CPIVolatilitySurface> source_;
    ReactionToTimeDecay decayMode_;
    const Date originalReferenceDate_;
};

} // namespace QuantExt
