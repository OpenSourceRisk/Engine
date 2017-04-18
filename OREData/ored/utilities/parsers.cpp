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

/*! \file ored/utilities/parsers.cpp
    \brief
    \ingroup utilities
*/

#include <ored/utilities/parsers.hpp>
#include <boost/algorithm/string.hpp>
#include <ql/utilities/dataparsers.hpp>
#include <ql/errors.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/all.hpp>
#include <qle/currencies/all.hpp>
#include <ql/currencies/all.hpp>
#include <ql/indexes/all.hpp>
#include <map>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

Date parseDate(const string& s) {
    // TODO: review

    if (s == "")
        return Date();

    // guess formats from token number and sizes
    // check permissible lengths
    QL_REQUIRE((s.size() >= 3 && s.size() <= 6) || s.size() == 8 || s.size() == 10,
               "invalid date format of " << s << ", date string length 8 or 10 or between 3 and 6 required");

    vector<string> tokens;
    boost::split(tokens, s, boost::is_any_of("-/.:"));

    if (tokens.size() == 1) {
        if (s.size() == 8) {
            // yyyymmdd
            int y = parseInteger(s.substr(0, 4));
            int m = parseInteger(s.substr(4, 2));
            int d = parseInteger(s.substr(6, 2));
            return Date(d, Month(m), y);
        } else if (s.size() >= 3 && s.size() <= 6) {
            // Excel format
            // Boundaries will be checked by Date constructor
            // Boundaries are minDate = 367 i.e. Jan 1st, 1901
            // and maxDate = 109574 i.e. Dec 31st, 2199
            BigInteger serial = parseInteger(s);
            return Date(serial);
        }
    } else if (tokens.size() == 3) {
        if (tokens[0].size() == 4) {
            // yyyy-mm-dd
            // yyyy/mm/dd
            // yyyy.mm.dd
            int y = parseInteger(tokens[0]);
            int m = parseInteger(tokens[1]);
            int d = parseInteger(tokens[2]);
            return Date(d, Month(m), y);
        } else if (tokens[0].size() == 2) {
            // dd-mm-yy
            // dd/mm/yy
            // dd.mm.yy
            // dd-mm-yyyy
            // dd/mm/yyyy
            // dd.mm.yyyy
            int d = parseInteger(tokens[0]);
            int m = parseInteger(tokens[1]);
            int y = parseInteger(tokens[2]);
            if (y < 100) {
                if (y > 80)
                    y += 1900;
                else
                    y += 2000;
            }
            return Date(d, Month(m), y);
        }
    }

    QL_FAIL("Cannot convert \"" << s << "\" to Date.");
}

Real parseReal(const string& s) {
    // TODO: review
    return atof(s.c_str());
}

Integer parseInteger(const string& s) { return io::to_integer(s); }

bool parseBool(const string& s) {
    static map<string, bool> b = {{"Y", true},
                                  {"YES", true},
                                  {"TRUE", true},
                                  {"true", true},
                                  {"1", true},
                                  {"N", false},
                                  {"NO", false},
                                  {"FALSE", false},
                                  {"false", false},
                                  {"0", false}};

    auto it = b.find(s);
    if (it != b.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert " << s << " to bool");
    }
}

