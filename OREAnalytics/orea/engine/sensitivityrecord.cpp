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

#include <ql/math/comparison.hpp>
#include <iomanip>

using QuantLib::close_enough;
using std::ostream;
using std::boolalpha;
using std::fixed;
using std::setprecision;

namespace ore {
namespace analytics {

bool SensitivityRecord::operator==(const SensitivityRecord& sr) const {
    // Note the use of close_enough for comparison of shift sizes
    return tradeId == sr.tradeId &&
        isPar == sr.isPar &&
        key_1 == sr.key_1 &&
        desc_1 == sr.desc_1 &&
        close_enough(shift_1, sr.shift_1) &&
        key_2 == sr.key_2 &&
        desc_2 == sr.desc_2 &&
        close_enough(shift_2, sr.shift_2) &&
        currency == sr.currency;
}

bool SensitivityRecord::operator<(const SensitivityRecord& sr) const {
    return tradeId < sr.tradeId &&
        isPar < sr.isPar &&
        key_1 < sr.key_1 &&
        desc_1 < sr.desc_1 &&
        (!close_enough(shift_1, sr.shift_1) && shift_1 < sr.shift_1) &&
        key_2 < sr.key_2 &&
        desc_2 < sr.desc_2 &&
        (!close_enough(shift_2, sr.shift_2) && shift_2 < sr.shift_2) &&
        currency < sr.currency;
}

bool SensitivityRecord::operator!=(const SensitivityRecord& sr) const {
    return !(*this == sr);
}

SensitivityRecord::operator bool() const {
    // A SensitivityRecord is false if it is default initialised
    return *this != SensitivityRecord();
}

std::ostream& operator<<(ostream& out, const SensitivityRecord& sr) {
    return out << "[" <<
        sr.tradeId << ", " <<
        std::boolalpha << sr.isPar << ", " <<
        sr.key_1 << ", " <<
        sr.desc_1 << ", " <<
        fixed << setprecision(6) << sr.shift_1 << ", " <<
        sr.key_2 << ", " <<
        sr.desc_2 << ", " <<
        fixed << setprecision(6) << sr.shift_2 << ", " <<
        sr.currency << ", " <<
        fixed << setprecision(2) << sr.baseNpv << ", " <<
        fixed << setprecision(2) << sr.delta << ", " <<
        fixed << setprecision(2) << sr.gamma <<
    "]";
}

}
}
