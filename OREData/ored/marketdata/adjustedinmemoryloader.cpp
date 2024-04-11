/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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
#include <ored/utilities/indexparser.hpp>
#include <ored/marketdata/adjustedinmemoryloader.hpp>

namespace ore {
namespace data {

void AdjustedInMemoryLoader::add(Date date, const string& name, Real value) {
    Real factor = 1.0;
    try {
        auto datum = parseMarketDatum(date, name, Null<Real>());
        if (auto eqDatum = QuantLib::ext::dynamic_pointer_cast<EquitySpotQuote>(datum))
            factor = factors_.getFactor(eqDatum->eqName(), date);
        else if (auto eqDatum = QuantLib::ext::dynamic_pointer_cast<EquityForwardQuote>(datum))
            factor = factors_.getFactor(eqDatum->eqName(), date);        
    } catch (const std::exception& e) {
        DLOG("AdjustedInMemoryLoader failure on " << name << ": " << e.what());
    }
    InMemoryLoader::add(date, name, value * factor);
}

void AdjustedInMemoryLoader::addFixing(Date date, const string& name, Real value) {
    Real factor = 1.0;
    try {
        auto ind = parseEquityIndex(name);
        factor = factors_.getFactor(ind->name(), date);
    } catch (...) {
    }
    InMemoryLoader::addFixing(date, name, value * factor);
}

} // namespace data
} // namespace ore