Calendar parseCalendar(const string& s) {
    static map<string, Calendar> m = {{"TGT", TARGET()},
                                      {"TARGET", TARGET()},
                                      {"EUR", TARGET()},
                                      {"ZUB", Switzerland()},
                                      {"CHF", Switzerland()},
                                      {"US", UnitedStates()},
                                      {"USD", UnitedStates()},
                                      {"NYB", UnitedStates()},
                                      {"US-SET", UnitedStates(UnitedStates::Settlement)},
                                      {"US settlement", UnitedStates(UnitedStates::Settlement)},
                                      {"US-GOV", UnitedStates(UnitedStates::GovernmentBond)},
                                      {"US-NYSE", UnitedStates(UnitedStates::NYSE)},
                                      {"US-NERC", UnitedStates(UnitedStates::NERC)},
                                      {"GB", UnitedKingdom()},
                                      {"GBP", UnitedKingdom()},
                                      {"UK", UnitedKingdom()},
                                      {"UK settlement", UnitedKingdom()},
                                      {"LNB", UnitedKingdom()},
                                      {"CA", Canada()},
                                      {"TRB", Canada()},
                                      {"CAD", Canada()},
                                      {"SYB", Australia()},
                                      {"AU", Australia()},
                                      {"AUD", Australia()},
                                      {"TKB", Japan()},
                                      {"JP", Japan()},
                                      {"JPY", Japan()},
                                      {"ZAR", SouthAfrica()},
                                      {"SA", SouthAfrica()},
                                      {"SS", Sweden()},
                                      {"SEK", Sweden()},
                                      {"ARS", Argentina()},
                                      {"BRL", Brazil()},
                                      {"CNY", China()},
                                      {"CZK", CzechRepublic()},
                                      {"DKK", Denmark()},
                                      {"DEN", Denmark()},
                                      {"FIN", Finland()},
                                      {"HKD", HongKong()},
                                      {"ISK", Iceland()},
                                      {"INR", India()},
                                      {"IDR", Indonesia()},
                                      {"MXN", Mexico()},
                                      {"NZD", NewZealand()},
                                      {"NOK", Norway()},
                                      {"PLN", Poland()},
                                      {"RUB", Russia()},
                                      {"SAR", SaudiArabia()},
                                      {"SGD", Singapore()},
                                      {"KRW", SouthKorea()},
                                      {"TWD", Taiwan()},
                                      {"TRY", Turkey()},
                                      {"UAH", Ukraine()},
                                      {"HUF", Hungary()},
                                      // fallback to TARGET for these emerging ccys
                                      {"CLP", TARGET()},
                                      {"RON", TARGET()},
                                      {"THB", TARGET()},
                                      {"COP", TARGET()},
                                      {"ILS", TARGET()},
                                      {"KWD", TARGET()},
                                      {"PEN", TARGET()},
                                      {"TND", TARGET()},
                                      {"MYR", TARGET()},
                                      {"KZT", TARGET()},
                                      {"QAR", TARGET()},
                                      {"MXV", TARGET()},
                                      {"CLF", TARGET()},
                                      {"EGP", TARGET()},
                                      {"BHD", TARGET()},
                                      {"OMR", TARGET()},
                                      {"VND", TARGET()},
                                      {"AED", TARGET()},
                                      {"PHP", TARGET()},
                                      {"NGN", TARGET()},
                                      {"MAD", TARGET()},
                                      {"WeekendsOnly", WeekendsOnly()}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        // Try to split them up
        vector<string> calendarNames;
        split(calendarNames, s, boost::is_any_of(","));
        QL_REQUIRE(calendarNames.size() > 1 && calendarNames.size() <= 4, "Cannot convert " << s << " to Calendar");

        // Populate a vector of calendars.
        vector<Calendar> calendars;
        for (Size i = 0; i < calendarNames.size(); i++) {
            boost::trim(calendarNames[i]);
            try {
                calendars.push_back(parseCalendar(calendarNames[i]));
            } catch (std::exception& e) {
                QL_FAIL("Cannot convert " << s << " to Calendar [exception:" << e.what() << "]");
            } catch (...) {
                QL_FAIL("Cannot convert " << s << " to Calendar [unhandled exception]");
            }
        }

        switch (calendarNames.size()) {
        case 2:
            return JointCalendar(calendars[0], calendars[1]);
        case 3:
            return JointCalendar(calendars[0], calendars[1], calendars[2]);
        case 4:
            return JointCalendar(calendars[0], calendars[1], calendars[2], calendars[3]);
        default:
            QL_FAIL("Cannot convert " << s << " to Calendar");
        }
    }
}

Period parsePeriod(const string& s) { return PeriodParser::parse(s); }

BusinessDayConvention parseBusinessDayConvention(const string& s) {
    static map<string, BusinessDayConvention> m = {{"F", Following},
                                                   {"Following", Following},
                                                   {"FOLLOWING", Following},
                                                   {"MF", ModifiedFollowing},
                                                   {"ModifiedFollowing", ModifiedFollowing},
                                                   {"Modified Following", ModifiedFollowing},
                                                   {"MODIFIEDF", ModifiedFollowing},
                                                   {"P", Preceding},
                                                   {"Preceding", Preceding},
                                                   {"PRECEDING", Preceding},
                                                   {"MP", ModifiedPreceding},
                                                   {"ModifiedPreceding", ModifiedPreceding},
                                                   {"Modified Preceding", ModifiedPreceding},
                                                   {"MODIFIEDP", ModifiedPreceding},
                                                   {"U", Unadjusted},
                                                   {"Unadjusted", Unadjusted},
                                                   {"INDIFF", Unadjusted}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert " << s << " to BusinessDayConvention");
    }
}

