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

#include <qle/termstructures/dynamicstype.hpp>

namespace QuantExt {

Stickyness parseStickyness(const std::string& s) {
    static std::map<std::string, Stickyness> m = {{"StickyStrike", StickyStrike},
                                             {"StickyLogMoneyness", StickyLogMoneyness},
                                             {"StickyDelta", StickyDelta},
                                             {"StickyAbsoluteMoneyness", StickyAbsoluteMoneyness}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Stickyness \"" << s << "\" not recognized");
    }
}

ReactionToTimeDecay parseDecayMode(const std::string& s) {
    static std::map<std::string, ReactionToTimeDecay> m = {{"ForwardVariance", ForwardForwardVariance},
                                                      {"ConstantVariance", ConstantVariance}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Decay mode \"" << s << "\" not recognized");
    }
}

YieldCurveRollDown parseYieldCurveRollDown(const std::string& s) {
    static std::map<std::string, YieldCurveRollDown> m = {{"ConstantDiscounts", ConstantDiscounts},
                                                          {"ForwardForward", ForwardForward}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Decay mode \"" << s << "\" not recognized");
    }
}

std::ostream& operator<<(std::ostream& out, const Stickyness t) {
    switch (t) {
    case StickyStrike:
        return out << "StickyStrike";
    case StickyLogMoneyness:
        return out << "StickyLogMoneyness";
    case StickyDelta:
        return out << "StickyDelta";
    case StickyAbsoluteMoneyness:
        return out << "StickyAbsoluteMoneyness";
    default:
        return out << "Unknown stickiness type (" << t << ")";
    }
}

std::ostream& operator<<(std::ostream& out, const ReactionToTimeDecay t) {
    switch (t) {
    case ConstantVariance:
        return out << "ConstantVariance";
    case ForwardForwardVariance:
        return out << "ForwardForwardVariance";
    default:
        return out << "Unknown reaction to time decay type (" << t << ")";
    }
}

std::ostream& operator<<(std::ostream& out, const YieldCurveRollDown t) {
    switch (t) {
    case ConstantDiscounts:
        return out << "ConstantDiscounts";
    case ForwardForward:
        return out << "ForwardForward";
    default:
        return out << "Unknown yield curve roll down type (" << t << ")";
    }
}

} // namespace QuantExt
