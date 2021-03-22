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

/*! \file qle/termstructures/strippedyoyinflationoptionletvol.hpp
\brief Stripped YoYInfaltion Optionlet Vol Adapter (with a deeper update method, linear interpolation and optional flat
extrapolation) \ingroup termstructures
*/

#ifndef quantext_stripped_yoy_inflation_optionlet_vol
#define quantext_stripped_yoy_inflation_optionlet_vol

#include <ql/math/interpolation.hpp>
#include <ql/math/interpolations/sabrinterpolation.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/optionlet/optionletstripper.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletbase.hpp>

namespace QuantExt {

/*! Helper class to wrap in a YoYOptionletVolatilitySurface object.
\ingroup termstructures
*/
class StrippedYoYInflationOptionletVol : public QuantLib::YoYOptionletVolatilitySurface, public QuantLib::LazyObject {
public:
    StrippedYoYInflationOptionletVol(Natural settlementDays, const Calendar& calendar, BusinessDayConvention bdc,
                                     const DayCounter& dc, const Period& observationLag, Frequency frequency,
                                     bool indexIsInterpolated, const std::vector<Date>& yoyoptionletDates,
                                     const std::vector<Rate>& strikes, const std::vector<std::vector<Handle<Quote> > >&,
                                     VolatilityType type = ShiftedLognormal, Real displacement = 0.0);

    QuantLib::Date maxDate() const;
    void performCalculations() const;
    void update();

    const std::vector<Rate>& yoyoptionletStrikes(Size i) const;
    const std::vector<Volatility>& yoyoptionletVolatilities(Size i) const;

    const std::vector<Date>& yoyoptionletFixingDates() const;
    const std::vector<Time>& yoyoptionletFixingTimes() const;

    DayCounter dayCounter() const;
    Calendar calendar() const;
    Natural settlementDays() const;
    BusinessDayConvention businessDayConvention() const;

    QuantLib::VolatilityType volatilityType() const;
    QuantLib::Real displacement() const;

protected:
    QuantLib::Rate minStrike() const;
    QuantLib::Rate maxStrike() const;
    QuantLib::Volatility volatilityImpl(Time length, QuantLib::Rate strike) const;

private:
    void checkInputs() const;
    void registerWithMarketData();

    Calendar calendar_;
    Natural settlementDays_;
    BusinessDayConvention businessDayConvention_;
    DayCounter dc_;
    VolatilityType type_;
    Real displacement_;

    Size nYoYOptionletDates_;
    std::vector<Date> yoyoptionletDates_;
    std::vector<Time> yoyoptionletTimes_;
    std::vector<std::vector<Rate> > yoyoptionletStrikes_;
    Size nStrikes_;

    std::vector<std::vector<Handle<Quote> > > yoyoptionletVolQuotes_;
    mutable std::vector<std::vector<Volatility> > yoyoptionletVolatilities_;
};

inline void StrippedYoYInflationOptionletVol::update() {
    TermStructure::update();
    LazyObject::update();
}

} // namespace QuantExt

#endif