DayCounter parseDayCounter(const string& s) {
    static map<string, DayCounter> m = {{"A360", Actual360()},
                                        {"Actual/360", Actual360()},
                                        {"ACT/360", Actual360()},
                                        {"A365", Actual365Fixed()},
                                        {"A365F", Actual365Fixed()},
                                        {"Actual/365 (Fixed)", Actual365Fixed()},
                                        {"ACT/365", Actual365Fixed()},
                                        {"T360", Thirty360(Thirty360::USA)},
                                        {"30/360", Thirty360(Thirty360::USA)},
                                        {"30/360 (Bond Basis)", Thirty360(Thirty360::USA)},
                                        {"ACT/nACT", Thirty360(Thirty360::USA)},
                                        {"30E/360 (Eurobond Basis)", Thirty360(Thirty360::European)},
                                        {"30E/360", Thirty360(Thirty360::European)},
                                        {"30/360 (Italian)", Thirty360(Thirty360::Italian)},
                                        {"ActActISDA", ActualActual(ActualActual::ISDA)},
                                        {"Actual/Actual (ISDA)", ActualActual(ActualActual::ISDA)},
                                        {"ACT/ACT", ActualActual(ActualActual::ISDA)},
                                        {"ACT29", ActualActual(ActualActual::ISDA)},
                                        {"ACT", ActualActual(ActualActual::ISDA)},
                                        {"ActActISMA", ActualActual(ActualActual::ISMA)},
                                        {"Actual/Actual (ISMA)", ActualActual(ActualActual::ISMA)},
                                        {"ActActAFB", ActualActual(ActualActual::AFB)},
                                        {"Actual/Actual (AFB)", ActualActual(ActualActual::AFB)},
                                        {"1/1", OneDayCounter()},
                                        {"BUS/252", Business252()},
                                        {"Business/252", Business252()},
                                        {"Actual/365 (No Leap)", Actual365NoLeap()},
                                        {"Act/365 (NL)", Actual365NoLeap()},
                                        {"NL/365", Actual365NoLeap()},
                                        {"Actual/365 (JGB)", Actual365NoLeap()}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("DayCounter " << s << " not recognized");
    }
}

Currency parseCurrency(const string& s) {
    static map<string, Currency> m = {{"ATS", ATSCurrency()},
                                      {"AUD", AUDCurrency()},
                                      {"BEF", BEFCurrency()},
                                      {"BRL", BRLCurrency()},
                                      {"CAD", CADCurrency()},
                                      {"CHF", CHFCurrency()},
                                      {"CNY", CNYCurrency()},
                                      {"CZK", CZKCurrency()},
                                      {"DEM", DEMCurrency()},
                                      {"DKK", DKKCurrency()},
                                      {"EUR", EURCurrency()},
                                      {"ESP", ESPCurrency()},
                                      {"FIM", FIMCurrency()},
                                      {"FRF", FRFCurrency()},
                                      {"GBP", GBPCurrency()},
                                      {"GRD", GRDCurrency()},
                                      {"HKD", HKDCurrency()},
                                      {"HUF", HUFCurrency()},
                                      {"IEP", IEPCurrency()},
                                      {"ITL", ITLCurrency()},
                                      {"INR", INRCurrency()},
                                      {"ISK", ISKCurrency()},
                                      {"JPY", JPYCurrency()},
                                      {"KRW", KRWCurrency()},
                                      {"LUF", LUFCurrency()},
                                      {"NLG", NLGCurrency()},
                                      {"NOK", NOKCurrency()},
                                      {"NZD", NZDCurrency()},
                                      {"PLN", PLNCurrency()},
                                      {"PTE", PTECurrency()},
                                      {"RON", RONCurrency()},
                                      {"SEK", SEKCurrency()},
                                      {"SGD", SGDCurrency()},
                                      {"THB", THBCurrency()},
                                      {"TRY", TRYCurrency()},
                                      {"TWD", TWDCurrency()},
                                      {"USD", USDCurrency()},
                                      {"ZAR", ZARCurrency()},
                                      {"ARS", ARSCurrency()},
                                      {"CLP", CLPCurrency()},
                                      {"COP", COPCurrency()},
                                      {"IDR", IDRCurrency()},
                                      {"ILS", ILSCurrency()},
                                      {"KWD", KWDCurrency()},
                                      {"PEN", PENCurrency()},
                                      {"MXN", MXNCurrency()},
                                      {"SAR", SARCurrency()},
                                      {"RUB", RUBCurrency()},
                                      {"TND", TNDCurrency()},
                                      {"MYR", MYRCurrency()},
                                      {"UAH", UAHCurrency()},
                                      {"KZT", KZTCurrency()},
                                      {"QAR", QARCurrency()},
                                      {"MXV", MXVCurrency()},
                                      {"CLF", CLFCurrency()},
                                      {"EGP", EGPCurrency()},
                                      {"BHD", BHDCurrency()},
                                      {"OMR", OMRCurrency()},
                                      {"VND", VNDCurrency()},
                                      {"AED", AEDCurrency()},
                                      {"PHP", PHPCurrency()},
                                      {"NGN", NGNCurrency()},
                                      {"MAD", MADCurrency()}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Currency " << s << " not recognized");
    }
}

