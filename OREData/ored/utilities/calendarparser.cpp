/*
 Copyright (C) 2022 Quaternion Risk Management Ltd

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

/*! \file ored/utilities/calendarparser.hpp
    \brief calendar parser singleton class
    \ingroup utilities
*/

#include <ored/utilities/calendarparser.hpp>

#include <ql/time/calendars/all.hpp>
#include <qle/calendars/amendedcalendar.hpp>
#include <qle/calendars/austria.hpp>
#include <qle/calendars/belgium.hpp>
#include <qle/calendars/cme.hpp>
#include <qle/calendars/colombia.hpp>
#include <qle/calendars/cyprus.hpp>
#include <qle/calendars/france.hpp>
#include <qle/calendars/greece.hpp>
#include <qle/calendars/ice.hpp>
#include <qle/calendars/ireland.hpp>
#include <qle/calendars/islamicweekendsonly.hpp>
#include <qle/calendars/israel.hpp>
#include <qle/calendars/luxembourg.hpp>
#include <qle/calendars/malaysia.hpp>
#include <qle/calendars/mauritius.hpp>
#include <qle/calendars/netherlands.hpp>
#include <qle/calendars/peru.hpp>
#include <qle/calendars/philippines.hpp>
#include <qle/calendars/russia.hpp>
#include <qle/calendars/spain.hpp>
#include <qle/calendars/switzerland.hpp>
#include <qle/calendars/unitedarabemirates.hpp>
#include <qle/calendars/wmr.hpp>

#include <boost/algorithm/string.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

CalendarParser::CalendarParser() { reset(); }

QuantLib::Calendar CalendarParser::parseCalendar(const std::string& name) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    auto it = calendars_.find(name);
    if (it != calendars_.end())
        return it->second;
    else {
        // Try to split them up
        std::vector<std::string> calendarNames;
        split(calendarNames, name, boost::is_any_of(",()")); // , is delimiter, the brackets may arise if joint calendar
        // if we have only one token, we won't make progress and exit here to avoid an infinite loop by calling
        // parseCalendar() recursively below
        QL_REQUIRE(calendarNames.size() > 1, "Cannot convert \"" << name << "\" to calendar");
        // now remove any leading strings indicating a joint calendar
        calendarNames.erase(std::remove(calendarNames.begin(), calendarNames.end(), "JoinHolidays"),
                            calendarNames.end());
        calendarNames.erase(std::remove(calendarNames.begin(), calendarNames.end(), "JoinBusinessDays"),
                            calendarNames.end());
        calendarNames.erase(std::remove(calendarNames.begin(), calendarNames.end(), ""), calendarNames.end());
        // Populate a vector of calendars.
        std::vector<QuantLib::Calendar> calendars;
        for (Size i = 0; i < calendarNames.size(); i++) {
            boost::trim(calendarNames[i]);
            try {
                calendars.push_back(parseCalendar(calendarNames[i]));
            } catch (std::exception& e) {
                QL_FAIL("Cannot convert \"" << name << "\" to Calendar [exception:" << e.what() << "]");
            } catch (...) {
                QL_FAIL("Cannot convert \"" << name << "\" to Calendar [unhandled exception]");
            }
        }
        return QuantLib::JointCalendar(calendars);
    }
}

QuantLib::Calendar CalendarParser::addCalendar(const std::string baseName, std::string& newName) {
    auto cal = parseCalendar(baseName);
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    auto it = calendars_.find(newName);
    if (it == calendars_.end()) {
        QuantExt::AmendedCalendar tmp(cal, newName);
        calendars_[newName] = tmp;
        return std::move(tmp);
    } else {
        return it->second;
    }
}

