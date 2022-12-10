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

#include <algorithm>
#include <qle/termstructures/crosscurrencypricetermstructure.hpp>

using QuantLib::Date;
using QuantLib::Handle;
using QuantLib::Natural;
using QuantLib::Quote;
using QuantLib::Time;
using QuantLib::YieldTermStructure;
using std::max;
using std::min;
using std::vector;

namespace QuantExt {

CrossCurrencyPriceTermStructure::CrossCurrencyPriceTermStructure(
    const QuantLib::Date& referenceDate, const QuantLib::Handle<PriceTermStructure>& basePriceTs,
    const QuantLib::Handle<QuantLib::Quote>& fxSpot,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& baseCurrencyYts,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& yts, const QuantLib::Currency& currency)
    : PriceTermStructure(referenceDate, basePriceTs->calendar(), basePriceTs->dayCounter()), basePriceTs_(basePriceTs),
      fxSpot_(fxSpot), baseCurrencyYts_(baseCurrencyYts), yts_(yts), currency_(currency) {
    registration();
}

CrossCurrencyPriceTermStructure::CrossCurrencyPriceTermStructure(
    Natural settlementDays, const QuantLib::Handle<PriceTermStructure>& basePriceTs,
    const QuantLib::Handle<QuantLib::Quote>& fxSpot,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& baseCurrencyYts,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& yts, const QuantLib::Currency& currency)
    : PriceTermStructure(settlementDays, basePriceTs->calendar(), basePriceTs->dayCounter()), basePriceTs_(basePriceTs),
      fxSpot_(fxSpot), baseCurrencyYts_(baseCurrencyYts), yts_(yts), currency_(currency) {
    registration();
}

Date CrossCurrencyPriceTermStructure::maxDate() const {
    return min(basePriceTs_->maxDate(), min(baseCurrencyYts_->maxDate(), yts_->maxDate()));
}

Time CrossCurrencyPriceTermStructure::maxTime() const {
    return min(basePriceTs_->maxTime(), min(baseCurrencyYts_->maxTime(), yts_->maxTime()));
}

Time CrossCurrencyPriceTermStructure::minTime() const { return basePriceTs_->minTime(); }

vector<Date> CrossCurrencyPriceTermStructure::pillarDates() const { return basePriceTs_->pillarDates(); }

QuantLib::Real CrossCurrencyPriceTermStructure::priceImpl(QuantLib::Time t) const {
    // Price in base currency times the FX forward, number of units of currency per unit of base currency.
    return basePriceTs_->price(t, true) * fxSpot_->value() * baseCurrencyYts_->discount(t, true) /
           yts_->discount(t, true);
}

void CrossCurrencyPriceTermStructure::registration() {
    registerWith(basePriceTs_);
    registerWith(fxSpot_);
    registerWith(baseCurrencyYts_);
    registerWith(yts_);
}

} // namespace QuantExt
