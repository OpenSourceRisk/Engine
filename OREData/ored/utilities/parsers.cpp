/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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
#include <ored/utilities/calendaradjustmentconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/currencies/all.hpp>
#include <ql/errors.hpp>
#include <ql/indexes/all.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/all.hpp>
#include <ql/utilities/dataparsers.hpp>
#include <ql/version.hpp>
#include <qle/calendars/austria.hpp>
#include <qle/calendars/belgium.hpp>
#include <qle/calendars/chile.hpp>
#include <qle/calendars/cme.hpp>
#include <qle/calendars/colombia.hpp>
#include <qle/calendars/france.hpp>
#include <qle/calendars/ice.hpp>
#include <qle/calendars/israel.hpp>
#include <qle/calendars/largejointcalendar.hpp>
#include <qle/calendars/luxembourg.hpp>
#include <qle/calendars/malaysia.hpp>
#include <qle/calendars/netherlands.hpp>
#include <qle/calendars/peru.hpp>
#include <qle/calendars/philippines.hpp>
#include <qle/calendars/spain.hpp>
#include <qle/calendars/switzerland.hpp>
#include <qle/calendars/thailand.hpp>
#include <qle/calendars/wmr.hpp>
#include <qle/currencies/africa.hpp>
#include <qle/currencies/america.hpp>
#include <qle/currencies/asia.hpp>
#include <qle/currencies/europe.hpp>
#include <qle/currencies/metals.hpp>
#include <qle/time/actual364.hpp>
#include <qle/time/yearcounter.hpp>

#include <boost/lexical_cast.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
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

