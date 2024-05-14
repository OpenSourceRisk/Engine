/*
 Copyright (C) 2016-2021 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

#include <boost/algorithm/string.hpp>
#include <map>
#include <ored/utilities/calendarparser.hpp>
#include <ored/utilities/currencyparser.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>
#include <ql/indexes/all.hpp>
#include <ql/time/daycounters/all.hpp>
#include <ql/utilities/dataparsers.hpp>
#include <qle/instruments/cashflowresults.hpp>
#include <qle/time/yearcounter.hpp>

#include <boost/lexical_cast.hpp>
#include <regex>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using boost::iequals;
using boost::algorithm::to_lower_copy;

namespace ore {
namespace data {

Date parseDate(const string& s) {
    // TODO: review

    if (s == "")
        return Date();

    // guess formats from token number and sizes
    // check permissible lengths
    QL_REQUIRE((s.size() >= 3 && s.size() <= 6) || s.size() == 8 || s.size() == 10,
               "invalid date format of \"" << s << "\", date string length 8 or 10 or between 3 and 6 required");

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
    try {
        return std::stod(s);
    } catch (const std::exception& ex) {
        QL_FAIL("Failed to parseReal(\"" << s << "\") " << ex.what());
    }
}

Real parseRealOrNull(const string& s) {
    if(s.empty())
        return Null<Real>();
    return parseReal(s);
}

bool tryParseReal(const string& s, QuantLib::Real& result) {
    try {
        result = std::stod(s);
    } catch (...) {
        result = Null<Real>();
        return false;
    }
    return true;
}

Integer parseInteger(const string& s) {
    try {
        return boost::lexical_cast<Integer>(s.c_str());
    } catch (std::exception& ex) {
        QL_FAIL("Failed to parseInteger(\"" << s << "\") " << ex.what());
    }
}

bool parseBool(const string& s) {
    static map<string, bool> b = {{"Y", true},      {"YES", true},    {"TRUE", true},   {"True", true},
                                  {"true", true},   {"1", true},      {"N", false},     {"NO", false},
                                  {"FALSE", false}, {"False", false}, {"false", false}, {"0", false}};

    auto it = b.find(s);
    if (it != b.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert \"" << s << "\" to bool");
    }
}

Calendar parseCalendar(const string& s) { return CalendarParser::instance().parseCalendar(s); }

bool isOnePeriod(const string& s) {
    if (s.empty())
        return false;
    char c = std::toupper(s.back());
    if (!(c == 'D' || c == 'W' || c == 'M' || c == 'Y'))
        return false;
    for (auto c = s.cbegin(); c != std::next(s.end(), -1); std::advance(c, 1))
        if (*c < '0' || *c > '9')
            return false;
    return true;
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
                                                   {"MODFOLLOWING", ModifiedFollowing},
                                                   {"P", Preceding},
                                                   {"Preceding", Preceding},
                                                   {"PRECEDING", Preceding},
                                                   {"MP", ModifiedPreceding},
                                                   {"ModifiedPreceding", ModifiedPreceding},
                                                   {"Modified Preceding", ModifiedPreceding},
                                                   {"MODIFIEDP", ModifiedPreceding},
                                                   {"U", Unadjusted},
                                                   {"Unadjusted", Unadjusted},
                                                   {"INDIFF", Unadjusted},
                                                   {"HalfMonthModifiedFollowing", HalfMonthModifiedFollowing},
                                                   {"HMMF", HalfMonthModifiedFollowing},
                                                   {"Half Month Modified Following", HalfMonthModifiedFollowing},
                                                   {"HALFMONTHMF", HalfMonthModifiedFollowing},
                                                   {"HalfMonthMF", HalfMonthModifiedFollowing},
                                                   {"NEAREST", Nearest},
                                                   {"NONE", Unadjusted},
                                                   {"NotApplicable", Unadjusted}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert \"" << s << "\" to BusinessDayConvention");
    }
}

DayCounter parseDayCounter(const string& s) {
    static map<string, DayCounter> m = {{"A360", Actual360()},
                                        {"Actual/360", Actual360()},
                                        {"ACT/360", Actual360()},
                                        {"Act/360", Actual360()},
                                        {"A360 (Incl Last)", Actual360(true)},
                                        {"Actual/360 (Incl Last)", Actual360(true)},
                                        {"ACT/360 (Incl Last)", Actual360(true)},
                                        {"Act/360 (Incl Last)", Actual360(true)},
                                        {"A365", Actual365Fixed()},
                                        {"A365F", Actual365Fixed()},
                                        {"Actual/365 (Fixed)", Actual365Fixed()},
                                        {"Actual/365 (fixed)", Actual365Fixed()},
                                        {"ACT/365.FIXED", Actual365Fixed()},
                                        {"ACT/365", Actual365Fixed()},
                                        {"ACT/365L", Actual365Fixed()},
                                        {"Act/365", Actual365Fixed()},
                                        {"Act/365L", Actual365Fixed()},
                                        {"Act/365 (Canadian Bond)", Actual365Fixed(Actual365Fixed::Canadian)},
                                        {"T360", Thirty360(Thirty360::USA)},
                                        {"30/360", Thirty360(Thirty360::USA)},
                                        {"30/360 US", Thirty360(Thirty360::USA)},
                                        {"30/360 (US)", Thirty360(Thirty360::USA)},
                                        {"30U/360", Thirty360(Thirty360::USA)},
                                        {"30US/360", Thirty360(Thirty360::USA)},
                                        {"30/360 (Bond Basis)", Thirty360(Thirty360::BondBasis)},
                                        {"ACT/nACT", Thirty360(Thirty360::USA)},
                                        {"30E/360 (Eurobond Basis)", Thirty360(Thirty360::European)},
                                        {"30/360 AIBD (Euro)", Thirty360(Thirty360::European)},
                                        {"30E/360.ICMA", Thirty360(Thirty360::European)},
                                        {"30E/360 ICMA", Thirty360(Thirty360::European)},
                                        {"30E/360", Thirty360(Thirty360::European)},
                                        {"30E/360E", Thirty360(Thirty360::German)},
                                        {"30E/360.ISDA", Thirty360(Thirty360::German)},
                                        {"30E/360 ISDA", Thirty360(Thirty360::German)},
                                        {"30/360 German", Thirty360(Thirty360::German)},
                                        {"30/360 (German)", Thirty360(Thirty360::German)},
                                        {"30/360 Italian", Thirty360(Thirty360::Italian)},
                                        {"30/360 (Italian)", Thirty360(Thirty360::Italian)},
                                        {"ActActISDA", ActualActual(ActualActual::ISDA)},
                                        {"ACT/ACT.ISDA", ActualActual(ActualActual::ISDA)},
                                        {"Actual/Actual (ISDA)", ActualActual(ActualActual::ISDA)},
                                        {"ActualActual (ISDA)", ActualActual(ActualActual::ISDA)},
                                        {"ACT/ACT", ActualActual(ActualActual::ISDA)},
                                        {"Act/Act", ActualActual(ActualActual::ISDA)},
                                        {"ACT29", ActualActual(ActualActual::AFB)},
                                        {"ACT", ActualActual(ActualActual::ISDA)},
                                        {"ActActISMA", ActualActual(ActualActual::ISMA)},
                                        {"Actual/Actual (ISMA)", ActualActual(ActualActual::ISMA)},
                                        {"ActualActual (ISMA)", ActualActual(ActualActual::ISMA)},
                                        {"ACT/ACT.ISMA", ActualActual(ActualActual::ISMA)},
                                        {"ActActICMA", ActualActual(ActualActual::ISMA)},
                                        {"Actual/Actual (ICMA)", ActualActual(ActualActual::ISMA)},
                                        {"ActualActual (ICMA)", ActualActual(ActualActual::ISMA)},
                                        {"ACT/ACT.ICMA", ActualActual(ActualActual::ISMA)},
                                        {"ActActAFB", ActualActual(ActualActual::AFB)},
                                        {"ACT/ACT.AFB", ActualActual(ActualActual::AFB)},
                                        {"Actual/Actual (AFB)", ActualActual(ActualActual::AFB)},
                                        {"1/1", OneDayCounter()},
                                        {"BUS/252", Business252()},
                                        {"Business/252", Business252()},
                                        {"Actual/365 (No Leap)", Actual365Fixed(Actual365Fixed::NoLeap)},
                                        {"Act/365 (NL)", Actual365Fixed(Actual365Fixed::NoLeap)},
                                        {"NL/365", Actual365Fixed(Actual365Fixed::NoLeap)},
                                        {"Actual/365 (JGB)", Actual365Fixed(Actual365Fixed::NoLeap)},
                                        {"Simple", SimpleDayCounter()},
                                        {"Year", YearCounter()},
                                        {"A364", QuantLib::Actual364()},
                                        {"Actual/364", Actual364()},
                                        {"Act/364", Actual364()},
                                        {"ACT/364", Actual364()}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;

    } else {
        QL_FAIL("DayCounter \"" << s << "\" not recognized");
    }
}

Currency parseCurrency(const string& s) { return CurrencyParser::instance().parseCurrency(s); }

QuantExt::ConfigurableCurrency::Type parseCurrencyType(const string& s) {
    static map<string, QuantExt::ConfigurableCurrency::Type> m = {
        {"Major", QuantExt::ConfigurableCurrency::Type::Major}, {"Fiat Currency", QuantExt::ConfigurableCurrency::Type::Major},
        {"Fiat", QuantExt::ConfigurableCurrency::Type::Major}, {"Metal", QuantExt::ConfigurableCurrency::Type::Metal}, 
        {"Precious Metal", QuantExt::ConfigurableCurrency::Type::Metal}, {"Crypto", QuantExt::ConfigurableCurrency::Type::Crypto},
        {"Cryptocurrency", QuantExt::ConfigurableCurrency::Type::Crypto}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Currency type \"" << s << "\" not recognised");
    }
}


Currency parseMinorCurrency(const string& s) { return CurrencyParser::instance().parseMinorCurrency(s); }

Currency parseCurrencyWithMinors(const string& s) { return CurrencyParser::instance().parseCurrencyWithMinors(s); }

pair<Currency, Currency> parseCurrencyPair(const string& s, const string& delimiters) {
    return CurrencyParser::instance().parseCurrencyPair(s, delimiters);
}

bool checkCurrency(const string& code) { return CurrencyParser::instance().isValidCurrency(code); }

bool isPseudoCurrency(const string& code) { return CurrencyParser::instance().isPseudoCurrency(code); }

bool isPreciousMetal(const string& code) { return CurrencyParser::instance().isPreciousMetal(code); }

bool isCryptoCurrency(const string& code) { return CurrencyParser::instance().isCryptoCurrency(code); }

QuantLib::Real convertMinorToMajorCurrency(const std::string& s, QuantLib::Real value) {
    return CurrencyParser::instance().convertMinorToMajorCurrency(s, value);
}

DateGeneration::Rule parseDateGenerationRule(const string& s) {
    static map<string, DateGeneration::Rule> m = {{"Backward", DateGeneration::Backward},
                                                  {"Forward", DateGeneration::Forward},
                                                  {"Zero", DateGeneration::Zero},
                                                  {"ThirdWednesday", DateGeneration::ThirdWednesday},
                                                  {"Twentieth", DateGeneration::Twentieth},
                                                  {"TwentiethIMM", DateGeneration::TwentiethIMM},
                                                  {"OldCDS", DateGeneration::OldCDS},
                                                  {"CDS2015", DateGeneration::CDS2015},
                                                  {"CDS", DateGeneration::CDS},
                                                  {"LastWednesday", DateGeneration::LastWednesday}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Date Generation Rule \"" << s << "\" not recognized");
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

QuantLib::Bond::Price::Type parseBondPriceType(const string& s) {
    static map<string, QuantLib::Bond::Price::Type> m = {{"Clean", QuantLib::Bond::Price::Type::Clean},
                                                         {"Dirty", QuantLib::Bond::Price::Type::Dirty}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("BondPriceType \"" << s << "\" not recognized");
    }
}

Position::Type parsePositionType(const std::string& s) {
    static map<string, Position::Type> m = {
        {"Long", Position::Long},
        {"Short", Position::Short},
        {"L", Position::Long},
        {"S", Position::Short},
    };

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Position type \"" << s << "\" not recognized");
    }
}

Protection::Side parseProtectionSide(const std::string& s) {
    static map<string, Protection::Side> m = {{"Buyer", Protection::Buyer},
                                              {"Seller", Protection::Seller},
                                              {"B", Protection::Buyer},
                                              {"S", Protection::Seller}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Protection side \"" << s << "\" not recognized");
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

Settlement::Method parseSettlementMethod(const std::string& s) {
    static map<string, Settlement::Method> m = {
        {"PhysicalOTC", Settlement::PhysicalOTC},
        {"PhysicalCleared", Settlement::PhysicalCleared},
        {"CollateralizedCashPrice", Settlement::CollateralizedCashPrice},
        {"ParYieldCurve", Settlement::ParYieldCurve},
    };

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Settlement method \"" << s << "\" not recognized");
    }
}

Exercise::Type parseExerciseType(const std::string& s) {
    static map<string, Exercise::Type> m = {
        {"European", Exercise::European},
        {"Bermudan", Exercise::Bermudan},
        {"American", Exercise::American},
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

    QL_REQUIRE(!s.empty(), "Option type not given.");
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Option type \"" << s << "\" not recognized");
    }
}

boost::variant<QuantLib::Date, QuantLib::Period> parseDateOrPeriod(const string& s) {
    QL_REQUIRE(!s.empty(), "Cannot parse empty string as date or period");
    string c(1, s.back());
    bool isPeriod = c.find_first_of("DdWwMmYy") != string::npos;
    if (isPeriod) {
        return ore::data::parsePeriod(s);
    } else {
        return ore::data::parseDate(s);
    }
}

namespace {
struct legacy_date_or_period_visitor : public boost::static_visitor<> {
    legacy_date_or_period_visitor(Date& res_d, Period& res_p, bool& res_is_date)
        : res_d(res_d), res_p(res_p), res_is_date(res_is_date) {}
    void operator()(const QuantLib::Date& d) const {
        res_d = d;
        res_is_date = true;
    }
    void operator()(const QuantLib::Period& p) const {
        res_p = p;
        res_is_date = false;
    }
    Date& res_d;
    Period& res_p;
    bool& res_is_date;
};
} // namespace

void parseDateOrPeriod(const string& s, Date& d, Period& p, bool& isDate) {
    auto r = parseDateOrPeriod(s);
    boost::apply_visitor(legacy_date_or_period_visitor(d, p, isDate), r);
}

QuantLib::LsmBasisSystem::PolynomialType parsePolynomType(const std::string& s) {
    static map<string, LsmBasisSystem::PolynomialType> poly = {
        {"Monomial", LsmBasisSystem::PolynomialType::Monomial},
        {"Laguerre", LsmBasisSystem::PolynomialType::Laguerre},
        {"Hermite", LsmBasisSystem::PolynomialType::Hermite},
        {"Hyperbolic", LsmBasisSystem::PolynomialType::Hyperbolic},
        {"Legendre", LsmBasisSystem::PolynomialType::Legendre},
        {"Chebyshev", LsmBasisSystem::PolynomialType::Chebyshev},
        {"Chebyshev2nd", LsmBasisSystem::PolynomialType::Chebyshev2nd},
    };

    auto it = poly.find(s);
    if (it != poly.end()) {
        return it->second;
    } else {
        QL_FAIL("Polynom type \"" << s << "\" not recognized");
    }
}

std::ostream& operator<<(std::ostream& os, QuantLib::LsmBasisSystem::PolynomialType a) {
    switch (a) {
    case LsmBasisSystem::PolynomialType::Monomial:
        return os << "Monomial";
    case LsmBasisSystem::PolynomialType::Laguerre:
        return os << "Laguerre";
    case LsmBasisSystem::PolynomialType::Hermite:
        return os << "Hermite";
    case LsmBasisSystem::PolynomialType::Hyperbolic:
        return os << "Hyperbolic";
    case LsmBasisSystem::PolynomialType::Legendre:
        return os << "Legendre";
    case LsmBasisSystem::PolynomialType::Chebyshev:
        return os << "Chebychev";
    case LsmBasisSystem::PolynomialType::Chebyshev2nd:
        return os << "Chebychev2nd";
    default:
        QL_FAIL("unknown LsmBasisSystem::PolynomialType '" << static_cast<int>(a) << "'");
    }
}

SobolBrownianGenerator::Ordering parseSobolBrownianGeneratorOrdering(const std::string& s) {
    static map<string, SobolBrownianGenerator::Ordering> m = {{"Factors", SobolBrownianGenerator::Ordering::Factors},
                                                              {"Steps", SobolBrownianGenerator::Ordering::Steps},
                                                              {"Diagonal", SobolBrownianGenerator::Ordering::Diagonal}};
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("SobolBrownianGenerator ordering \"" << s << "\" not recognized");
    }
}

SobolRsg::DirectionIntegers parseSobolRsgDirectionIntegers(const std::string& s) {
    static map<string, SobolRsg::DirectionIntegers> m = {
        {"Unit", SobolRsg::DirectionIntegers::Unit},
        {"Jaeckel", SobolRsg::DirectionIntegers::Jaeckel},
        {"SobolLevitan", SobolRsg::DirectionIntegers::SobolLevitan},
        {"SobolLevitanLemieux", SobolRsg::DirectionIntegers::SobolLevitanLemieux},
        {"JoeKuoD5", SobolRsg::DirectionIntegers::JoeKuoD5},
        {"JoeKuoD6", SobolRsg::DirectionIntegers::JoeKuoD6},
        {"JoeKuoD7", SobolRsg::DirectionIntegers::JoeKuoD7},
        {"Kuo", SobolRsg::DirectionIntegers::Kuo},
        {"Kuo2", SobolRsg::DirectionIntegers::Kuo2},
        {"Kuo3", SobolRsg::DirectionIntegers::Kuo3}};
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("SobolRsg direction integers \"" << s << "\" not recognized");
    }
}

Weekday parseWeekday(const string& s) {

    static map<string, Weekday> m = {{"Sun", Weekday::Sunday},    {"Mon", Weekday::Monday},   {"Tue", Weekday::Tuesday},
                                     {"Wed", Weekday::Wednesday}, {"Thu", Weekday::Thursday}, {"Fri", Weekday::Friday},
                                     {"Sat", Weekday::Saturday}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("The string \"" << s << "\" is not recognized as a Weekday");
    }
}

Month parseMonth(const string& s) {

    static map<string, Month> m = {{"Jan", Month::January}, {"Feb", Month::February}, {"Mar", Month::March},
                                   {"Apr", Month::April},   {"May", Month::May},      {"Jun", Month::June},
                                   {"Jul", Month::July},    {"Aug", Month::August},   {"Sep", Month::September},
                                   {"Oct", Month::October}, {"Nov", Month::November}, {"Dec", Month::December}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("The string \"" << s << "\" is not recognized as a Month");
    }
}

PaymentLag parsePaymentLag(const string& s) {
    Period p;
    Natural n;
    if (tryParse<Period>(s, p, parsePeriod))
        return p;
    else if (tryParse<Natural>(s, n, parseInteger))
        return n;
    else
        return 0;
}

std::vector<string> parseListOfValues(string s, const char escape, const char delim, const char quote) {
    boost::trim(s);
    std::vector<string> vec;
    boost::escaped_list_separator<char> sep(escape, delim, quote);
    boost::tokenizer<boost::escaped_list_separator<char>> tokens(s, sep);
    for (auto r : tokens) {
        boost::trim(r);
        vec.push_back(r);
    }
    return vec;
}

AmortizationType parseAmortizationType(const std::string& s) {
    static map<string, AmortizationType> type = {
        {"None", AmortizationType::None},
        {"FixedAmount", AmortizationType::FixedAmount},
        {"RelativeToInitialNotional", AmortizationType::RelativeToInitialNotional},
        {"RelativeToPreviousNotional", AmortizationType::RelativeToPreviousNotional},
        {"Annuity", AmortizationType::Annuity},
        {"LinearToMaturity", AmortizationType::LinearToMaturity}};

    auto it = type.find(s);
    if (it != type.end()) {
        return it->second;
    } else {
        QL_FAIL("Amortitazion type \"" << s << "\" not recognized");
    }
}

SequenceType parseSequenceType(const std::string& s) {
    static map<string, SequenceType> seq = {
        {"MersenneTwister", SequenceType::MersenneTwister},
        {"MersenneTwisterAntithetic", SequenceType::MersenneTwisterAntithetic},
        {"Sobol", SequenceType::Sobol},
        {"SobolBrownianBridge", SequenceType::SobolBrownianBridge},
        {"Burley2020SobolBrownianBridge", SequenceType::Burley2020SobolBrownianBridge}};
    auto it = seq.find(s);
    if (it != seq.end())
        return it->second;
    else
        QL_FAIL("sequence type \"" << s << "\" not recognised");
}

QuantLib::CPI::InterpolationType parseObservationInterpolation(const std::string& s) {
    static map<string, CPI::InterpolationType> seq = {
        {"Flat", CPI::Flat}, {"Linear", CPI::Linear}, {"AsIndex", CPI::AsIndex}};
    auto it = seq.find(s);
    if (it != seq.end())
        return it->second;
    else
        QL_FAIL("observation interpolation type \"" << s << "\" not recognised");
}

FdmSchemeDesc parseFdmSchemeDesc(const std::string& s) {
    static std::map<std::string, FdmSchemeDesc> m = {{"CrankNicolson", FdmSchemeDesc::CrankNicolson()},
                                                     {"Hundsdorfer", FdmSchemeDesc::Hundsdorfer()},
                                                     {"Douglas", FdmSchemeDesc::Douglas()},
                                                     {"CraigSneyd", FdmSchemeDesc::CraigSneyd()},
                                                     {"ModifiedCraigSneyd", FdmSchemeDesc::ModifiedCraigSneyd()},
                                                     {"ImplicitEuler", FdmSchemeDesc::ImplicitEuler()},
                                                     {"ExplicitEuler", FdmSchemeDesc::ExplicitEuler()},
                                                     {"MethodOfLines", FdmSchemeDesc::MethodOfLines()},
                                                     {"TrBDF2", FdmSchemeDesc::TrBDF2()}};

    auto it = m.find(s);
    if (it != m.end())
        return it->second;
    else
        QL_FAIL("fdm scheme \"" << s << "\" not recognised");
}

AssetClass parseAssetClass(const std::string& s) {
    static map<string, AssetClass> assetClasses = {{"EQ", AssetClass::EQ},     {"FX", AssetClass::FX},
                                                   {"COM", AssetClass::COM},   {"IR", AssetClass::IR},
                                                   {"INF", AssetClass::INF},   {"CR", AssetClass::CR},
                                                   {"BOND", AssetClass::BOND}, {"BOND_INDEX", AssetClass::BOND_INDEX}};
    auto it = assetClasses.find(s);
    if (it != assetClasses.end()) {
        return it->second;
    } else {
        QL_FAIL("AssetClass \"" << s << "\" not recognized");
    }
}

std::ostream& operator<<(std::ostream& os, AssetClass a) {
    switch (a) {
    case AssetClass::EQ:
        return os << "EQ";
    case AssetClass::FX:
        return os << "FX";
    case AssetClass::COM:
        return os << "COM";
    case AssetClass::IR:
        return os << "IR";
    case AssetClass::INF:
        return os << "INF";
    case AssetClass::CR:
        return os << "CR";
    case AssetClass::BOND:
        return os << "BOND";
    case AssetClass::BOND_INDEX:
        return os << "BOND_INDEX";
    default:
        QL_FAIL("Unknown AssetClass");
    }
}

DeltaVolQuote::AtmType parseAtmType(const std::string& s) {
    static map<string, DeltaVolQuote::AtmType> m = {{"AtmNull", DeltaVolQuote::AtmNull},
                                                    {"AtmSpot", DeltaVolQuote::AtmSpot},
                                                    {"AtmFwd", DeltaVolQuote::AtmFwd},
                                                    {"AtmDeltaNeutral", DeltaVolQuote::AtmDeltaNeutral},
                                                    {"AtmVegaMax", DeltaVolQuote::AtmVegaMax},
                                                    {"AtmGammaMax", DeltaVolQuote::AtmGammaMax},
                                                    {"AtmPutCall50", DeltaVolQuote::AtmPutCall50}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("ATM type \"" << s << "\" not recognized");
    }
}

DeltaVolQuote::DeltaType parseDeltaType(const std::string& s) {
    static map<string, DeltaVolQuote::DeltaType> m = {{"Spot", DeltaVolQuote::Spot},
                                                      {"Fwd", DeltaVolQuote::Fwd},
                                                      {"PaSpot", DeltaVolQuote::PaSpot},
                                                      {"PaFwd", DeltaVolQuote::PaFwd}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Delta type \"" << s << "\" not recognized");
    }
}

//! Parse Extrapolation from string
Extrapolation parseExtrapolation(const string& s) {
    if (s == "None") {
        return Extrapolation::None;
    } else if (s == "UseInterpolator" || s == "Linear") {
        return Extrapolation::UseInterpolator;
    } else if (s == "Flat") {
        return Extrapolation::Flat;
    } else {
        QL_FAIL("Extrapolation '" << s << "' not recognized");
    }
}

//! Write Extrapolation, \p extrap, to stream.
std::ostream& operator<<(std::ostream& os, Extrapolation extrap) {
    switch (extrap) {
    case Extrapolation::None:
        return os << "None";
    case Extrapolation::UseInterpolator:
        return os << "UseInterpolator";
    case Extrapolation::Flat:
        return os << "Flat";
    default:
        QL_FAIL("Unknown Extrapolation");
    }
}

VolatilityType parseVolatilityQuoteType(const string& s) {

    if (s == "Normal") {
        return Normal;
    } else if (s == "ShiftedLognormal") {
        return ShiftedLognormal;
    } else {
        QL_FAIL("Unknown volatility quote type " << s);
    }
}

CapFloor::Type parseCapFloorType(const string& s) {
    if (s == "Cap") {
        return CapFloor::Cap;
    } else if (s == "Floor") {
        return CapFloor::Floor;
    } else if (s == "Collar") {
        return CapFloor::Collar;
    } else {
        QL_FAIL("Unknown cap floor type " << s);
    }
}

YoYInflationCapFloor::Type parseYoYInflationCapFloorType(const string& s) {
    if (s == "Cap" || s == "YoYInflationCap") {
        return YoYInflationCapFloor::Cap;
    } else if (s == "Floor" || s == "YoYInflationFloor") {
        return YoYInflationCapFloor::Floor;
    } else if (s == "Collar" || s == "YoYInflationCollar") {
        return YoYInflationCapFloor::Collar;
    } else {
        QL_FAIL("Unknown year on year inflation cap floor type " << s);
    }
}

QuantExt::CrossAssetModel::AssetType parseCamAssetType(const string& s) {
    if (s == "IR") {
        return QuantExt::CrossAssetModel::AssetType::IR;
    } else if (s == "FX") {
        return QuantExt::CrossAssetModel::AssetType::FX;
    } else if (s == "INF") {
        return QuantExt::CrossAssetModel::AssetType::INF;
    } else if (s == "CR") {
        return QuantExt::CrossAssetModel::AssetType::CR;
    } else if (s == "EQ") {
        return QuantExt::CrossAssetModel::AssetType::EQ;
    } else if (s == "COM") {
        return QuantExt::CrossAssetModel::AssetType::COM;
    } else if (s == "CrState") {
        return QuantExt::CrossAssetModel::AssetType::CrState;
    } else {
        QL_FAIL("Unknown cross asset model type " << s);
    }
}

pair<string, string> parseBoostAny(const boost::any& anyType, Size precision) {
    string resultType;
    std::ostringstream oss;

    if (anyType.type() == typeid(int)) {
        resultType = "int";
        int r = boost::any_cast<int>(anyType);
        oss << std::fixed << std::setprecision(precision) << r;
    } else if (anyType.type() == typeid(Size)) {
        resultType = "size";
        int r = boost::any_cast<Size>(anyType);
        oss << std::fixed << std::setprecision(precision) << r;
    } else if (anyType.type() == typeid(double)) {
        resultType = "double";
        double r = boost::any_cast<double>(anyType);
        if (r != Null<Real>())
            oss << std::fixed << std::setprecision(precision) << r;
    } else if (anyType.type() == typeid(std::string)) {
        resultType = "string";
        std::string r = boost::any_cast<std::string>(anyType);
        oss << std::fixed << std::setprecision(precision) << r;
    } else if (anyType.type() == typeid(Date)) {
        resultType = "date";
        oss << io::iso_date(boost::any_cast<Date>(anyType));
    } else if (anyType.type() == typeid(bool)) {
        resultType = "bool";
        oss << std::boolalpha << boost::any_cast<bool>(anyType);
    } else if (anyType.type() == typeid(std::vector<bool>)) {
        resultType = "vector_bool";
        std::vector<bool> r = boost::any_cast<std::vector<bool>>(anyType);
        if (r.size() == 0) {
            oss << "";
        } else {
            oss << std::boolalpha << "\"" << boost::any_cast<bool>(anyType);
            for (Size i = 1; i < r.size(); i++) {
                oss << ", " << r[i];
            }
            oss << "\"";
        }
    } else if (anyType.type() == typeid(std::vector<double>)) {
        resultType = "vector_double";
        std::vector<double> r = boost::any_cast<std::vector<double>>(anyType);
        if (r.size() == 0) {
            oss << "";
        } else {
            oss << std::fixed << std::setprecision(precision) << "\"";
            if (r[0] != Null<Real>())
                oss << r[0];
            for (Size i = 1; i < r.size(); i++) {
                oss << ", ";
                if (r[i] != Null<Real>())
                    oss << r[i];
            }
            oss << "\"";
        }
    } else if (anyType.type() == typeid(std::vector<Date>)) {
        resultType = "vector_date";
        std::vector<Date> r = boost::any_cast<std::vector<Date>>(anyType);
        if (r.size() == 0) {
            oss << "";
        } else {
            oss << std::fixed << std::setprecision(precision) << "\"" << to_string(r[0]);
            for (Size i = 1; i < r.size(); i++) {
                oss << ", " << to_string(r[i]);
            }
            oss << "\"";
        }
    } else if (anyType.type() == typeid(std::vector<std::string>)) {
        resultType = "vector_string";
        std::vector<std::string> r = boost::any_cast<std::vector<std::string>>(anyType);
        if (r.size() == 0) {
            oss << "";
        } else {
            oss << std::fixed << std::setprecision(precision) << "\"" << r[0];
            for (Size i = 1; i < r.size(); i++) {
                oss << ", " << r[i];
            }
            oss << "\"";
        }
    } else if (anyType.type() == typeid(std::vector<CashFlowResults>)) {
        resultType = "vector_cashflows";
        std::vector<CashFlowResults> r = boost::any_cast<std::vector<CashFlowResults>>(anyType);
        if (!r.empty()) {
            oss << std::fixed << std::setprecision(precision) << "\"" << r[0];
            for (Size i = 1; i < r.size(); ++i) {
                oss << ", " << r[i];
            }
            oss << "\"";
        }
    } else if (anyType.type() == typeid(QuantLib::Matrix)) {
        resultType = "matrix";
        QuantLib::Matrix r = boost::any_cast<QuantLib::Matrix>(anyType);
        std::regex pattern("\n");
        std::ostringstream tmp;
        tmp << std::setprecision(precision) << r;
        oss << std::fixed << std::regex_replace(tmp.str(), pattern, std::string(""));
    } else if (anyType.type() == typeid(QuantLib::Array)) {
        resultType = "array";
        QuantLib::Array r = boost::any_cast<QuantLib::Array>(anyType);
        oss << std::fixed << std::setprecision(precision) << r;
    } else if (anyType.type() == typeid(QuantLib::Currency)) {
        resultType = "currency";
        QuantLib::Currency r = boost::any_cast<QuantLib::Currency>(anyType);
        oss << r;
    } else {
        ALOG("Unsupported Boost::Any type");
        resultType = "unsupported_type";
    }
    return make_pair(resultType, oss.str());
}

QuantLib::RateAveraging::Type parseOvernightIndexFutureNettingType(const std::string& s) {
    if (s == "Averaging") {
        return QuantLib::RateAveraging::Type::Simple;
    } else if (s == "Compounding") {
        return QuantLib::RateAveraging::Type::Compound;
    } else {
        QL_FAIL("Overnight Index Future Netting Type '" << s << "' not known, expected 'Averaging' or 'Compounding'");
    }
}

std::ostream& operator<<(std::ostream& os, QuantLib::RateAveraging::Type t) {
    if (t == QuantLib::RateAveraging::Type::Simple)
        return os << "Averaging";
    else if (t == QuantLib::RateAveraging::Type::Compound)
        return os << "Compounding";
    else {
        QL_FAIL("Internal error: unknown RateAveraging::Type - check implementation of operator<< "
                "for this enum");
    }
}

FutureConvention::DateGenerationRule parseFutureDateGenerationRule(const std::string& s) {
    if (s == "IMM")
        return FutureConvention::DateGenerationRule::IMM;
    else if (s == "FirstDayOfMonth")
        return FutureConvention::DateGenerationRule::FirstDayOfMonth;
    else {
        QL_FAIL("FutureConvention /  DateGenerationRule '" << s << "' not known, expect 'IMM' or 'FirstDayOfMonth'");
    }
}

std::ostream& operator<<(std::ostream& os, FutureConvention::DateGenerationRule t) {
    if (t == FutureConvention::DateGenerationRule::IMM)
        return os << "IMM";
    else if (t == FutureConvention::DateGenerationRule::FirstDayOfMonth)
        return os << "FirstDayOfMonth";
    else {
        QL_FAIL("Internal error: unknown FutureConvention::DateGenerationRule - check implementation of operator<< "
                "for this enum");
    }
}

InflationSwapConvention::PublicationRoll parseInflationSwapPublicationRoll(const string& s) {
    using IPR = InflationSwapConvention::PublicationRoll;
    if (s == "None") {
        return IPR::None;
    } else if (s == "OnPublicationDate") {
        return IPR::OnPublicationDate;
    } else if (s == "AfterPublicationDate") {
        return IPR::AfterPublicationDate;
    } else {
        QL_FAIL("InflationSwapConvention::PublicationRoll '"
                << s << "' not known, expect "
                << "'None', 'OnPublicationDate' or 'AfterPublicationDate'");
    }
}

QuantLib::Rounding::Type parseRoundingType(const std::string& s) {
    static map<string, QuantLib::Rounding::Type> m = {{"Up", QuantLib::Rounding::Type::Up},
                                                      {"Down", QuantLib::Rounding::Type::Down},
                                                      {"Closest", QuantLib::Rounding::Type::Closest},
                                                      {"Floor", QuantLib::Rounding::Type::Floor},
                                                      {"Ceiling", QuantLib::Rounding::Type::Ceiling}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Rounding type \"" << s << "\" not recognized");
    }
}

Barrier::Type parseBarrierType(const std::string& s) {
    static map<string, Barrier::Type> type = {{"DownAndIn", Barrier::Type::DownIn},
                                              {"UpAndIn", Barrier::Type::UpIn},
                                              {"DownAndOut", Barrier::Type::DownOut},
                                              {"UpAndOut", Barrier::Type::UpOut},
                                              // Maintain old versions for backwards compatibility
                                              {"Down&In", Barrier::Type::DownIn},
                                              {"Up&In", Barrier::Type::UpIn},
                                              {"Down&Out", Barrier::Type::DownOut},
                                              {"Up&Out", Barrier::Type::UpOut}};

    auto it = type.find(s);
    if (it != type.end()) {
        return it->second;
    } else {
        QL_FAIL("Barrier type \"" << s << "\" not recognized");
    }
}

DoubleBarrier::Type parseDoubleBarrierType(const std::string& s) {
    static map<string, DoubleBarrier::Type> type = {
        {"KnockIn", DoubleBarrier::Type::KnockIn},
        {"KnockOut", DoubleBarrier::Type::KnockOut},
        {"KIKO", DoubleBarrier::Type::KIKO},
        {"KOKI", DoubleBarrier::Type::KOKI},
    };

    auto it = type.find(s);
    if (it != type.end()) {
        return it->second;
    } else {
        QL_FAIL("DoubleBarrier type \"" << s << "\" not recognized");
    }
}

ostream& operator<<(ostream& os, InflationSwapConvention::PublicationRoll pr) {
    using IPR = InflationSwapConvention::PublicationRoll;
    if (pr == IPR::None) {
        return os << "None";
    } else if (pr == IPR::OnPublicationDate) {
        return os << "OnPublicationDate";
    } else if (pr == IPR::AfterPublicationDate) {
        return os << "AfterPublicationDate";
    } else {
        QL_FAIL("Unknown InflationSwapConvention::PublicationRoll.");
    }
}

std::ostream& operator<<(std::ostream& os, SobolBrownianGenerator::Ordering t) {
    static map<SobolBrownianGenerator::Ordering, string> m = {{SobolBrownianGenerator::Ordering::Factors, "Factors"},
                                                              {SobolBrownianGenerator::Ordering::Steps, "Steps"},
                                                              {SobolBrownianGenerator::Ordering::Diagonal, "Diagonal"}};
    auto it = m.find(t);
    if (it != m.end()) {
        return os << it->second;
    } else {
        QL_FAIL("Internal error: unknown SobolBrownianGenerator::Ordering - check implementation of operator<< "
                "for this enum");
    }
}

std::ostream& operator<<(std::ostream& os, SobolRsg::DirectionIntegers t) {
    static map<SobolRsg::DirectionIntegers, string> m = {
        {SobolRsg::DirectionIntegers::Unit, "Unit"},
        {SobolRsg::DirectionIntegers::Jaeckel, "Jaeckel"},
        {SobolRsg::DirectionIntegers::SobolLevitan, "SobolLevitan"},
        {SobolRsg::DirectionIntegers::SobolLevitanLemieux, "SobolLevitanLemieux"},
        {SobolRsg::DirectionIntegers::JoeKuoD5, "JoeKuoD5"},
        {SobolRsg::DirectionIntegers::JoeKuoD6, "JoeKuoD6"},
        {SobolRsg::DirectionIntegers::JoeKuoD7, "JoeKuoD7"},
        {SobolRsg::DirectionIntegers::Kuo, "Kuo"},
        {SobolRsg::DirectionIntegers::Kuo2, "Kuo2"},
        {SobolRsg::DirectionIntegers::Kuo3, "Kuo3"}};
    auto it = m.find(t);
    if (it != m.end()) {
        return os << it->second;
    } else {
        QL_FAIL("Internal error: unknown SobolRsg::DirectionIntegers - check implementation of operator<< "
                "for this enum");
    }
}

std::ostream& operator<<(std::ostream& out, QuantExt::CrossAssetModel::Discretization dis) {
    switch (dis) {
    case QuantExt::CrossAssetModel::Discretization::Exact:
        return out << "Exact";
    case QuantExt::CrossAssetModel::Discretization::Euler:
        return out << "Euler";
    default:
        return out << "?";
    }
}

using ADCP = CommodityFutureConvention::AveragingData::CalculationPeriod;
ADCP parseAveragingDataPeriod(const string& s) {
    if (s == "PreviousMonth") {
        return ADCP::PreviousMonth;
    } else if (s == "ExpiryToExpiry") {
        return ADCP::ExpiryToExpiry;
    } else {
        QL_FAIL("AveragingData::CalculationPeriod '" << s << "' not known, expect "
                                                     << "'PreviousMonth' or 'ExpiryToExpiry'");
    }
}

ostream& operator<<(ostream& os, ADCP cp) {
    if (cp == ADCP::PreviousMonth) {
        return os << "PreviousMonth";
    } else if (cp == ADCP::ExpiryToExpiry) {
        return os << "ExpiryToExpiry";
    } else {
        QL_FAIL("Unknown AveragingData::CalculationPeriod.");
    }
}

using PST = PriceSegment::Type;
PriceSegment::Type parsePriceSegmentType(const string& s) {
    if (s == "Future") {
        return PST::Future;
    } else if (s == "AveragingFuture") {
        return PST::AveragingFuture;
    } else if (s == "AveragingSpot") {
        return PST::AveragingSpot;
    } else if (s == "AveragingOffPeakPower") {
        return PST::AveragingOffPeakPower;
    } else if (s == "OffPeakPowerDaily") {
        return PST::OffPeakPowerDaily;
    } else {
        QL_FAIL("PriceSegment::Type '" << s << "' not known, expect "
                                       << "'Future', 'AveragingFuture' or 'AveragingSpot'");
    }
}

ostream& operator<<(ostream& os, PriceSegment::Type pst) {
    if (pst == PST::Future) {
        return os << "Future";
    } else if (pst == PST::AveragingFuture) {
        return os << "AveragingFuture";
    } else if (pst == PST::AveragingSpot) {
        return os << "AveragingSpot";
    } else if (pst == PST::AveragingOffPeakPower) {
        return os << "AveragingOffPeakPower";
    } else if (pst == PST::OffPeakPowerDaily) {
        return os << "OffPeakPowerDaily";
    } else {
        QL_FAIL("Unknown PriceSegment::Type.");
    }
}

using CQF = CommodityQuantityFrequency;
CommodityQuantityFrequency parseCommodityQuantityFrequency(const string& s) {
    if (iequals(s, "PerCalculationPeriod")) {
        return CQF::PerCalculationPeriod;
    } else if (iequals(s, "PerCalendarDay")) {
        return CQF::PerCalendarDay;
    } else if (iequals(s, "PerPricingDay")) {
        return CQF::PerPricingDay;
    } else if (iequals(s, "PerHour")) {
        return CQF::PerHour;
    } else if (iequals(s, "PerHourAndCalendarDay")) {
        return CQF::PerHourAndCalendarDay;
    } else {
        QL_FAIL("Could not parse " << s << " to CommodityQuantityFrequency");
    }
}

ostream& operator<<(ostream& os, CommodityQuantityFrequency cqf) {
    if (cqf == CQF::PerCalculationPeriod) {
        return os << "PerCalculationPeriod";
    } else if (cqf == CQF::PerCalendarDay) {
        return os << "PerCalendarDay";
    } else if (cqf == CQF::PerPricingDay) {
        return os << "PerPricingDay";
    } else if (cqf == CQF::PerHour) {
        return os << "PerHour";
    } else if (cqf == CQF::PerHourAndCalendarDay) {
        return os << "PerHourAndCalendarDay";
    } else {
        QL_FAIL("Do not recognise CommodityQuantityFrequency " << static_cast<int>(cqf));
    }
}

ostream& operator<<(ostream& os, Rounding::Type t) {
    static map<Rounding::Type, string> m = {{Rounding::Type::Up, "Up"},
                                            {Rounding::Type::Down, "Down"},
                                            {Rounding::Type::Closest, "Closest"},
                                            {Rounding::Type::Floor, "Floor"},
                                            {Rounding::Type::Ceiling, "Ceiling"}};
    auto it = m.find(t);
    if (it != m.end()) {
        return os << it->second;
    } else {
        QL_FAIL("Internal error: unknown Rounding::Type - check implementation of operator<< "
                "for this enum");
    }
}

using QuantExt::CdsOption;
CdsOption::StrikeType parseCdsOptionStrikeType(const string& s) {
    using ST = CdsOption::StrikeType;
    if (s == "Spread") {
        return ST::Spread;
    } else if (s == "Price") {
        return ST::Price;
    } else {
        QL_FAIL("CdsOption::StrikeType \"" << s << "\" not recognized");
    }
}

Average::Type parseAverageType(const std::string& s) {
    if (s == "Arithmetic") {
        return Average::Type::Arithmetic;
    } else if (s == "Geometric") {
        return Average::Type::Geometric;
    } else {
        QL_FAIL("Average::Type '" << s << "' not recognized. Should be Arithmetic or Geometric");
    }
}

QuantExt::BondIndex::PriceQuoteMethod parsePriceQuoteMethod(const std::string& s) {
    if (s == "CurrencyPerUnit")
        return QuantExt::BondIndex::PriceQuoteMethod::CurrencyPerUnit;
    else if (s == "PercentageOfPar")
        return QuantExt::BondIndex::PriceQuoteMethod::PercentageOfPar;
    else {
        QL_FAIL("PriceQuoteMethod '" << s << "' not recognized. Expected CurrencyPerUnit or PercentageOfPar.");
    }
}

std::ostream& operator<<(std::ostream& os, QuantExt::BondIndex::PriceQuoteMethod p) {
    if (p == QuantExt::BondIndex::PriceQuoteMethod::PercentageOfPar)
        os << "PercentageOfPar";
    else if (p == QuantExt::BondIndex::PriceQuoteMethod::CurrencyPerUnit)
        os << "CurrencyPerUnit";
    else
        os << "Unknown PriceQuoteMethod (" << static_cast<int>(p) << ")";
    return os;
}

std::vector<std::string> getCorrelationTokens(const std::string& name) {
    // Look for & first as it avoids collisions with : which can be used in an index name
    // if it is not there we fall back on the old behaviour
    string delim;
    if (name.find('&') != std::string::npos)
        delim = "&";
    else
        delim = "/:,";
    vector<string> tokens;
    boost::split(tokens, name, boost::is_any_of(delim));
    QL_REQUIRE(tokens.size() == 2,
               "invalid correlation name '" << name << "', expected Index2:Index1 or Index2/Index1 or Index2&Index1");
    return tokens;
}

string fxDominance(const string& s1, const string& s2) {

    // short cut for trivial inputs EUR,EUR => EUREUR

    if (s1 == s2)
        return s1 + s2;

    // This should run even if we don't have s1 or s2 in our table, it should not throw
    // It will also work if s1 == s2

    static vector<string> dominance = {// Precious Metals (always are before currencies, Metal crosses are not
                                       // common so the ordering of the actual 4 here is just the standard order we see
                                       "XAU", "XAG", "XPT", "XPD",
                                       // The majors (except JPY)
                                       "EUR", "GBP", "AUD", "NZD", "USD", "CAD", "CHF", "ZAR",
                                       // The rest - not really sure about some of these (MXNSEK anyone)
                                       "MYR", "SGD",               // not sure of order here
                                       "DKK", "NOK", "SEK",        // order here is correct
                                       "HKD", "THB", "TWD", "MXN", // not sure of order here
                                       "CNY", "CNH",

                                       // JPY at the end (of majors)
                                       "JPY",
                                       // JPYIDR and JPYKRW - who knew!
                                       "IDR", "KRW"};

    auto p1 = std::find(dominance.begin(), dominance.end(), s1);
    auto p2 = std::find(dominance.begin(), dominance.end(), s2);

    // if both on the list - we return the first one first
    if (p1 != dominance.end() && p2 != dominance.end()) {
        if (p1 > p2)
            return s2 + s1;
        else
            return s1 + s2;
    }
    // if nether on the list - we return s1+s2
    if (p1 == dominance.end() && p2 == dominance.end()) {
        WLOG("No dominance for either " << s1 << " or " << s2 << " assuming " << s1 + s2);
        return s1 + s2;
    }

    // if one on the list - we return that first (unless it's JPY - in which case it's last)
    if (s1 == "JPY")
        return s2 + s1;
    else if (s2 == "JPY")
        return s1 + s2;
    else {
        if (p1 != dominance.end())
            return s1 + s2;
        else
            return s2 + s1;
    }
}

string normaliseFxIndex(const std::string& indexName) {
    auto fx = ore::data::parseFxIndex(indexName);
    std::string ccy1 = fx->sourceCurrency().code();
    std::string ccy2 = fx->targetCurrency().code();
    if (ore::data::fxDominance(ccy1, ccy2) != ccy1 + ccy2)
        return ore::data::inverseFxIndex(indexName);
    else
        return indexName;
}

MomentType parseMomentType(const std::string& s) {
    static map<string, MomentType> momentTypes = {
        {"Variance", MomentType::Variance},
        {"Volatility", MomentType::Volatility},
    };
    auto it = momentTypes.find(s);
    if (it != momentTypes.end()) {
        return it->second;
    } else {
        QL_FAIL("momentTypes \"" << s << "\" not recognized");
    }
}

CreditPortfolioSensitivityDecomposition parseCreditPortfolioSensitivityDecomposition(const std::string& s) {
    if (s == "Underlying") {
        return CreditPortfolioSensitivityDecomposition::Underlying;
    } else if (s == "NotionalWeighted") {
        return CreditPortfolioSensitivityDecomposition::NotionalWeighted;
    } else if (s == "LossWeighted") {
        return CreditPortfolioSensitivityDecomposition::LossWeighted;
    } else if (s == "DeltaWeighted") {
        return CreditPortfolioSensitivityDecomposition::DeltaWeighted;
    } else {
        QL_FAIL("CreditPortfolioSensitivityDecomposition '"
                << s << "' invalid, expected Underlying, NotionalWeighted, LossWeighted, DeltaWeighted");
    }
}

std::ostream& operator<<(std::ostream& os, const CreditPortfolioSensitivityDecomposition d) {
    if (d == CreditPortfolioSensitivityDecomposition::Underlying) {
        return os << "Underlying";
    } else if (d == CreditPortfolioSensitivityDecomposition::NotionalWeighted) {
        return os << "NotionalWeighted";
    } else if (d == CreditPortfolioSensitivityDecomposition::LossWeighted) {
        return os << "LossWeighted";
    } else if (d == CreditPortfolioSensitivityDecomposition::DeltaWeighted) {
        return os << "DeltaWeighted";
    } else {
        QL_FAIL("Unknonw CreditPortfolioSensitivitiyDecomposition value " << static_cast<std::size_t>(d));
    }
}

QuantLib::Pillar::Choice parsePillarChoice(const std::string& s) {
    /* we support
       - the string corresponding to the enum label (preferred, first alternative below)
       - the string generated by operator<<() in QuantLib */
    if (s == "MaturityDate" || s == "MaturityPillarDate")
        return QuantLib::Pillar::MaturityDate;
    else if (s == "LastRelevantDate" || s == "LastRelevantPillarDate")
        return QuantLib::Pillar::LastRelevantDate;
    else if (s == "CustomDate" || s == "CustomPillarDate")
        return QuantLib::Pillar::CustomDate;
    else {
        QL_FAIL("PillarChoice '" << s << "' not recognized, expected MaturityDate, LastRelevantDate, CustomDate");
    }
}

