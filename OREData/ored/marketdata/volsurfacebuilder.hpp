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

/*! \file ored/marketdata/volsurfacebuilder.hpp
    \brief Shared vol surface builder utilities used across vol curve builders.
    \ingroup marketdata
*/

#pragma once

#include <ored/configuration/parametricsmileconfiguration.hpp>
#include <qle/termstructures/parametricvolatility.hpp>
#include <qle/termstructures/sviparametricvolatility.hpp>
#include <ql/handle.hpp>
#include <ql/option.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
#include <optional>
#include <string>
#include <vector>

namespace ore {
namespace data {

//! Build a BlackVolatilitySurfaceSvi, force calibration, and log RMSE diagnostics.
/*!
  \param curveLabel                   Prefix for log/error messages, e.g. "EquityVolCurve", "FXVolCurve".
  \param asof                         Valuation/anchor date.
  \param dates                        Expiry dates of the vol surface.
  \param strikes                      Per-expiry strike vectors.
  \param quotes                       Per-expiry market vol (or price) vectors aligned with \p strikes.
  \param optionTypes                  Per-expiry option type vectors (Call/Put) aligned with \p strikes.
  \param dayCounter                   Day counter used to convert dates to year fractions.
  \param calendar                     Holiday calendar for the surface.
  \param spot                         Spot price quote handle.
  \param spotDays                     Spot settlement lag. Pass 0 for equity/commodity surfaces.
  \param spotCalendar                 Spot settlement calendar. Pass the surface calendar for equity/commodity.
  \param curve1                       For FX: domestic discount curve. For EQ/COMM: forecast/risk-free curve.
  \param curve2                       For FX: foreign discount curve. For EQ/COMM: dividend/convenience yield.
  \param sviModelVariant              Which SVI/SSVI/ESSVI parametric model to calibrate.
  \param parametricSmileConfiguration Optional initial parameter values and calibration settings; if absent,
                                      model defaults are used.
  \param interpolationModel           String name of the model variant; used for log/error messages.
  \param inputMarketQuoteType         Quote type of the input vols (shifted-lognormal or normal).
  \param logPerStrikeFit              If true, logs market vs fitted vol for every (expiry, strike) slice.
*/
QuantLib::ext::shared_ptr<QuantLib::BlackVolTermStructure> buildSviSurface(
    const std::string& curveLabel,
    const QuantLib::Date& asof,
    const std::vector<QuantLib::Date>& dates,
    const std::vector<std::vector<QuantLib::Real>>& strikes,
    const std::vector<std::vector<QuantLib::Real>>& quotes,
    const std::vector<std::vector<QuantLib::Option::Type>>& optionTypes,
    const QuantLib::DayCounter& dayCounter,
    const QuantLib::Calendar& calendar,
    const QuantLib::Handle<QuantLib::Quote>& spot,
    QuantLib::Size spotDays,
    const QuantLib::Calendar& spotCalendar,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve1,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& curve2,
    QuantExt::SviParametricVolatility::ModelVariant sviModelVariant,
    const QuantLib::ext::optional<ParametricSmileConfiguration>& parametricSmileConfiguration,
    const std::string& interpolationModel,
    QuantExt::ParametricVolatility::MarketQuoteType inputMarketQuoteType =
        QuantExt::ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility,
    bool logPerStrikeFit = false);

} // namespace data
} // namespace ore
