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

#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/strippedyoyinflationoptionletvol.hpp>

#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/interpolations/sabrinterpolation.hpp>
#include <ql/termstructures/volatility/interpolatedsmilesection.hpp>

using namespace QuantLib;
using std::vector;

namespace QuantExt {

StrippedYoYInflationOptionletVol::StrippedYoYInflationOptionletVol(
    Natural settlementDays, const Calendar& calendar, BusinessDayConvention bdc, const DayCounter& dc,
    const Period& observationLag, Frequency frequency, bool indexIsInterpolated,
    const std::vector<Date>& yoyoptionletDates, const std::vector<Rate>& strikes,
    const std::vector<std::vector<Handle<Quote> > >& v, VolatilityType type, Real displacement)
    : YoYOptionletVolatilitySurface(settlementDays, calendar, bdc, dc, observationLag, frequency, indexIsInterpolated),
      calendar_(calendar), settlementDays_(settlementDays), businessDayConvention_(bdc), dc_(dc), type_(type),
      displacement_(displacement), nYoYOptionletDates_(yoyoptionletDates.size()), yoyoptionletDates_(yoyoptionletDates),
      yoyoptionletTimes_(nYoYOptionletDates_), yoyoptionletStrikes_(nYoYOptionletDates_, strikes),
      nStrikes_(strikes.size()), yoyoptionletVolQuotes_(v),
      yoyoptionletVolatilities_(nYoYOptionletDates_, vector<Volatility>(nStrikes_)) {

    checkInputs();
    registerWith(Settings::instance().evaluationDate());
    registerWithMarketData();

    for (Size i = 0; i < nYoYOptionletDates_; ++i)
        yoyoptionletTimes_[i] = dc_.yearFraction(Settings::instance().evaluationDate(), yoyoptionletDates_[i]);
}

void StrippedYoYInflationOptionletVol::checkInputs() const {
    if (type_ == Normal) {
        QL_REQUIRE(displacement_ == 0.0, "non-null displacement is not allowed with Normal model");
    }

    QL_REQUIRE(!yoyoptionletDates_.empty(), "empty yoy optionlet tenor vector");
    QL_REQUIRE(nYoYOptionletDates_ == yoyoptionletVolQuotes_.size(), "mismatch between number of option tenors ("
                                                                         << nYoYOptionletDates_
                                                                         << ") and number of volatility rows ("
                                                                         << yoyoptionletVolQuotes_.size() << ")");
    QL_REQUIRE(yoyoptionletDates_[0] > Settings::instance().evaluationDate(),
               "first option date (" << yoyoptionletDates_[0] << ") is in the past");
    for (Size i = 1; i < nYoYOptionletDates_; ++i)
        QL_REQUIRE(yoyoptionletDates_[i] > yoyoptionletDates_[i - 1],
                   "non increasing option dates: " << io::ordinal(i) << " is " << yoyoptionletDates_[i - 1] << ", "
                                                   << io::ordinal(i + 1) << " is " << yoyoptionletDates_[i]);

    QL_REQUIRE(nStrikes_ == yoyoptionletVolQuotes_[0].size(),
               "mismatch between strikes(" << yoyoptionletStrikes_[0].size() << ") and vol columns ("
                                           << yoyoptionletVolQuotes_[0].size() << ")");
    for (Size j = 1; j < nStrikes_; ++j)
        QL_REQUIRE(yoyoptionletStrikes_[0][j - 1] < yoyoptionletStrikes_[0][j],
                   "non increasing strikes: " << io::ordinal(j) << " is " << io::rate(yoyoptionletStrikes_[0][j - 1])
                                              << ", " << io::ordinal(j + 1) << " is "
                                              << io::rate(yoyoptionletStrikes_[0][j]));
}

void StrippedYoYInflationOptionletVol::registerWithMarketData() {
    for (Size i = 0; i < nYoYOptionletDates_; ++i)
        for (Size j = 0; j < nStrikes_; ++j)
            registerWith(yoyoptionletVolQuotes_[i][j]);
}

Volatility StrippedYoYInflationOptionletVol::volatilityImpl(Time length, Rate strike) const {
    calculate();

    std::vector<Volatility> vol(nYoYOptionletDates_);
    for (Size i = 0; i < nYoYOptionletDates_; ++i) {
        const std::vector<Rate>& yoyoptionletStrikes = StrippedYoYInflationOptionletVol::yoyoptionletStrikes(i);
        const std::vector<Volatility>& yoyoptionletVolatilities =
            StrippedYoYInflationOptionletVol::yoyoptionletVolatilities(i);
        QuantLib::ext::shared_ptr<LinearInterpolation> tmp(new LinearInterpolation(
            yoyoptionletStrikes.begin(), yoyoptionletStrikes.end(), yoyoptionletVolatilities.begin()));
        vol[i] = tmp->operator()(strike, true);
    }

    const std::vector<Time>& yoyoptionletTimes = yoyoptionletFixingTimes();
    QuantLib::ext::shared_ptr<LinearInterpolation> timeVolInterpolator(
        new LinearInterpolation(yoyoptionletTimes.begin(), yoyoptionletTimes.end(), vol.begin()));
    return timeVolInterpolator->operator()(length, true);
}

void StrippedYoYInflationOptionletVol::performCalculations() const {
    for (Size i = 0; i < nYoYOptionletDates_; ++i)
        for (Size j = 0; j < nStrikes_; ++j)
            yoyoptionletVolatilities_[i][j] = yoyoptionletVolQuotes_[i][j]->value();
}

Rate StrippedYoYInflationOptionletVol::minStrike() const { return yoyoptionletStrikes(0).front(); }

Rate StrippedYoYInflationOptionletVol::maxStrike() const { return yoyoptionletStrikes(0).back(); }

Date StrippedYoYInflationOptionletVol::maxDate() const { return yoyoptionletFixingDates().back(); }

const vector<Rate>& StrippedYoYInflationOptionletVol::yoyoptionletStrikes(Size i) const {
    QL_REQUIRE(i < yoyoptionletStrikes_.size(), "index (" << i << ") must be less than yoyoptionletStrikes size ("
                                                          << yoyoptionletStrikes_.size() << ")");
    return yoyoptionletStrikes_[i];
}

const vector<Volatility>& StrippedYoYInflationOptionletVol::yoyoptionletVolatilities(Size i) const {
    QL_REQUIRE(i < yoyoptionletVolatilities_.size(), "index (" << i
                                                               << ") must be less than yoyoptionletVolatilities size ("
                                                               << yoyoptionletVolatilities_.size() << ")");
    return yoyoptionletVolatilities_[i];
}

const vector<Date>& StrippedYoYInflationOptionletVol::yoyoptionletFixingDates() const { return yoyoptionletDates_; }

const vector<Time>& StrippedYoYInflationOptionletVol::yoyoptionletFixingTimes() const { return yoyoptionletTimes_; }

DayCounter StrippedYoYInflationOptionletVol::dayCounter() const { return dc_; }

Calendar StrippedYoYInflationOptionletVol::calendar() const { return calendar_; }

Natural StrippedYoYInflationOptionletVol::settlementDays() const { return settlementDays_; }

BusinessDayConvention StrippedYoYInflationOptionletVol::businessDayConvention() const { return businessDayConvention_; }

VolatilityType StrippedYoYInflationOptionletVol::volatilityType() const { return type_; }

Real StrippedYoYInflationOptionletVol::displacement() const { return displacement_; }

} // namespace QuantExt
