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

#include <orea/engine/cvasensitivityrecord.hpp>

#include <iomanip>
#include <ql/math/comparison.hpp>

using QuantLib::close;
using std::boolalpha;
using std::fixed;
using std::ostream;
using std::setprecision;

namespace ore {
namespace analytics {

bool CvaSensitivityRecord::operator==(const CvaSensitivityRecord& sr) const {
    // Define in terms of operator<
    return !(*this < sr) && !(sr < *this);
}

bool CvaSensitivityRecord::operator<(const CvaSensitivityRecord& sr) const {
    // Verbose, would be nice to use std::tie but want to use close on the shifts

    if (key != sr.key)
        return key < sr.key;

    if (nettingSetId != sr.nettingSetId)
        return nettingSetId < sr.nettingSetId;

    return false;
}

bool CvaSensitivityRecord::operator!=(const CvaSensitivityRecord& sr) const {
    // Define in terms of operator==
    return !(*this == sr);
}

CvaSensitivityRecord::operator bool() const {
    // A SensitivityRecord is false if it is default initialised
    return *this != CvaSensitivityRecord();
}

std::ostream& operator<<(ostream& out, const CvaSensitivityRecord& sr) {
    return out << "[" << sr.nettingSetId << ", " << sr.key << ", " << sr.shiftType << "," << fixed << setprecision(6) << sr.shiftSize << ", "
        << sr.currency << ", " << fixed << setprecision(2) << sr.baseCva << ", " << fixed << setprecision(2) << sr.delta << "]";
}

} // namespace analytics
} // namespace ore
