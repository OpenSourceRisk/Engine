/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file dynamicstype.hpp
    \brief dynamics type definitions
*/

#ifndef quantext_dynamics_type_hpp
#define quantext_dynamics_type_hpp

#include <iostream>

namespace QuantExt {

enum Stickyness { StickyStrike, StickyLogMoneyness, StickyAbsoluteMoneyness };

enum ReactionToTimeDecay { ConstantVariance, ForwardForwardVariance };

inline std::ostream &operator<<(std::ostream &out, const Stickyness &t) {
    switch (t) {
    case StickyStrike:
        return out << "StickyStrike";
    case StickyLogMoneyness:
        return out << "StickyLogMoneyness";
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
        return out << "Unknown volatility type (" << t << ")";
    }
};

} // namesapce QuantExt

#endif