void CalendarParser::reset() {
    resetAddedAndRemovedHolidays();

    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    // When adding to the static map, keep in mind that the calendar name on the LHS might be used to add or remove
    // holidays in calendaradjustmentconfig.xml. The calendar on the RHS of the mapping will then be adjusted, so this
    // latter calendar should never be a fallback like WeekendsOnly(), because then the WeekendsOnly() calendar would
    // unintentionally be adjusted. Instead, use a copy of the fallback calendar in these cases. For example, do not map
    // "AED" => WeekendsOnly()
    // but instead use the mapping
    // "AED" => AmendedCalendar(WeekendsOnly(), "AED")

    static std::map<std::string, QuantExt::Calendar> ref = {
        {"TGT", TARGET()},
        {"TARGET", TARGET()},

        // Country-Description
        {"CN-IB", China(China::IB)},
        {"US-FED", UnitedStates(UnitedStates::FederalReserve)},
        {"US-GOV", UnitedStates(UnitedStates::GovernmentBond)},
        {"US-NERC", UnitedStates(UnitedStates::NERC)},
        {"US-NYSE", UnitedStates(UnitedStates::NYSE)},
        {"US-SET", UnitedStates(UnitedStates::Settlement)},
        {"US-SOFR", UnitedStates(UnitedStates::SOFR)},

        // Country full name to Settlement/Default
        {"Australia", Australia()},
        {"Canada", Canada()},
        {"Cyprus", Cyprus()},
        {"Denmark", Denmark()},
        {"Greece", Greece()},
        {"Ireland", Ireland(Ireland::BankHolidays)},
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
        {"USNY", UnitedStates(UnitedStates::Settlement)},

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
        {"CY", Cyprus()},
        {"CZ", CzechRepublic()},
        {"DK", Denmark()},
        {"FI", Finland()},
        {"FR", QuantExt::France()},
        {"GR", Greece()},
        {"DE", Germany(Germany::Settlement)},
        {"HK", HongKong()},
        {"HU", Hungary()},
        {"IE", Ireland(Ireland::BankHolidays)},
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
        // {"SA", SaudiArabia()},
        {"SG", Singapore()},
        {"ZA", SouthAfrica()},
        {"KR", SouthKorea(SouthKorea::Settlement)},
        {"SE", Sweden()},
        {"CH", QuantExt::Switzerland()},
        {"TW", Taiwan()},
        {"TH", Thailand()},
        {"TR", Turkey()},
        {"UA", Ukraine()},
        {"GB", UnitedKingdom()},
        {"US", UnitedStates(UnitedStates::Settlement)},
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
        {"CYP", Cyprus()},
        {"CZE", CzechRepublic()},
        {"DNK", Denmark()},
        {"FIN", Finland()},
        {"GRC", Greece()},
        //{"FRA", QuantExt::France()},
        {"DEU", Germany(Germany::Settlement)},
        {"HKG", HongKong()},
        {"HUN", Hungary()},
        {"ISL", Iceland()},

        {"IRL", Ireland(Ireland::BankHolidays)},
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
        {"THA", Thailand()},
        {"TUR", Turkey()},
        {"UKR", Ukraine()},
        {"GBR", UnitedKingdom()},
        {"USA", UnitedStates(UnitedStates::Settlement)},
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
        {"THB", Thailand()},
        {"TRY", Turkey()},
        {"UAH", Ukraine()},
        {"GBP", UnitedKingdom()},
        {"USD", UnitedStates(UnitedStates::Settlement)},
        {"BEF", Belgium()},
        {"LUF", Luxembourg()},
        {"ATS", QuantExt::Austria()},

        // Minor Currencies
        {"GBp", UnitedKingdom()},
        {"GBX", UnitedKingdom()},
        {"ILa", QuantLib::Israel()},
        {"ILX", QuantLib::Israel()},
        {"ILs", QuantLib::Israel()},
        {"ILA", QuantLib::Israel()},
        {"ZAc", SouthAfrica()},
        {"ZAC", SouthAfrica()},
        {"ZAX", SouthAfrica()},

        // fallback to IslamicWeekendsOnly for these ccys and use amendmends
        {"AED", AmendedCalendar(UnitedArabEmirates(), "AED")},
        {"AE", AmendedCalendar(UnitedArabEmirates(), "AED")},
        {"ARE", AmendedCalendar(UnitedArabEmirates(), "AED")},

        // fallback to amended Mauritius calendar.
        {"MU", AmendedCalendar(Mauritius(), "MUR")},
        {"MUR", AmendedCalendar(Mauritius(), "MUR")},
        {"MUS", AmendedCalendar(Mauritius(), "MUR")},
        // fallback to WeekendsOnly for these emerging ccys
        {"BHD", AmendedCalendar(WeekendsOnly(), "BHD")},
        {"CLF", AmendedCalendar(WeekendsOnly(), "CLF")},
        {"EGP", AmendedCalendar(WeekendsOnly(), "EGP")},
        {"KWD", AmendedCalendar(WeekendsOnly(), "KWD")},
        {"KZT", AmendedCalendar(WeekendsOnly(), "KZT")},
        {"MAD", AmendedCalendar(WeekendsOnly(), "MAD")},
        {"MXV", AmendedCalendar(WeekendsOnly(), "MXV")},
        {"NGN", AmendedCalendar(WeekendsOnly(), "MGN")},
        {"OMR", AmendedCalendar(WeekendsOnly(), "OMR")},
        {"PKR", AmendedCalendar(WeekendsOnly(), "PKR")},
        {"QAR", AmendedCalendar(WeekendsOnly(), "QAR")},
        {"UYU", AmendedCalendar(WeekendsOnly(), "UYU")},
        {"TND", AmendedCalendar(WeekendsOnly(), "TND")},
        {"VND", AmendedCalendar(WeekendsOnly(), "VND")},
        // new GFMA currencies
        {"AOA", AmendedCalendar(WeekendsOnly(), "AOA")},
        {"BGN", AmendedCalendar(WeekendsOnly(), "BGN")},
        {"ETB", AmendedCalendar(WeekendsOnly(), "ETB")},
        {"GEL", AmendedCalendar(WeekendsOnly(), "GEL")},
        {"GHS", AmendedCalendar(WeekendsOnly(), "GHS")},
        {"HRK", AmendedCalendar(WeekendsOnly(), "HRK")},
        {"JOD", AmendedCalendar(WeekendsOnly(), "JOD")},
        {"KES", AmendedCalendar(WeekendsOnly(), "KES")},
        {"LKR", AmendedCalendar(WeekendsOnly(), "LKR")},
        {"RSD", AmendedCalendar(WeekendsOnly(), "RSD")},
        {"UGX", AmendedCalendar(WeekendsOnly(), "UGX")},
        {"XOF", AmendedCalendar(WeekendsOnly(), "XOF")},
        {"ZMW", AmendedCalendar(WeekendsOnly(), "ZMW")},

        // ISO 10383 MIC Exchange
        {"XASX", Australia(Australia::ASX)},
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
        {"MISX", RussiaModified(Russia::MOEX)},
        {"XKRX", SouthKorea(SouthKorea::KRX)},
        {"XSWX", QuantExt::Switzerland(QuantExt::Switzerland::SIX)},
        {"XLON", UnitedKingdom(UnitedKingdom::Exchange)},
        {"XLME", UnitedKingdom(UnitedKingdom::Metals)},
        {"XNYS", UnitedStates(UnitedStates::NYSE)},
        {"XDUB", Ireland()},
        {"XPAR", QuantLib::France(QuantLib::France::Exchange)},

        // Other / Legacy
        {"DEN", Denmark()}, // TODO: consider remove it, not ISO
        {"Telbor", QuantExt::Israel(QuantExt::Israel::Telbor)},
        {"London stock exchange", UnitedKingdom(UnitedKingdom::Exchange)},
        {"LNB", UnitedKingdom()},
        {"New York stock exchange", UnitedStates(UnitedStates::NYSE)},
        {"SOFR fixing calendar", UnitedStates(UnitedStates::SOFR)},
        {"NGL", Netherlands()},
        {"NYB", UnitedStates(UnitedStates::Settlement)},
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

    calendars_ = ref;

    // add ql calendar names
    for (auto const& c : ref) {
        calendars_[c.second.name()] = c.second;
    }
}

void CalendarParser::resetAddedAndRemovedHolidays() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    for (auto& m : calendars_) {
        m.second.resetAddedAndRemovedHolidays();
    }
}

} // namespace data
} // namespace ore
