/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <orea/scenario/scenario.hpp>

namespace openriskengine {
namespace analytics {

std::ostream& operator<<(std::ostream& out, const RiskFactorKey::KeyType& type) {
    switch (type) {
    case RiskFactorKey::KeyType::DiscountCurve:
        return out << "DiscountCurve";
    case RiskFactorKey::KeyType::YieldCurve:
        return out << "YieldCurve";
    case RiskFactorKey::KeyType::IndexCurve:
        return out << "IndexCurve";
    case RiskFactorKey::KeyType::SwaptionVolatility:
        return out << "SwaptionVolatility";
    case RiskFactorKey::KeyType::FXSpot:
        return out << "FXSpot";
    case RiskFactorKey::KeyType::FXVolatility:
        return out << "FXVolatility";
    default:
        return out << "?";
    }
}

std::ostream& operator<<(std::ostream& out, const RiskFactorKey& key) {
    return out << key.keytype << "/" << key.name << "/" << key.index;
}
}
}
