/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <orea/engine/sensitivityrecord.hpp>

#include <iomanip>
#include <ql/math/comparison.hpp>

using QuantLib::close;
using std::boolalpha;
using std::fixed;
using std::ostream;
using std::setprecision;

namespace ore {
namespace analytics {

bool SensitivityRecord::operator==(const SensitivityRecord& sr) const {
    // Define in terms of operator<
    return !(*this < sr) && !(sr < *this);
}

bool SensitivityRecord::operator<(const SensitivityRecord& sr) const {
    // Verbose, would be nice to use std::tie but want to use close on the shifts

    if (key_1 != sr.key_1)
        return key_1 < sr.key_1;

    if (key_2 != sr.key_2)
        return key_2 < sr.key_2;

    if (tradeId != sr.tradeId)
        return tradeId < sr.tradeId;

    return false;
}

bool SensitivityRecord::operator!=(const SensitivityRecord& sr) const {
    // Define in terms of operator==
    return !(*this == sr);
}

SensitivityRecord::operator bool() const {
    // A SensitivityRecord is false if it is default initialised
    return *this != SensitivityRecord();
}

bool SensitivityRecord::isCrossGamma() const { return key_2 != RiskFactorKey(); }

std::ostream& operator<<(ostream& out, const SensitivityRecord& sr) {
    return out << "[" << sr.tradeId << ", " << std::boolalpha << sr.isPar << ", " << sr.key_1 << ", " << sr.desc_1
               << ", " << fixed << setprecision(6) << sr.shift_1 << ", " << sr.key_2 << ", " << sr.desc_2 << ", "
               << fixed << setprecision(6) << sr.shift_2 << ", " << sr.currency << ", " << fixed << setprecision(2)
               << sr.baseNpv << ", " << fixed << setprecision(2) << sr.delta << ", " << fixed << setprecision(2)
               << sr.gamma << "]";
}

} // namespace analytics
} // namespace ore
