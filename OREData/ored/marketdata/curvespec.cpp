/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ored/marketdata/curvespec.hpp>

namespace ore {
namespace data {

bool operator<(const CurveSpec& lhs, const CurveSpec& rhs) {
    if (lhs == rhs) {
        /* If CurveSpecs are equal (i.e. have same name), return false. */
        return false;
    } else if (lhs.baseType() != rhs.baseType()) {
        /* If types are not the same, use the enum value for ordering along type.
         * The enum order in the header file is deliberate, it ensures that FX comes before FXVol
         * this property is used in data::order() (see curveloader.hpp)
         */
        return lhs.baseType() < rhs.baseType();
    } else {
        /* If types are the same and CurveSpecs are different, use default < for string */
        return lhs.name() < rhs.name();
    }
}

bool operator==(const CurveSpec& lhs, const CurveSpec& rhs) {
    /* We consider two CurveSpecs equal if they have the same name. */
    return lhs.name() == rhs.name();
}

bool operator<(const boost::shared_ptr<CurveSpec>& lhs, const boost::shared_ptr<CurveSpec>& rhs) { return *lhs < *rhs; }

bool operator==(const boost::shared_ptr<CurveSpec>& lhs, const boost::shared_ptr<CurveSpec>& rhs) {
    return *lhs == *rhs;
}

std::ostream& operator<<(std::ostream& os, const CurveSpec& spec) { return os << spec.name(); }
} // namespace data
} // namespace ore
