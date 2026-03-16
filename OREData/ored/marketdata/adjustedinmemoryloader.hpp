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

#pragma once

#include <ored/marketdata/inmemoryloader.hpp>
#include <ored/marketdata/adjustmentfactors.hpp>

namespace ore {
namespace data {

//! An Adjusted In Memory Loader,
//  takes unadjusted market data and a set of adjustment factors and returns adjusted data
class AdjustedInMemoryLoader : public ore::data::InMemoryLoader {
public:
    AdjustedInMemoryLoader(const ore::data::AdjustmentFactors& factors) : factors_(factors){};

    // add a market datum
    void add(QuantLib::Date date, const std::string& name, QuantLib::Real value) override;
    // add a fixing
    void addFixing(QuantLib::Date date, const std::string& name, QuantLib::Real value) override;

    // return the adjustment factors
    ore::data::AdjustmentFactors adjustmentFactors() const { return factors_; }

private:
    ore::data::AdjustmentFactors factors_;
};

} // namespace data
} // namespace ore
