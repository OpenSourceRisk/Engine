/*
 Copyright (C) 2026 AcadiaSoft, Inc.
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

#include <qle/math/flatextrapolation2d.hpp>
#include <qle/termstructures/strippedoptionletbasebumped.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/calendar.hpp>

namespace QuantExt {

using namespace QuantLib;
using std::vector;

StrippedOptionletBaseBumped::StrippedOptionletBaseBumped(ext::shared_ptr<StrippedOptionletBase> sob,
    QuoteCurve bumpQuotes, vector<Time> bumpTimes)
    : sob_(std::move(sob)), bumpQuotes_(makeGrid(std::move(bumpQuotes))), bumpTimes_(std::move(bumpTimes)),
      bumpStrikes_({0.0, 0.1}), bumpMatrix_(bumpTimes_.size(), 2, 0.0) {
    init();
}

StrippedOptionletBaseBumped::StrippedOptionletBaseBumped(ext::shared_ptr<StrippedOptionletBase> sob,
    QuoteGrid bumpQuotes, vector<Time> bumpTimes, vector<Rate> bumpStrikes)
    : sob_(std::move(sob)), bumpQuotes_(std::move(bumpQuotes)), bumpTimes_(std::move(bumpTimes)),
      bumpStrikes_(std::move(bumpStrikes)), bumpMatrix_(bumpTimes_.size(), bumpStrikes_.size(), 0.0) {
    init();
}

const vector<Rate>& StrippedOptionletBaseBumped::optionletStrikes(Size i) const {
    return sob_->optionletStrikes(i);
}

const vector<Volatility>& StrippedOptionletBaseBumped::optionletVolatilities(Size i) const {
    calculate();
    QL_REQUIRE(i < volatilities_.size(), "StrippedOptionletBaseBumped: index (" << i << ") must be less "
        "than optionletVolatilities size (" << volatilities_.size() << ")");
    return volatilities_[i];
}

const vector<Date>& StrippedOptionletBaseBumped::optionletFixingDates() const {
    return sob_->optionletFixingDates();
}

const vector<Time>& StrippedOptionletBaseBumped::optionletFixingTimes() const {
    return sob_->optionletFixingTimes();
}

Size StrippedOptionletBaseBumped::optionletMaturities() const {
    return sob_->optionletMaturities();
}

const vector<Time>& StrippedOptionletBaseBumped::atmOptionletRates() const {
    return sob_->atmOptionletRates();
}

DayCounter StrippedOptionletBaseBumped::dayCounter() const {
    return sob_->dayCounter();
}

Calendar StrippedOptionletBaseBumped::calendar() const {
    return sob_->calendar();
}

Natural StrippedOptionletBaseBumped::settlementDays() const {
    return sob_->settlementDays();
}

BusinessDayConvention StrippedOptionletBaseBumped::businessDayConvention() const {
    return sob_->businessDayConvention();
}

VolatilityType StrippedOptionletBaseBumped::volatilityType() const {
    return sob_->volatilityType();
}

Real StrippedOptionletBaseBumped::displacement() const {
    return sob_->displacement();
}

bool StrippedOptionletBaseBumped::useEffectiveVolatility() const {
    return sob_->useEffectiveVolatility();
}

void StrippedOptionletBaseBumped::performCalculations() const {
    // Update the matrix of bumps from the quotes.
    for (Size i = 0; i < bumpTimes_.size(); ++i) {
        for (Size j = 0; j < bumpStrikes_.size(); ++j)
            bumpMatrix_[i][j] = bumpQuotes_[i][j]->value();
    }

    // Populate the volatilities.
    const auto& optFixingTimes = sob_->optionletFixingTimes();
    for (Size i = 0; i < optFixingTimes.size(); ++i) {
        const auto& baseVolRow = sob_->optionletVolatilities(i);
        const auto& baseVolStrikes = sob_->optionletStrikes(i);
        for (Size j = 0; j < baseVolRow.size(); ++j)
            volatilities_[i][j] = baseVolRow[j] + bumpInterp_(baseVolStrikes[j], optFixingTimes[i]);
    }
}

StrippedOptionletBaseBumped::QuoteGrid StrippedOptionletBaseBumped::makeGrid(QuoteCurve quoteCurve) {
    QuoteGrid grid;
    grid.reserve(quoteCurve.size());
    for (auto& quote : quoteCurve)
        grid.emplace_back(QuoteRow{ quote, quote });
    return grid;
}

void StrippedOptionletBaseBumped::init() {

    // Check consistency of bump quotes.
    QL_REQUIRE(!bumpQuotes_.empty(), "StrippedOptionletBaseBumped: bump quotes should not be empty.");
    QL_REQUIRE(bumpQuotes_.size() == bumpTimes_.size(), "StrippedOptionletBaseBumped: there is a mismatch between "
        "number of rows in the quote grid (" << bumpQuotes_.size() << ") and the number of bump times (" <<
        bumpTimes_.size() << ").");
    QL_REQUIRE(bumpQuotes_[0].size() == bumpStrikes_.size(), "StrippedOptionletBaseBumped: there is a mismatch between "
        "number of columns in the quote grid (" << bumpQuotes_[0].size() << ") and the number of bump strikes (" <<
        bumpStrikes_.size() << ").");

    // Register with the underlying StrippedOptionletBase.
    registerWith(sob_);

    // Register with the bump quotes.
    for (const auto& row : bumpQuotes_)
        for (const auto& quote : row)
            registerWith(quote);

    // Initialise the volatilities_ grid.
    Size nOptDates = optionletMaturities();
    volatilities_.reserve(nOptDates);
    for (Size i = 0; i < nOptDates; ++i)
        volatilities_.emplace_back(optionletStrikes(i).size());

    // Initialise the 2D bump interpolation (the parameter order is counter intuitive!).
    bumpInterp_ = BilinearFlat().interpolate(bumpStrikes_.begin(), bumpStrikes_.end(),
        bumpTimes_.begin(), bumpTimes_.end(), bumpMatrix_);
    bumpInterp_.enableExtrapolation();
}

} // namespace QuantExt
