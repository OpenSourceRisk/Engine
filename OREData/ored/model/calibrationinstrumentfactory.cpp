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

#include <ored/model/calibrationinstrumentfactory.hpp>

using std::function;
using std::string;

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<CalibrationInstrument> CalibrationInstrumentFactory::build(const string& instrumentType) {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    auto it = map_.find(instrumentType);
    if (it == map_.end())
        return nullptr;
    return it->second();
}

void CalibrationInstrumentFactory::addBuilder(const string& instrumentType,
                                              function<QuantLib::ext::shared_ptr<CalibrationInstrument>()> builder,
                                              const bool allowOverwrite) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    QL_REQUIRE(map_.insert(std::make_pair(instrumentType, builder)).second,
               "CalibrationInstrumentFactory::addBuilder(" << instrumentType << "): builder for key already exists.");
}
}
}
