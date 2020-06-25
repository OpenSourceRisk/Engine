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

#include <ored/marketdata/fittedbondcurvehelpermarket.hpp>

#include <ored/utilities/indexparser.hpp>

#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

namespace ore {
namespace data {

FittedBondCurveHelperMarket::FittedBondCurveHelperMarket(
    const std::map<std::string, Handle<YieldTermStructure>>& iborIndexCurves, const Conventions& conventions) {

    // populate the ibor index curves
    for (auto const& c : iborIndexCurves)
        iborIndices_[std::make_pair(Market::defaultConfiguration, c.first)] =
            Handle<IborIndex>(parseIborIndex(c.first, c.second,
                                             conventions.has(c.first, Convention::Type::IborIndex) ||
                                                     conventions.has(c.first, Convention::Type::OvernightIndex)
                                                 ? conventions.get(c.first)
                                                 : nullptr));
}

Handle<YieldTermStructure> FittedBondCurveHelperMarket::yieldCurve(const string& name,
                                                                   const string& configuration) const {
    return Handle<YieldTermStructure>(boost::make_shared<FlatForward>(0, NullCalendar(), 0.0, Actual365Fixed()));
}

Handle<Quote> FittedBondCurveHelperMarket::securitySpread(const string& securityID, const string& configuration) const {
    return Handle<Quote>(boost::make_shared<SimpleQuote>(0.0));
}

Handle<DefaultProbabilityTermStructure> FittedBondCurveHelperMarket::defaultCurve(const string&,
                                                                                  const string& configuration) const {
    return Handle<DefaultProbabilityTermStructure>(
        boost::make_shared<FlatHazardRate>(0, NullCalendar(), 0.0, Actual365Fixed()));
}

Handle<Quote> FittedBondCurveHelperMarket::recoveryRate(const string&, const string& configuration) const {
    return Handle<Quote>(boost::make_shared<SimpleQuote>(0.0));
}

} // namespace data
} // namespace ore
