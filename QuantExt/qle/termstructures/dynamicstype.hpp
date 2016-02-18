/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file dynamicstype.hpp
    \brief dynamics type definitions
*/

#ifndef quantext_dynamics_type_hpp
#define quantext_dynamics_type_hpp

#include <ostream>

namespace QuantExt {

enum Stickyness { StickyStrike, StickyLogMoneyness, StickyAbsoluteMoneyness };

enum ReactionToTimeDecay { ConstantVariance, ForwardForwardVariance };

enum YieldCurveRollDown { ConstantDiscounts, ForwardForward };

inline std::ostream &operator<<(std::ostream &out, const Stickyness &t) {
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
};

inline std::ostream &operator<<(std::ostream &out,
                                const ReactionToTimeDecay &t) {
    switch (t) {
    case ConstantVariance:
        return out << "ConstantVariance";
    case ForwardForwardVariance:
        return out << "ForwardForwardVariance";
    default:
        return out << "Unknown reaction to time decay type (" << t << ")";
    }
};

inline std::ostream &operator<<(std::ostream &out,
                                const YieldCurveRollDown &t) {
    switch (t) {
    case ConstantDiscounts:
        return out << "ConstantDiscounts";
    case ForwardForwardVariance:
        return out << "ForwardForward";
    default:
        return out << "Unknown yield curve roll down type (" << t << ")";
    }
};

} // namesapce QuantExt

#endif
