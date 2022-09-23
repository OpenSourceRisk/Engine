/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file spreadedoptionletvolatility2.hpp
    \brief Optionlet volatility with overlayed bilinearly interpolated spread surface
    \ingroup termstructures
*/

#pragma once

#include <ql/math/interpolations/interpolation2d.hpp>
#include <ql/patterns/lazyobject.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>

#include <boost/smart_ptr/shared_ptr.hpp>

namespace QuantExt {
using namespace QuantLib;

class SpreadedCPIVolatilitySurface : public QuantLib::CPIVolatilitySurface, public LazyObject {
public:
    // warning we assume that volatilities are retrieved with obLag = -1D, i.e. using the standard lag from the ts
    SpreadedCPIVolatilitySurface(const Handle<CPIVolatilitySurface>& baseVol, const std::vector<Date>& optionDates,
                                 const std::vector<Real>& strikes,
                                 const std::vector<std::vector<Handle<Quote>>>& volSpreads);
    Rate minStrike() const override;
    Rate maxStrike() const override;
    Date maxDate() const override;
    Time maxTime() const override;
    const Date& referenceDate() const override;
    void update() override;
    void deepUpdate() override;

protected:
    Volatility volatilityImpl(Time length, Rate strike) const override;
    void performCalculations() const override;

private:
    Handle<CPIVolatilitySurface> baseVol_;
    std::vector<Date> optionDates_;
    std::vector<Real> strikes_;
    std::vector<std::vector<Handle<Quote>>> volSpreads_;
    //
    mutable std::vector<Real> optionTimes_;
    mutable Matrix volSpreadValues_;
    mutable Interpolation2D volSpreadInterpolation_;
};

} // namespace QuantExt
