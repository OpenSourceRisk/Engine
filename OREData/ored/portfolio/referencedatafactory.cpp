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

#include <ored/portfolio/referencedatafactory.hpp>

#include <ql/errors.hpp>

using std::string;

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<ReferenceDatum> ReferenceDatumFactory::build(const string& refDatumType) {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    auto it = map_.find(refDatumType);
    if (it == map_.end())
        return nullptr;
    return it->second()->build();
}

void ReferenceDatumFactory::addBuilder(const string& refDatumType,
                                       std::function<QuantLib::ext::shared_ptr<AbstractReferenceDatumBuilder>()> builder,
                                       const bool allowOverwrite) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    QL_REQUIRE(map_.insert(std::make_pair(refDatumType, builder)).second,
               "ReferenceDatumFactory::addBuilder(" << refDatumType << "): builder for key already exists.");
}

} // namespace data
} // namespace ore
