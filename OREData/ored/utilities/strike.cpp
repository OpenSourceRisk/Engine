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

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

#include <ql/errors.hpp>
#include <regex>

namespace ore {
namespace data {

Strike parseStrike(const std::string& s) {
    std::regex m1("^(ATM|atm)");
    std::regex m1b("^(ATMF|atmf)");
    std::regex m2("^(ATM|atm)(\\+|\\-)([0-9]+[.]?[0-9]*)");
    std::regex m3("^(\\+|\\-)?([0-9]+[.]?[0-9]*)");
    std::regex m4("^(\\+|\\-)?([0-9]+[.]?[0-9]*)(d|D)");
    std::regex m4b("(d|D)");
    std::regex m5("^(\\+|\\-)?([0-9]+[.]?[0-9]*)(c|C)");
    std::regex m5b("^(c|C)");
    std::regex m6("^(\\+|\\-)?([0-9]+[.]?[0-9]*)(p|P)");
    std::regex m6b("^(p|P)");
    std::regex m7("^(\\+|\\-)?([0-9]+[.]?[0-9]*)(bf|BF)");
    std::regex m7b("^(bf|BF)");
    std::regex m8("^(\\+|\\-)?([0-9]+[.]?[0-9]*)(rr|RR)");
    std::regex m8b("^(rr|RR)");
    std::regex m9("^(\\+|\\-)?([0-9]+[.]?[0-9]*)(ATMF|atmf)");
    std::regex m9b("(ATMF|atmf)");
    std::regex m10("^(\\+|\\-)?([0-9]+[.]?[0-9]*)(ATM|atm)");
    std::regex m10b("(ATM|atm)");
    Strike result;
    if (std::regex_match(s, m1)) {
        result.type = Strike::Type::ATM;
        result.value = 0.0;
        return result;
    }
    if (std::regex_match(s, m1b)) {
        result.type = Strike::Type::ATMF;
        result.value = 0.0;
        return result;
    }
    if (std::regex_match(s, m2)) {
        result.type = Strike::Type::ATM_Offset;
        result.value = parseReal(regex_replace(s, m1, std::string("")));
        return result;
    }
    if (std::regex_match(s, m3)) {
        result.type = Strike::Type::Absolute;
        result.value = parseReal(s);
        return result;
    }
    if (std::regex_match(s, m4)) {
        result.type = Strike::Type::Delta;
        result.value = parseReal(regex_replace(s, m4b, std::string("")));
        return result;
    }
    if (std::regex_match(s, m5)) {
        result.type = Strike::Type::DeltaCall;
        result.value = parseReal(regex_replace(s, m5b, std::string("")));
        return result;
    }
    if (std::regex_match(s, m6)) {
        result.type = Strike::Type::DeltaPut;
        result.value = parseReal(regex_replace(s, m6b, std::string("")));
        return result;
    }
    if (std::regex_match(s, m7)) {
        result.type = Strike::Type::BF;
        result.value = parseReal(regex_replace(s, m7b, std::string("")));
        return result;
    }
    if (std::regex_match(s, m8)) {
        result.type = Strike::Type::RR;
        result.value = parseReal(regex_replace(s, m8b, std::string("")));
        return result;
    }
    if (std::regex_match(s, m9)) {
        result.type = Strike::Type::ATMF_Moneyness;
        result.value = parseReal(regex_replace(s, m9b, std::string("")));
        return result;
    }
    if (std::regex_match(s, m10)) {
        result.type = Strike::Type::ATM_Moneyness;
        result.value = parseReal(regex_replace(s, m10b, std::string("")));
        return result;
    }
    QL_FAIL("could not parse strike given by " << s);
}

std::ostream& operator<<(std::ostream& out, const Strike& s) {
    switch (s.type) {
    case Strike::Type::ATM:
        out << "ATM";
        break;
    case Strike::Type::ATMF:
        out << "ATMF";
        break;
    case Strike::Type::ATM_Offset:
        out << "ATM_Offset";
        break;
    case Strike::Type::Absolute:
        out << "Absolute";
        break;
    case Strike::Type::Delta:
        out << "Delta";
        break;
    case Strike::Type::ATMF_Moneyness:
        out << "ATMF_Moneyness";
        break;
    case Strike::Type::ATM_Moneyness:
        out << "ATM_Moneyness";
        break;
    default:
        out << "UNKNOWN";
        break;
    }
    if (s.type == Strike::Type::ATM_Offset || s.type == Strike::Type::Absolute || s.type == Strike::Type::Delta ||
        s.type == Strike::Type::ATMF_Moneyness || s.type == Strike::Type::ATM_Moneyness) {
        if (s.value >= 0.0)
            out << "+";
        else
            out << "-";
        out << s.value;
    }
    return out;
}

namespace {
Strike normaliseStrike(const Strike& s) {
    if (s.type == Strike::Type::ATM_Offset && QuantLib::close_enough(s.value, 0.0))
        return Strike({Strike::Type::ATM, 0.0});
    if (s.type == Strike::Type::ATMF_Moneyness && QuantLib::close_enough(s.value, 1.0))
        return Strike({Strike::Type::ATMF, 0.0});
    if (s.type == Strike::Type::ATM_Moneyness && QuantLib::close_enough(s.value, 1.0))
        return Strike({Strike::Type::ATM, 0.0});
    return s;
}
} // anonymous namespace

bool operator==(const Strike& s1, const Strike& s2) {
    Strike tmp1 = normaliseStrike(s1);
    Strike tmp2 = normaliseStrike(s2);
    return (tmp1.type == tmp2.type && QuantLib::close_enough(tmp1.value, tmp2.value));
}

QuantLib::Real computeAbsoluteStrike(const Strike& s, const QuantLib::Real atm, const QuantLib::Real atmf) {
    switch (s.type) {
    case Strike::Type::ATM:
        return atm;
    case Strike::Type::ATMF:
        return atmf;
    case Strike::Type::ATM_Offset:
        return atm + s.value;
    case Strike::Type::Absolute:
        return s.value;
    case Strike::Type::Delta:
        QL_FAIL("can not compute absolute strike for type delta");
    case Strike::Type::ATMF_Moneyness:
        return atmf * s.value;
    case Strike::Type::ATM_Moneyness:
        return atm * s.value;
    default:
        QL_FAIL("can not compute strike for unknown type " << s);
    }
}

DeltaString::DeltaString(const std::string& s) {
    QL_REQUIRE(!s.empty() && (s.back() == 'P' || s.back() == 'C' || s == "ATM"),
               "invalid delta quote, expected ATM, 10P, 25C, ...");
    isAtm_ = s == "ATM";
    isPut_ = !s.empty() && s.back() == 'P';
    isCall_ = !s.empty() && s.back() == 'C';
    if (isPut_ || isCall_) {
        try {
            delta_ = ore::data::parseReal(s.substr(0, s.size() - 1)) / 100.0 * (isPut_ ? -1.0 : 1.0);
        } catch (const std::exception& e) {
            QL_FAIL("DeltaString: can not convert call / put delta '" << s << "' to numeric value: " << e.what());
        }
    }
}

} // namespace data
} // namespace ore
