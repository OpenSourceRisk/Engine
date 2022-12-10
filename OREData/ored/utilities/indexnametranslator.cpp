/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/log.hpp>

#include <ql/errors.hpp>

namespace ore {
namespace data {

std::string IndexNameTranslator::oreName(const std::string& qlName) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    auto n = data_.left.find(qlName);
    QL_REQUIRE(n != data_.left.end(), "IndexNameTranslator: qlName '" << qlName << "' not found.");
    return n->second;
}

std::string IndexNameTranslator::qlName(const std::string& oreName) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    auto n = data_.right.find(oreName);
    QL_REQUIRE(n != data_.right.end(), "IndexNameTranslator: oreName '" << oreName << "' not found.");
    return n->second;
}

void IndexNameTranslator::add(const std::string& qlName, const std::string& oreName) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    data_.insert(boost::bimap<std::string, std::string>::value_type(qlName, oreName));
    TLOG("IndexNameTranslator: adding '" << qlName << "' <-> '" << oreName << "'");
}

void IndexNameTranslator::clear() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    data_.clear();
}

} // namespace data
} // namespace ore