QuantExt::McMultiLegBaseEngine::RegressorModel parseRegressorModel(const std::string& s) {
    if (s == "Simple")
        return McMultiLegBaseEngine::RegressorModel::Simple;
    else if (s == "LaggedFX")
        return McMultiLegBaseEngine::RegressorModel::LaggedFX;
    else {
        QL_FAIL("RegressorModel '" << s << "' not recognized, expected Simple, LaggedFX");
    }
}

MporCashFlowMode parseMporCashFlowMode(const string& s){
    static map<string, MporCashFlowMode> m = {{"Unspecified", MporCashFlowMode::Unspecified},
                                              {"NonePay", MporCashFlowMode::NonePay},
                                              {"BothPay", MporCashFlowMode::BothPay},
                                              {"WePay", MporCashFlowMode::WePay},
                                              {"TheyPay", MporCashFlowMode::TheyPay}};
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Mpor cash flow mode \"" << s << "\" not recognized");
    }
}
std::ostream& operator<<(std::ostream& out, MporCashFlowMode t) {
    if (t == MporCashFlowMode::Unspecified)
        out << "Unspecified";
    else if (t == MporCashFlowMode::NonePay)
        out << "NonePay";
    else if (t == MporCashFlowMode::BothPay)
        out << "BothPay";
    else if (t == MporCashFlowMode::WePay)
        out << "WePay";
    else if (t == MporCashFlowMode::TheyPay)
        out << "TheyPay";
    else
        QL_FAIL("Mpor cash flow mode not covered, expected one of 'Unspecified', 'NonePay', 'BothPay', 'WePay', "
                "'TheyPay'.");
    return out;
}

