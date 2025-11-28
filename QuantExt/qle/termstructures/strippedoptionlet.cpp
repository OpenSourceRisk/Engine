/*
 Copyright (C) 2025 AcadiaSoft Inc.
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

#include <qle/termstructures/strippedoptionlet.hpp>
#include <ql/quotes/simplequote.hpp>

using std::vector;

namespace QuantExt {

StrippedOptionlet::StrippedOptionlet(Natural settlementDays,
                                     const Calendar& calendar,
                                     BusinessDayConvention bdc,
                                     ext::shared_ptr<IborIndex> iborIndex,
                                     const vector<Date>& optionletDates,
                                     const vector<Rate>& strikes,
                                     const Handle<OptionletVolatilityStructure>& baseVol,
                                     vector<vector<Handle<Quote>>> quotes,
                                     DayCounter dc,
                                     VolatilityType type,
                                     Real displacement,
                                     const vector<Real>& atmOptionletRates)
    : StrippedOptionlet(settlementDays,
                        calendar,
                        bdc,
                        std::move(iborIndex),
                        optionletDates,
                        vector<vector<Rate>>(optionletDates.size(), strikes),
                        baseVol,
                        quotes,
                        std::move(dc),
                        type,
                        displacement,
                        atmOptionletRates) {}

StrippedOptionlet::StrippedOptionlet(Natural settlementDays,
                                     const Calendar& calendar,
                                     BusinessDayConvention bdc,
                                     ext::shared_ptr<IborIndex> iborIndex,
                                     const vector<Date>& optionletDates,
                                     const vector<vector<Rate>>& strikes,
                                     const Handle<OptionletVolatilityStructure>& baseVol,
                                     vector<vector<Handle<Quote>>> quotes,
                                     DayCounter dc,
                                     VolatilityType type,
                                     Real displacement,
                                     const vector<Real>& atmOptionletRates)
    : QuantLib::StrippedOptionlet(settlementDays, calendar, bdc, iborIndex, optionletDates,
                                  strikes,
                                  quotes,
                                  dc, type, displacement, atmOptionletRates),
                                  baseVol_(baseVol), quotes_(quotes) {
    registerWith(baseVol);
    for (auto x : quotes_)
        for (auto y : x)
            unregisterWith(y);
}

void StrippedOptionlet::performCalculations() const {
    // fill the quotes matrix from the base vol structure
    for (Size i = 0; i < optionletFixingDates().size(); ++i) {
        Date d = optionletFixingDates()[i];
        for (Size j = 0; j < optionletStrikes(i).size(); ++j) {
            Rate k = optionletStrikes(i)[j];
            Volatility vol = baseVol_->volatility(d, k, true);
            auto sq = ext::dynamic_pointer_cast<SimpleQuote>(*quotes_[i][j]);
            QL_REQUIRE(sq, "StrippedOptionlet::performCalculations(): null SimpleQuote");
            sq->setValue(vol);
        }
    }
    QuantLib::StrippedOptionlet::performCalculations();
}

} // namespace QuantExt