Calendar parseCalendar(const string& s, bool adjustCalendar) {
    static map<string, Calendar> m = {
        {"TGT", TARGET()},
        {"TARGET", TARGET()},

        // Country-Description
        {"CN-IB", China(China::IB)},
        {"US-FED", UnitedStates(UnitedStates::FederalReserve)},
        {"US-GOV", UnitedStates(UnitedStates::GovernmentBond)},
        {"US-NERC", UnitedStates(UnitedStates::NERC)},
        {"US-NYSE", UnitedStates(UnitedStates::NYSE)},
        {"US-SET", UnitedStates(UnitedStates::Settlement)},

        // Country full name to Settlement/Default
        {"Australia", Australia()},
        {"Canada", Canada()},
        {"Denmark", Denmark()},
        {"Japan", Japan()},
        {"Norway", Norway()},
        {"Switzerland", QuantExt::Switzerland()},
        {"Sweden", Sweden()},
        {"Belgium", Belgium()},
        {"Luxembourg", Luxembourg()},
        {"Spain", Spain()},
        {"Austria", QuantExt::Austria()},

        // city specific calendars
        {"FRA", Germany(Germany::Settlement)},

        // Country City
        {"CATO", Canada()},
        {"CHZU", QuantExt::Switzerland()},
        {"JPTO", Japan()},
        {"GBLO", UnitedKingdom()},
        {"SEST", Sweden()},
        {"TRIS", Turkey()},
        {"USNY", UnitedStates()},

        // ISDA http://www.fpml.org/coding-scheme/business-center-7-15.xml
        {"EUTA", TARGET()},
        {"BEBR", Belgium()}, // Belgium, Brussels not in QL,

        // ISO 3166-1 Alpha-2 code
        {"AT", QuantExt::Austria()},
        {"AR", Argentina()},
        {"AU", Australia()},
        {"BW", Botswana()},
        {"BR", Brazil()},
        {"CA", Canada()},
        {"CL", Chile()},
        {"CN", China()},
        {"CO", Colombia()},
        {"CZ", CzechRepublic()},
        {"DK", Denmark()},
        {"FI", Finland()},
        {"FR", QuantExt::France()},
        {"DE", Germany(Germany::Settlement)},
        {"HK", HongKong()},
        {"HU", Hungary()},
        {"IS", Iceland()},
        {"IN", India()},
        {"ID", Indonesia()},
        {"IL", QuantLib::Israel()},
        {"IT", Italy()},
        {"JP", Japan()},
        {"MX", Mexico()},
        {"MY", Malaysia()},
        {"NL", Netherlands()},
        {"NO", Norway()},
        {"NZ", NewZealand()},
        {"PE", Peru()},
        {"PH", Philippines()},
        {"PL", Poland()},
        {"RO", Romania()},
        {"RU", Russia()},
        // {"SA", SaudiArabic()}
        {"SG", Singapore()},
        {"ZA", SouthAfrica()},
        {"KR", SouthKorea(SouthKorea::Settlement)},
        {"SE", Sweden()},
        {"CH", QuantExt::Switzerland()},
        {"TW", Taiwan()},
        {"TH", QuantExt::Thailand()},
        {"TR", Turkey()},
        {"UA", Ukraine()},
        {"GB", UnitedKingdom()},
        {"US", UnitedStates()},
        {"BE", Belgium()},
        {"LU", Luxembourg()},
        {"ES", Spain()},
        {"AT", QuantExt::Austria()},

        // ISO 3166-1 Alpha-3 code
        {"ARG", Argentina()},
        {"AUS", Australia()},
        {"ATS", QuantExt::Austria()},
        {"BWA", Botswana()},
        {"BRA", Brazil()},
        {"CAN", Canada()},
        {"CHL", Chile()},
        {"CHN", China()},
        {"COL", Colombia()},
        {"CZE", CzechRepublic()},
        {"DNK", Denmark()},
        {"FIN", Finland()},
        //{"FRA", QuantExt::France()},
        {"DEU", Germany(Germany::Settlement)},
        {"HKG", HongKong()},
        {"HUN", Hungary()},
        {"ISL", Iceland()},
        {"IND", India()},
        {"IDN", Indonesia()},
        {"ISR", QuantLib::Israel()},
        {"ITA", Italy()},
        {"JPN", Japan()},
        {"MEX", Mexico()},
        {"MYS", Malaysia()},
        {"NLD", Netherlands()},
        {"NOR", Norway()},
        {"NZL", NewZealand()},
        {"PER", Peru()},
        {"PHL", Philippines()},
        {"POL", Poland()},
        {"ROU", Romania()},
        {"RUS", Russia()},
        {"SAU", SaudiArabia()},
        {"SGP", Singapore()},
        {"ZAF", SouthAfrica()},
        {"KOR", SouthKorea(SouthKorea::Settlement)},
        {"SWE", Sweden()},
        {"CHE", QuantExt::Switzerland()},
        {"TWN", Taiwan()},
        {"THA", QuantExt::Thailand()},
        {"TUR", Turkey()},
        {"UKR", Ukraine()},
        {"GBR", UnitedKingdom()},
        {"USA", UnitedStates()},
        {"BEL", Belgium()},
        {"LUX", Luxembourg()},
        {"ESP", Spain()},
        {"AUT", QuantExt::Austria()},

        // ISO 4217 Currency Alphabetic code
        {"ARS", Argentina()},
        {"AUD", Australia()},
        {"BWP", Botswana()},
        {"BRL", Brazil()},
        {"CAD", Canada()},
        {"CLP", Chile()},
        {"CNH", China()},
        {"CNY", China()},
        {"COP", Colombia()},
        {"CZK", CzechRepublic()},
        {"DKK", Denmark()},
        {"FRF", QuantExt::France()},
        {"HKD", HongKong()},
        {"HUF", Hungary()},
        {"INR", India()},
        {"IDR", Indonesia()},
        {"ILS", QuantLib::Israel()},
        {"ISK", Iceland()},
        {"ITL", Italy()},
        {"JPY", Japan()},
        {"MXN", Mexico()},
        {"MYR", Malaysia()},
        {"NOK", Norway()},
        {"NZD", NewZealand()},
        {"PEN", Peru()},
        {"PHP", Philippines()},
        {"PLN", Poland()},
        {"RON", Romania()},
        {"RUB", Russia()},
        {"SAR", SaudiArabia()},
        {"SGD", Singapore()},
        {"ZAR", SouthAfrica()},
        {"KRW", SouthKorea(SouthKorea::Settlement)},
        {"SEK", Sweden()},
        {"CHF", QuantExt::Switzerland()},
        {"EUR", TARGET()},
        {"TWD", Taiwan()},
        {"THB", QuantExt::Thailand()},
        {"TRY", Turkey()},
        {"UAH", Ukraine()},
        {"GBP", UnitedKingdom()},
        {"USD", UnitedStates()},
        {"BEF", Belgium()},
        {"LUF", Luxembourg()},
        {"ATS", QuantExt::Austria()},

        // fallback to TARGET for these emerging ccys
        {"AED", TARGET()},
        {"BHD", TARGET()},
        {"CLF", TARGET()},
        {"EGP", TARGET()},
        {"KWD", TARGET()},
        {"KZT", TARGET()},
        {"MAD", TARGET()},
        {"MXV", TARGET()},
        {"NGN", TARGET()},
        {"OMR", TARGET()},
        {"PKR", TARGET()},
        {"QAR", TARGET()},
        {"UYU", TARGET()},
        {"TND", TARGET()},
        {"VND", TARGET()},

        // ISO 10383 MIC Exchange
        {"BVMF", Brazil(Brazil::Exchange)},
        {"XTSE", Canada(Canada::TSX)},
        {"XSHG", China(China::SSE)},
        {"XFRA", Germany(Germany::FrankfurtStockExchange)},
        {"XETR", Germany(Germany::Xetra)},
        {"ECAG", Germany(Germany::Eurex)},
        {"EUWA", Germany(Germany::Euwax)},
        {"XJKT", Indonesia(Indonesia::JSX)},
        {"XIDX", Indonesia(Indonesia::IDX)},
        {"XTAE", QuantLib::Israel(QuantLib::Israel::TASE)},
        {"XMIL", Italy(Italy::Exchange)},
        {"MISX", Russia(Russia::MOEX)},
        {"XKRX", SouthKorea(SouthKorea::KRX)},
        {"XSWX", QuantExt::Switzerland(QuantExt::Switzerland::SIX)},
        {"XLON", UnitedKingdom(UnitedKingdom::Exchange)},
        {"XLME", UnitedKingdom(UnitedKingdom::Metals)},
        {"XNYS", UnitedStates(UnitedStates::NYSE)},

        // Other / Legacy
        {"DEN", Denmark()}, // TODO: consider remove it, not ISO
        {"Telbor", QuantExt::Israel(QuantExt::Israel::Telbor)},
        {"London stock exchange", UnitedKingdom(UnitedKingdom::Exchange)},
        {"LNB", UnitedKingdom()},
        {"New York stock exchange", UnitedStates(UnitedStates::NYSE)},
        {"NGL", Netherlands()},
        {"NYB", UnitedStates()},
        {"SA", SouthAfrica()}, // TODO: consider remove it, not ISO & confuses with Saudi Arabia
        {"SS", Sweden()},      // TODO: consider remove it, not ISO & confuses with South Sudan
        {"SYB", Australia()},
        {"TKB", Japan()},
        {"TRB", Canada()},
        {"UK", UnitedKingdom()},
        {"UK settlement", UnitedKingdom()},
        {"US settlement", UnitedStates(UnitedStates::Settlement)},
        {"US with Libor impact", UnitedStates(UnitedStates::LiborImpact)},
        {"WMR", Wmr()},
        {"ZUB", QuantExt::Switzerland()},

        // ICE exchange calendars
        {"ICE_FuturesUS", ICE(ICE::FuturesUS)},
        {"ICE_FuturesUS_1", ICE(ICE::FuturesUS_1)},
        {"ICE_FuturesUS_2", ICE(ICE::FuturesUS_2)},
        {"ICE_FuturesEU", ICE(ICE::FuturesEU)},
        {"ICE_FuturesEU_1", ICE(ICE::FuturesEU_1)},
        {"ICE_EndexEnergy", ICE(ICE::EndexEnergy)},
        {"ICE_EndexEquities", ICE(ICE::EndexEquities)},
        {"ICE_SwapTradeUS", ICE(ICE::SwapTradeUS)},
        {"ICE_SwapTradeUK", ICE(ICE::SwapTradeUK)},
        {"ICE_FuturesSingapore", ICE(ICE::FuturesSingapore)},
        // CME exchange calendar
        {"CME", CME()},

        // Simple calendars
        {"WeekendsOnly", WeekendsOnly()},
        {"UNMAPPED", WeekendsOnly()},
        {"NullCalendar", NullCalendar()},
        {"", NullCalendar()}};
    static bool isInitialised = false;
    if (!isInitialised) {
        // extend the static map to include quantlib map names
        // allCals should be set, but it doesn't know how to order calendar
        vector<Calendar> allCals;
        for (auto it : m)
            allCals.push_back(it.second);
        for (auto cal : allCals)
            m[cal.name()] = cal;
        isInitialised = true;
    }

    auto it = m.find(s);
    if (it != m.end()) {
        Calendar cal = it->second;
        if (adjustCalendar) {
            // add custom holidays from populated calendar adjustments
            const CalendarAdjustmentConfig& config = CalendarAdjustments::instance().config();
            for (auto h : config.getHolidays(s)) {
                cal.addHoliday(h);
            }
            for (auto b : config.getBusinessDays(s)) {
                cal.removeHoliday(b);
            }
        }
        return cal;

    } else {
        // Try to split them up
        vector<string> calendarNames;
        split(calendarNames, s, boost::is_any_of(",()")); // , is delimiter, the brackets may arise if joint calendar
        // if we have only one token, we won't make progress and exit here to avoid an infinite loop by calling
        // parseCalendar() recursively below
        QL_REQUIRE(calendarNames.size() > 1, "Cannot convert \"" << s << "\" to calendar");
        // now remove any leading strings indicating a joint calendar
        calendarNames.erase(std::remove(calendarNames.begin(), calendarNames.end(), "JoinHolidays"),
                            calendarNames.end());
        calendarNames.erase(std::remove(calendarNames.begin(), calendarNames.end(), "JoinBusinessDays"),
                            calendarNames.end());
        calendarNames.erase(std::remove(calendarNames.begin(), calendarNames.end(), ""), calendarNames.end());
        // Populate a vector of calendars.
        vector<Calendar> calendars;
        for (Size i = 0; i < calendarNames.size(); i++) {
            boost::trim(calendarNames[i]);
            try {
                calendars.push_back(parseCalendar(calendarNames[i], adjustCalendar));
            } catch (std::exception& e) {
                QL_FAIL("Cannot convert \"" << s << "\" to Calendar [exception:" << e.what() << "]");
            } catch (...) {
                QL_FAIL("Cannot convert \"" << s << "\" to Calendar [unhandled exception]");
            }
        }

        return LargeJointCalendar(calendars);
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
                                        {"A365", Actual365Fixed()},
                                        {"A365F", Actual365Fixed()},
                                        {"Actual/365 (Fixed)", Actual365Fixed()},
                                        {"Actual/365 (fixed)", Actual365Fixed()},
                                        {"ACT/365.FIXED", Actual365Fixed()},
                                        {"ACT/365", Actual365Fixed()},
                                        {"ACT/365L", Actual365Fixed()},
                                        {"T360", Thirty360(Thirty360::USA)},
                                        {"30/360", Thirty360(Thirty360::USA)},
                                        {"30/360 (Bond Basis)", Thirty360(Thirty360::USA)},
                                        {"ACT/nACT", Thirty360(Thirty360::USA)},
                                        {"30E/360 (Eurobond Basis)", Thirty360(Thirty360::European)},
                                        {"30E/360", Thirty360(Thirty360::European)},
                                        {"30E/360.ISDA", Thirty360(Thirty360::European)},
                                        {"30/360 (Italian)", Thirty360(Thirty360::Italian)},
                                        {"ActActISDA", ActualActual(ActualActual::ISDA)},
                                        {"ACT/ACT.ISDA", ActualActual(ActualActual::ISDA)},
                                        {"Actual/Actual (ISDA)", ActualActual(ActualActual::ISDA)},
                                        {"ActualActual (ISDA)", ActualActual(ActualActual::ISDA)},
                                        {"ACT/ACT", ActualActual(ActualActual::ISDA)},
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
                                        {"A364", Actual364()},
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

Currency parseCurrency(const string& s) {
    static map<string, Currency> m = {
        {"ATS", ATSCurrency()}, {"AUD", AUDCurrency()}, {"BEF", BEFCurrency()}, {"BRL", BRLCurrency()},
        {"CAD", CADCurrency()}, {"CHF", CHFCurrency()}, {"CNH", CNHCurrency()}, {"CNY", CNYCurrency()},
        {"CZK", CZKCurrency()}, {"DEM", DEMCurrency()}, {"DKK", DKKCurrency()}, {"EUR", EURCurrency()},
        {"ESP", ESPCurrency()}, {"FIM", FIMCurrency()}, {"FRF", FRFCurrency()}, {"GBP", GBPCurrency()},
        {"GRD", GRDCurrency()}, {"HKD", HKDCurrency()}, {"HUF", HUFCurrency()}, {"IEP", IEPCurrency()},
        {"ITL", ITLCurrency()}, {"INR", INRCurrency()}, {"ISK", ISKCurrency()}, {"JPY", JPYCurrency()},
        {"KRW", KRWCurrency()}, {"LUF", LUFCurrency()}, {"NLG", NLGCurrency()}, {"NOK", NOKCurrency()},
        {"NZD", NZDCurrency()}, {"PLN", PLNCurrency()}, {"PTE", PTECurrency()}, {"RON", RONCurrency()},
        {"SEK", SEKCurrency()}, {"SGD", SGDCurrency()}, {"THB", THBCurrency()}, {"TRY", TRYCurrency()},
        {"TWD", TWDCurrency()}, {"USD", USDCurrency()}, {"ZAR", ZARCurrency()}, {"ARS", ARSCurrency()},
        {"CLP", CLPCurrency()}, {"COP", COPCurrency()}, {"IDR", IDRCurrency()}, {"ILS", ILSCurrency()},
        {"KWD", KWDCurrency()}, {"PEN", PENCurrency()}, {"PKR", PKRCurrency()}, {"MXN", MXNCurrency()},
        {"SAR", SARCurrency()}, {"RUB", RUBCurrency()}, {"TND", TNDCurrency()}, {"MYR", MYRCurrency()},
        {"UAH", UAHCurrency()}, {"KZT", KZTCurrency()}, {"QAR", QARCurrency()}, {"MXV", MXVCurrency()},
        {"CLF", CLFCurrency()}, {"EGP", EGPCurrency()}, {"BHD", BHDCurrency()}, {"OMR", OMRCurrency()},
        {"VND", VNDCurrency()}, {"AED", AEDCurrency()}, {"PHP", PHPCurrency()}, {"NGN", NGNCurrency()},
        {"MAD", MADCurrency()}, {"UYU", UYUCurrency()}, {"XAU", XAUCurrency()}, {"XAG", XAGCurrency()},
        {"XPD", XPDCurrency()}, {"XPT", XPTCurrency()}, {"KES", KESCurrency()}, {"LKR", LKRCurrency()},
        {"RSD", RSDCurrency()}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Currency \"" << s << "\" not recognized");
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
                                                  {"CDS2015", DateGeneration::CDS2015},
                                                  {"CDS", DateGeneration::CDS}};

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

void parseDateOrPeriod(const string& s, Date& d, Period& p, bool& isDate) {
    QL_REQUIRE(!s.empty(), "Cannot parse empty string as date or period");
    string c(1, s.back());
    bool isPeriod = c.find_first_of("DdWwMmYy") != string::npos;
    if (isPeriod) {
        p = ore::data::parsePeriod(s);
        isDate = false;
    } else {
        d = ore::data::parseDate(s);
        QL_REQUIRE(d != Date(), "Cannot parse \"" << s << "\" as date");
        isDate = true;
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

AmortizationType parseAmortizationType(const std::string& s) {
    static map<string, AmortizationType> type = {
        {"None", AmortizationType::None},
        {"FixedAmount", AmortizationType::FixedAmount},
        {"RelativeToInitialNotional", AmortizationType::RelativeToInitialNotional},
        {"RelativeToPreviousNotional", AmortizationType::RelativeToPreviousNotional},
        {"Annuity", AmortizationType::Annuity}};

    auto it = type.find(s);
    if (it != type.end()) {
        return it->second;
    } else {
        QL_FAIL("Amortitazion type \"" << s << "\" not recognized");
    }
}

SequenceType parseSequenceType(const std::string& s) {
    static map<string, SequenceType> seq = {{"MersenneTwister", SequenceType::MersenneTwister},
                                            {"MersenneTwisterAntithetic", SequenceType::MersenneTwisterAntithetic},
                                            {"Sobol", SequenceType::Sobol},
                                            {"SobolBrownianBridge", SequenceType::SobolBrownianBridge}};
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
    static std::map<std::string, FdmSchemeDesc> m = {
        {"Hundsdorfer", FdmSchemeDesc::Hundsdorfer()},     {"Douglas", FdmSchemeDesc::Douglas()},
        {"CraigSneyd", FdmSchemeDesc::CraigSneyd()},       {"ModifiedCraigSneyd", FdmSchemeDesc::ModifiedCraigSneyd()},
        {"ImplicitEuler", FdmSchemeDesc::ImplicitEuler()}, {"ExplicitEuler", FdmSchemeDesc::ExplicitEuler()},
        {"MethodOfLines", FdmSchemeDesc::MethodOfLines()}, {"TrBDF2", FdmSchemeDesc::TrBDF2()}};

    auto it = m.find(s);
    if (it != m.end())
        return it->second;
    else
        QL_FAIL("fdm scheme \"" << s << "\" not recognised");
}

AssetClass parseAssetClass(const std::string& s) {
    static map<string, AssetClass> assetClasses = {{"EQ", AssetClass::EQ},   {"FX", AssetClass::FX},
                                                   {"COM", AssetClass::COM}, {"IR", AssetClass::IR},
                                                   {"INF", AssetClass::INF}, {"CR", AssetClass::CR}};
    auto it = assetClasses.find(s);
    if (it != assetClasses.end()) {
        return it->second;
    } else {
        QL_FAIL("AssetClass \"" << s << "\" not recognized");
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

Average::Type parseAverageType(const std::string& s) {
    if (s == "Arithmetic") {
        return Average::Type::Arithmetic;
    } else if (s == "Geometric") {
        return Average::Type::Geometric;
    } else {
        QL_FAIL("Average::Type '" << s << "' not recognized. Should be Arithmetic or Geometric");
    }
}

} // namespace data
} // namespace ore
