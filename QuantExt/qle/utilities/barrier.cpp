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

/*! \file qle/utilities/barrier.hpp
    \brief barrier related utilities
*/

#include <qle/utilities/barrier.hpp>

#include <ql/errors.hpp>

namespace QuantExt {

bool checkBarrier(const double spot, const QuantLib::Barrier::Type type, const double barrier, const int strictComparison) {
    switch (type) {
    case QuantLib::Barrier::DownIn:
    case QuantLib::Barrier::DownOut:
        if (strictComparison == 1) {
            return spot < barrier;
        } else {
            return spot <= barrier;
        }       
    case QuantLib::Barrier::UpIn:
    case QuantLib::Barrier::UpOut:
        if (strictComparison == 1) {
            return spot > barrier;
        } else {
            return spot >= barrier;
        }
    default:
        QL_FAIL("unhandled barrier type " << type);
    }
}

} // namespace QuantExt
