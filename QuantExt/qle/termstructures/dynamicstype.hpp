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

/*! \file dynamicstype.hpp
    \brief dynamics type definitions
    \ingroup termstructures
*/

#ifndef quantext_dynamics_type_hpp
#define quantext_dynamics_type_hpp

#include <ostream>

namespace QuantExt {

/*! \addtogroup termstructures
    @{
*/

//! Stickyness
enum Stickyness { StickyStrike, StickyLogMoneyness, StickyAbsoluteMoneyness };

//! Reaction to Time Decay
enum ReactionToTimeDecay { ConstantVariance, ForwardForwardVariance };

//! Yield Curve Roll Down
enum YieldCurveRollDown { ConstantDiscounts, ForwardForward };

/*! @} */

inline std::ostream& operator<<(std::ostream& out, const Stickyness& t) {
    switch (t) {
    case StickyStrike:
        return out << "StickyStrike";
    case StickyLogMoneyness:
        return out << "StickyLogMoneyness";
    case StickyAbsoluteMoneyness:
        return out << "StickyAbsoluteMoneyness";
    default:
        return out << "Unknown stickyness type (" << t << ")";
    }
}

inline std::ostream& operator<<(std::ostream& out, const ReactionToTimeDecay& t) {
    switch (t) {
    case ConstantVariance:
        return out << "ConstantVariance";
    case ForwardForwardVariance:
        return out << "ForwardForwardVariance";
    default:
        return out << "Unknown reaction to time decay type (" << t << ")";
    }
}

inline std::ostream& operator<<(std::ostream& out, const YieldCurveRollDown& t) {
    switch (t) {
    case ConstantDiscounts:
        return out << "ConstantDiscounts";
    case ForwardForwardVariance:
        return out << "ForwardForward";
    default:
        return out << "Unknown yield curve roll down type (" << t << ")";
    }
}

} // namespace QuantExt

#endif
