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

using std::string;

namespace ore {
namespace data {

boost::shared_ptr<ReferenceDatum> ReferenceDatumFactory::build(const string& refDatumType) {
    auto it = map_.find(refDatumType);
    if (it == map_.end())
        return nullptr;
    return it->second()->build();
}

void ReferenceDatumFactory::addBuilder(const string& refDatumType,
                                       std::function<boost::shared_ptr<AbstractReferenceDatumBuilder>()> builder) {
    map_[refDatumType] = builder;
}

} // namespace data
} // namespace ore