DateGeneration::Rule parseDateGenerationRule(const string& s) {
    static map<string, DateGeneration::Rule> m = {{"Backward", DateGeneration::Backward},
                                                  {"Forward", DateGeneration::Forward},
                                                  {"Zero", DateGeneration::Zero},
                                                  {"ThirdWednesday", DateGeneration::ThirdWednesday},
                                                  {"Twentieth", DateGeneration::Twentieth},
                                                  {"TwentiethIMM", DateGeneration::TwentiethIMM},
                                                  {"OldCDS", DateGeneration::OldCDS},
                                                  {"CDS", DateGeneration::CDS}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Date Generation Rule " << s << " not recognized");
    }
}

Frequency parseFrequency(const string& s) {
    static map<string, Frequency> m = {{"Z", Once},
                                       {"Once", Once},
                                       {"A", Annual},
                                       {"Annual", Annual},
                                       {"S", Semiannual},
                                       {"Semiannual", Semiannual},
                                       {"Q", Quarterly},
                                       {"Quarterly", Quarterly},
                                       {"B", Bimonthly},
                                       {"Bimonthly", Bimonthly},
                                       {"M", Monthly},
                                       {"Monthly", Monthly},
                                       {"L", EveryFourthWeek},
                                       {"Lunarmonth", EveryFourthWeek},
                                       {"W", Weekly},
                                       {"Weekly", Weekly},
                                       {"D", Daily},
                                       {"Daily", Daily}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Frequency \"" << s << "\" not recognized");
    }
}

Compounding parseCompounding(const string& s) {
    static map<string, Compounding> m = {
        {"Simple", Simple},
        {"Compounded", Compounded},
        {"Continuous", Continuous},
        {"SimpleThenCompounded", SimpleThenCompounded},
    };

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Compounding \"" << s << "\" not recognized");
    }
}

Position::Type parsePositionType(const std::string& s) {
    static map<string, Position::Type> m = {
        {"Long", Position::Long}, {"Short", Position::Short}, {"L", Position::Long}, {"S", Position::Short},
    };

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Position type \"" << s << "\" not recognized");
    }
}

Settlement::Type parseSettlementType(const std::string& s) {
    static map<string, Settlement::Type> m = {
        {"Cash", Settlement::Cash},
        {"Physical", Settlement::Physical},
        {"C", Settlement::Cash},
        {"P", Settlement::Physical},
    };

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Settlement type \"" << s << "\" not recognized");
    }
}

Exercise::Type parseExerciseType(const std::string& s) {
    static map<string, Exercise::Type> m = {
        {"European", Exercise::European}, {"Bermudan", Exercise::Bermudan}, {"American", Exercise::American},
    };

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Exercise type \"" << s << "\" not recognized");
    }
}

Option::Type parseOptionType(const std::string& s) {
    static map<string, Option::Type> m = {{"Put", Option::Put}, {"Call", Option::Call}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Option type \"" << s << "\" not recognized");
    }
}

void parseDateOrPeriod(const string& s, Date& d, Period& p, bool& isDate) {
    isDate = false;
    try {
        p = ore::data::parsePeriod(s);
    } catch (...) {
        try {
            d = ore::data::parseDate(s);
            isDate = true;
        } catch (...) {
            QL_FAIL("could not parse " << s << " as date or period");
        }
    }
}

QuantLib::LsmBasisSystem::PolynomType parsePolynomType(const std::string& s) {
    static map<string, LsmBasisSystem::PolynomType> poly = {
        {"Monomial", LsmBasisSystem::PolynomType::Monomial},
        {"Laguerre", LsmBasisSystem::PolynomType::Laguerre},
        {"Hermite", LsmBasisSystem::PolynomType::Hermite},
        {"Hyperbolic", LsmBasisSystem::PolynomType::Hyperbolic},
        {"Legendre", LsmBasisSystem::PolynomType::Legendre},
        {"Chebyshev", LsmBasisSystem::PolynomType::Chebyshev},
        {"Chebyshev2nd", LsmBasisSystem::PolynomType::Chebyshev2nd},
    };

    auto it = poly.find(s);
    if (it != poly.end()) {
        return it->second;
    } else {
        QL_FAIL("Polynom type \"" << s << "\" not recognized");
    }
}

std::vector<string> parseListOfValues(string s) {
    boost::trim(s);
    std::vector<string> vec;
    boost::char_separator<char> sep(",");
    boost::tokenizer<boost::char_separator<char>> tokens(s, sep);
    for (auto r : tokens) {
        boost::trim(r);
        vec.push_back(r);
    }
    return vec;
}

} // namespace data
} // namespace ore
