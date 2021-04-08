/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file blackvolsurfacebfrr.hpp
    \brief Black volatility surface based on bf/rr quotes
*/

#pragma once

#include <ql/experimental/fx/deltavolquote.hpp>
#include <ql/math/interpolation.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/option.hpp>

namespace QuantExt {

using namespace QuantLib;

class BlackVolatilitySurfaceBFRR : public BlackVolatilityTermStructure {
public:
    enum class SmileInterpolation { Linear, Cubic };
    BlackVolatilitySurfaceBFRR(
        Date referenceDate, const std::vector<Date>& dates, const std::vector<Real>& deltas,
        const std::vector<std::vector<Real>>& bfQuotes, const std::vector<std::vector<Real>>& rrQuotes,
        const std::vector<Real>& atmQuotes, const DayCounter& dayCounter, const Calendar& calendar,
        const Handle<Quote>& spot, const Handle<YieldTermStructure>& domesticTS,
        const Handle<YieldTermStructure>& foreignTS, const DeltaVolQuote::DeltaType dt = DeltaVolQuote::DeltaType::Spot,
        const DeltaVolQuote::AtmType at = DeltaVolQuote::AtmType::AtmDeltaNeutral, const Period& switchTenor = 0 * Days,
        const DeltaVolQuote::DeltaType ltdt = DeltaVolQuote::DeltaType::Fwd,
        const DeltaVolQuote::AtmType ltat = DeltaVolQuote::AtmType::AtmDeltaNeutral,
        const Option::Type riskReversalInFavorOf = Option::Call, const bool butterflyIsBrokerStyle = true,
        const SmileInterpolation smileInterpolation = SmileInterpolation::Linear);

    Date maxDate() const override { return Date::maxDate(); }
    Real minStrike() const override { return 0; }
    Real maxStrike() const override { return QL_MAX_REAL; }

    const std::vector<QuantLib::Date>& dates() const { return dates_; }
    const std::vector<Real>& deltas() const { return deltas_; }
    const std::vector<std::vector<Real>>& bfQuotes() const { return bfQuotes_; }
    const std::vector<std::vector<Real>>& rrQuotes() const { return rrQuotes_; }
    const std::vector<Real>& atmQuotes() const { return atmQuotes_; }
    const Handle<Quote>& spot() const { return spot_; }
    const Handle<YieldTermStructure>& domesticTS() const { return domesticTS_; }
    const Handle<YieldTermStructure>& foreignTS() const { return foreignTS_; }
    DeltaVolQuote::DeltaType deltaType() const { return dt_; }
    DeltaVolQuote::AtmType atmType() const { return at_; }
    const Period& switchTenor() const { return switchTenor_; }
    DeltaVolQuote::DeltaType longTermDeltaType() const { return ltdt_; }
    DeltaVolQuote::AtmType longTermAtmType() const { return ltat_; }
    Option::Type riskReversalInFavorOf() const { return riskReversalInFavorOf_; }
    bool butterflyIsBrokerStyle() const { return butterflyIsBrokerStyle_; }
    SmileInterpolation smileInterpolation() const { return smileInterpolation_; }

private:
    Volatility blackVolImpl(Time t, Real strike) const override;

    std::vector<Date> dates_;
    std::vector<Real> deltas_;
    std::vector<std::vector<Real>> bfQuotes_;
    std::vector<std::vector<Real>> rrQuotes_;
    std::vector<Real> atmQuotes_;
    Handle<Quote> spot_;
    Handle<YieldTermStructure> domesticTS_;
    Handle<YieldTermStructure> foreignTS_;
    DeltaVolQuote::DeltaType dt_;
    DeltaVolQuote::AtmType at_;
    Period switchTenor_;
    DeltaVolQuote::DeltaType ltdt_;
    DeltaVolQuote::AtmType ltat_;
    Option::Type riskReversalInFavorOf_;
    bool butterflyIsBrokerStyle_;
    SmileInterpolation smileInterpolation_;
};

} // namespace QuantExt
