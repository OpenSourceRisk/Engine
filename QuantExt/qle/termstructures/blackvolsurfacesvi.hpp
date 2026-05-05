/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file blackvolsurfacesvi.hpp
    \brief Black volatility surface backed by SVI parametric volatility
*/

#pragma once

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/quotes/simplequote.hpp>
#include <qle/termstructures/sviparametricvolatility.hpp>

namespace QuantExt {

using namespace QuantLib;

/*! Black volatility surface that uses SviParametricVolatility for smile interpolation.
    The surface is calibrated to absolute-strike option volatility quotes.
    Works for any asset class (equity, FX, etc.) given spot, forecast and dividend/foreign curves.
*/
class BlackVolatilitySurfaceSvi : public BlackVolatilityTermStructure {
public:
    BlackVolatilitySurfaceSvi(
        const Date& referenceDate, const std::vector<Date>& dates, const std::vector<std::vector<Real>>& strikes,
        const std::vector<std::vector<Real>>& quotes, const DayCounter& dayCounter, const Calendar& calendar,
        const Handle<Quote>& spot, const Size spotDays, const Calendar spotCalendar,
        const Handle<YieldTermStructure>& domesticTS,
        const Handle<YieldTermStructure>& foreignTS,
        SviParametricVolatility::ModelVariant modelVariant,
        ParametricVolatility::MarketQuoteType inputMarketQuoteType =
            ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
        const std::vector<std::vector<QuantLib::Option::Type>>& optionTypes = {},
        const std::map<std::pair<Real, Real>, std::vector<std::pair<Real, ParametricVolatility::ParameterCalibration>>>&
            modelParameters = {},
        Size maxCalibrationAttempts = 10, Real exitEarlyErrorThreshold = 0.005, Real maxAcceptableError = 0.05);

    Date maxDate() const override { return Date::maxDate(); }
    Real minStrike() const override { return 0; }
    Real maxStrike() const override { return QL_MAX_REAL; }

    const QuantLib::ext::shared_ptr<ParametricVolatility>& parametricVolatility() const { return svi_; }

private:
    Volatility blackVolImpl(Time t, Real strike) const override;

    //! Compute forward price at time t
    Real forward(Time t) const;

    Handle<Quote> spot_;
    Size spotDays_;
    Calendar spotCalendar_;
    Handle<YieldTermStructure> domesticTS_;
    Handle<YieldTermStructure> foreignTS_;
    QuantLib::ext::shared_ptr<ParametricVolatility> svi_;
    std::vector<Date> dates_;
};

} // namespace QuantExt
