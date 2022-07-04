/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <qle/termstructures/datedstrippedoptionlet.hpp>

#include <ql/utilities/dataformatters.hpp>

#include <boost/algorithm/cxx11/is_sorted.hpp>

using boost::algorithm::is_strictly_increasing;

namespace QuantExt {

DatedStrippedOptionlet::DatedStrippedOptionlet(const Date& referenceDate,
                                               const boost::shared_ptr<StrippedOptionletBase>& s)
    : referenceDate_(referenceDate), calendar_(s->calendar()), businessDayConvention_(s->businessDayConvention()),
      optionletDates_(s->optionletFixingDates()), nOptionletDates_(s->optionletMaturities()),
      optionletTimes_(s->optionletFixingTimes()), optionletStrikes_(nOptionletDates_, vector<Rate>()),
      optionletVolatilities_(nOptionletDates_, vector<Volatility>()), optionletAtmRates_(s->atmOptionletRates()),
      dayCounter_(s->dayCounter()), type_(s->volatilityType()), displacement_(s->displacement()) {

    // Populate the optionlet strikes and volatilities
    for (Size i = 0; i < nOptionletDates_; ++i) {
        optionletStrikes_[i] = s->optionletStrikes(i);
        optionletVolatilities_[i] = s->optionletVolatilities(i);
    }
}

DatedStrippedOptionlet::DatedStrippedOptionlet(const Date& referenceDate, const Calendar& calendar,
                                               BusinessDayConvention bdc, const vector<Date>& optionletDates,
                                               const vector<vector<Rate> >& strikes,
                                               const vector<vector<Volatility> >& volatilities,
                                               const vector<Rate>& optionletAtmRates, const DayCounter& dayCounter,
                                               VolatilityType type, Real displacement)
    : referenceDate_(referenceDate), calendar_(calendar), businessDayConvention_(bdc), optionletDates_(optionletDates),
      nOptionletDates_(optionletDates.size()), optionletTimes_(nOptionletDates_), optionletStrikes_(strikes),
      optionletVolatilities_(volatilities), optionletAtmRates_(optionletAtmRates), dayCounter_(dayCounter), type_(type),
      displacement_(displacement) {

    checkInputs();
    // Populate the optionlet times
    for (Size i = 0; i < nOptionletDates_; ++i)
        optionletTimes_[i] = dayCounter_.yearFraction(referenceDate_, optionletDates_[i]);
}

void DatedStrippedOptionlet::checkInputs() const {

    QL_REQUIRE(!optionletDates_.empty(), "Need at least one optionlet to create optionlet surface");
    QL_REQUIRE(nOptionletDates_ == optionletVolatilities_.size(),
               "Mismatch between number of option tenors (" << nOptionletDates_ << ") and number of volatility rows ("
                                                            << optionletVolatilities_.size() << ")");
    QL_REQUIRE(nOptionletDates_ == optionletStrikes_.size(), "Mismatch between number of option tenors ("
                                                                 << nOptionletDates_ << ") and number of strike rows ("
                                                                 << optionletStrikes_.size() << ")");
    QL_REQUIRE(nOptionletDates_ == optionletAtmRates_.size(), "Mismatch between number of option tenors ("
                                                                  << nOptionletDates_ << ") and number of ATM rates ("
                                                                  << optionletAtmRates_.size() << ")");
    QL_REQUIRE(optionletDates_[0] > referenceDate_,
               "First option date (" << optionletDates_[0] << ") must be greater than the reference date");
    QL_REQUIRE(is_strictly_increasing(optionletDates_.begin(), optionletDates_.end()),
               "Optionlet dates must be sorted in ascending order");

    for (Size i = 0; i < nOptionletDates_; ++i) {
        QL_REQUIRE(!optionletStrikes_[i].empty(), "The " << io::ordinal(i) << " row of strikes is empty");
        QL_REQUIRE(optionletStrikes_[i].size() == optionletVolatilities_[i].size(),
                   "Size of " << io::ordinal(i) << " row of strikes and volatilities are not equal");
        QL_REQUIRE(is_strictly_increasing(optionletStrikes_[i].begin(), optionletStrikes_[i].end()),
                   "The " << io::ordinal(i) << " row of strikes is not sorted in ascending order");
    }
}

const vector<Rate>& DatedStrippedOptionlet::optionletStrikes(Size i) const {
    QL_REQUIRE(i < optionletStrikes_.size(),
               "index (" << i << ") must be less than optionletStrikes size (" << optionletStrikes_.size() << ")");
    return optionletStrikes_[i];
}

const vector<Volatility>& DatedStrippedOptionlet::optionletVolatilities(Size i) const {
    QL_REQUIRE(i < optionletVolatilities_.size(), "index (" << i << ") must be less than optionletVolatilities size ("
                                                            << optionletVolatilities_.size() << ")");
    return optionletVolatilities_[i];
}

const vector<Date>& DatedStrippedOptionlet::optionletFixingDates() const { return optionletDates_; }

const vector<Time>& DatedStrippedOptionlet::optionletFixingTimes() const { return optionletTimes_; }

Size DatedStrippedOptionlet::optionletMaturities() const { return nOptionletDates_; }

const vector<Time>& DatedStrippedOptionlet::atmOptionletRates() const { return optionletAtmRates_; }

const DayCounter& DatedStrippedOptionlet::dayCounter() const { return dayCounter_; }

const Calendar& DatedStrippedOptionlet::calendar() const { return calendar_; }

const Date& DatedStrippedOptionlet::referenceDate() const { return referenceDate_; }

BusinessDayConvention DatedStrippedOptionlet::businessDayConvention() const { return businessDayConvention_; }

VolatilityType DatedStrippedOptionlet::volatilityType() const { return type_; }

Real DatedStrippedOptionlet::displacement() const { return displacement_; }
} // namespace QuantExt
