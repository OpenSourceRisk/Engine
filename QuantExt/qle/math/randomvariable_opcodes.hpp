/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#pragma once

#include <string>
#include <vector>

namespace QuantExt {

struct RandomVariableOpCode {
    static constexpr std::size_t None = 0;
    static constexpr std::size_t Add = 1;
    static constexpr std::size_t Subtract = 2;
    static constexpr std::size_t Negative = 3;
    static constexpr std::size_t Mult = 4;
    static constexpr std::size_t Div = 5;
    static constexpr std::size_t ConditionalExpectation = 6;
    static constexpr std::size_t IndicatorEq = 7;
    static constexpr std::size_t IndicatorGt = 8;
    static constexpr std::size_t IndicatorGeq = 9;
    static constexpr std::size_t Min = 10;
    static constexpr std::size_t Max = 11;
    static constexpr std::size_t Abs = 12;
    static constexpr std::size_t Exp = 13;
    static constexpr std::size_t Sqrt = 14;
    static constexpr std::size_t Log = 15;
    static constexpr std::size_t Pow = 16;
    static constexpr std::size_t NormalCdf = 17;
    static constexpr std::size_t NormalPdf = 18;
};

// random variable operation labels

inline std::vector<std::string> getRandomVariableOpLabels() {
    static std::vector<std::string> tmp = {
        "None",        "Add",         "Subtract",     "Negative",  "Mult",     "Div", "ConditionalExpectation",
        "IndicatorEq", "IndicatorGt", "IndicatorGeq", "Min",       "Max",      "Abs", "Exp",
        "Sqrt",        "Log",         "Pow",          "NormalCdf", "NormalPdf"};

    return tmp;
}

} // namespace QuantExt
