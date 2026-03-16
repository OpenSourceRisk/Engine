/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <qle/cashflows/typedcashflow.hpp>

using std::ostream;

namespace QuantExt {

TypedCashFlow::Type TypedCashFlow::type() const { return type_; }

ostream& operator<<(ostream& out, const TypedCashFlow::Type& type) {
    switch (type) {
    case TypedCashFlow::Type::Interest:
        return out << "Interest";
    case TypedCashFlow::Type::Notional:
        return out << "Notional";
    case TypedCashFlow::Type::Fee:
        return out << "Fee";
    case TypedCashFlow::Type::Premium:
        return out << "Premium";
    case TypedCashFlow::Type::Unspecified:
        return out << "Unspecified";
    default:
        QL_FAIL("Unknown Type for TypedCashFlow");
    }
}

} // namespace QuantExt
