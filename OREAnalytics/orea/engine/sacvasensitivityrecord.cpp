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

#include <orea/engine/sacvasensitivityrecord.hpp>
#include <ored/utilities/to_string.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/assign.hpp>
#include <boost/bimap.hpp>

#include <iomanip>
#include <ql/math/comparison.hpp>

namespace ore {
namespace analytics {
using QuantLib::close;
using std::ostream;
using std::setprecision;
using boost::assign::list_of;
using std::map;
using std::ostream;
using std::set;
using std::string;
using std::vector;
using ore::data::to_string;

// custom comparator for bimaps
struct string_cmp {
    bool operator()(const string& lhs, const string& rhs) const {
        return ((boost::to_lower_copy(lhs)) < (boost::to_lower_copy(rhs)));
    }
};

// Ease the notation below
template <typename T> using bm = boost::bimap<T, boost::bimaps::set_of<std::string, string_cmp>>;

const bm<CvaType> cvaTypeMap =
    list_of<bm<CvaType>::value_type>(CvaType::CvaAggregate, "CvaAggregate")(
        CvaType::CvaHedge, "CvaHedge");

bool SaCvaSensitivityRecord::operator==(const SaCvaSensitivityRecord& sr) const {
    return  std::tie(nettingSetId, riskType, cvaType, marginType, riskFactor, bucket) ==
            std::tie(sr.nettingSetId, sr.riskType, sr.cvaType, sr.marginType, sr.riskFactor, sr.bucket);
}

bool SaCvaSensitivityRecord::operator<(const SaCvaSensitivityRecord& sr) const {
    return  std::tie(nettingSetId, riskType, cvaType, marginType, riskFactor, bucket) <
            std::tie(sr.nettingSetId, sr.riskType, sr.cvaType, sr.marginType, sr.riskFactor, sr.bucket);
}

std::ostream& operator<<(ostream& out, const SaCvaSensitivityRecord& sr) {
    return out << "["  << sr.nettingSetId << ", " << sr.riskType << ", " << sr.cvaType << ", " << sr.marginType
               << ", " << sr.riskFactor << ", " << sr.bucket << ", " << setprecision(6) << sr.value << "]";
}

ostream& operator<<(ostream& out, const CvaType& mt) {
    QL_REQUIRE(cvaTypeMap.left.count(mt) > 0, "Margin type not a valid CvaType");
    return out << cvaTypeMap.left.at(mt);
}

CvaType parseCvaType(const string& mt) {
    QL_REQUIRE(cvaTypeMap.right.count(mt) > 0,
               "CVA type string " << mt << " does not correspond to a valid CvaType");
    return cvaTypeMap.right.at(mt);
}

} // namespace analytics
} // namespace ore