SabrParametricVolatility::ModelVariant parseSabrParametricVolatilityModelVariant(const std::string& s) {
    static map<string, SabrParametricVolatility::ModelVariant> m = {
        {"Hagan2002Lognormal", SabrParametricVolatility::ModelVariant::Hagan2002Lognormal},
        {"Hagan2002Normal", SabrParametricVolatility::ModelVariant::Hagan2002Normal},
        {"Hagan2002NormalZeroBeta", SabrParametricVolatility::ModelVariant::Hagan2002NormalZeroBeta},
        {"Antonov2015FreeBoundaryNormal", SabrParametricVolatility::ModelVariant::Antonov2015FreeBoundaryNormal},
        {"KienitzLawsonSwaynePde", SabrParametricVolatility::ModelVariant::KienitzLawsonSwaynePde},
        {"FlochKennedy", SabrParametricVolatility::ModelVariant::FlochKennedy}};
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL(
            "SabrParametricVolatilityModelVariant '"
            << s
            << "' not recognized, expected one of 'Hagan2002Lognormal', 'Hagan2002Normal', 'Hagan2002NormalZeroBeta', "
               "'Antonov2015FreeBoundaryNormal', 'KienitzLawsonSwaynePde', 'FlochKennedy'.");
    }
}

std::ostream& operator<<(std::ostream& out, SabrParametricVolatility::ModelVariant m) {
    if (m == SabrParametricVolatility::ModelVariant::Hagan2002Lognormal) {
        out << "Hagan2002Lognormal";
    } else if (m == SabrParametricVolatility::ModelVariant::Hagan2002Normal) {
        out << "Hagan2002Normal";
    } else if (m == SabrParametricVolatility::ModelVariant::Hagan2002NormalZeroBeta) {
        out << "Hagan200NormalZeroBeta";
    } else if (m == SabrParametricVolatility::ModelVariant::Antonov2015FreeBoundaryNormal) {
        out << "AntonovNormalZeroBeta";
    } else if (m == SabrParametricVolatility::ModelVariant::KienitzLawsonSwaynePde) {
        out << "KienitzLawsonSwaynePde";
    } else if (m == SabrParametricVolatility::ModelVariant::FlochKennedy) {
        out << "FlochKennedy";
    } else {
        QL_FAIL("SabrParametricVolatility::ModelVariant (" << static_cast<int>(m)
                                                           << ") not recognized. This is an internal error.");
    }
    return out;
}

std::ostream& operator<<(std::ostream& os, Exercise::Type type) {
    if (type == Exercise::European) {
        os << "European";
    } else if (type == Exercise::Bermudan) {
        os << "Bermudan";
    } else if (type == Exercise::American) {
        os << "American";
    } else {
        QL_FAIL("Exercise::Type (" << static_cast<int>(type)
                                   << " not recognized. Expected 'European', 'Bermudan', or 'American'.");
    }

    return os;
}

} // namespace data
} // namespace ore
