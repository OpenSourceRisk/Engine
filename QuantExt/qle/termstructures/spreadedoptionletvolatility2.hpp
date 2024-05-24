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
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

#include <boost/smart_ptr/shared_ptr.hpp>

namespace QuantExt {
using namespace QuantLib;

class SpreadedOptionletVolatility2 : public OptionletVolatilityStructure, public LazyObject {
public:
    SpreadedOptionletVolatility2(const Handle<OptionletVolatilityStructure>& baseVol,
                                 const std::vector<Date>& optionDates, const std::vector<Real>& strikes,
                                 const std::vector<std::vector<Handle<Quote>>>& volSpreads);
    BusinessDayConvention businessDayConvention() const override;
    Rate minStrike() const override;
    Rate maxStrike() const override;
    DayCounter dayCounter() const override;
    Date maxDate() const override;
    Time maxTime() const override;
    const Date& referenceDate() const override;
    Calendar calendar() const override;
    Natural settlementDays() const override;
    VolatilityType volatilityType() const override;
    Real displacement() const override;
    void update() override;
    void deepUpdate() override;

protected:
    QuantLib::ext::shared_ptr<SmileSection> smileSectionImpl(Time optionTime) const override;
    Volatility volatilityImpl(Time optionTime, Rate strike) const override;
    void performCalculations() const override;

private:
    Handle<OptionletVolatilityStructure> baseVol_;
    std::vector<Date> optionDates_;
    std::vector<Real> strikes_;
    std::vector<std::vector<Handle<Quote>>> volSpreads_;
    //
    mutable std::vector<Real> optionTimes_;
    mutable Matrix volSpreadValues_;
    mutable Interpolation2D volSpreadInterpolation_;
};
} // namespace QuantExt
