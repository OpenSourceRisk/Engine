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

#include <ored/utilities/currencyparser.hpp>

#include <qle/currencies/africa.hpp>
#include <qle/currencies/america.hpp>
#include <qle/currencies/asia.hpp>
#include <qle/currencies/currencycomparator.hpp>
#include <qle/currencies/europe.hpp>
#include <qle/currencies/metals.hpp>

#include <ql/currencies/all.hpp>
#include <ql/errors.hpp>

#include <boost/algorithm/string.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

CurrencyParser::CurrencyParser() { reset(); }

QuantLib::Currency CurrencyParser::parseCurrency(const std::string& name) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    {
        auto it = currencies_.find(name);
        if (it != currencies_.end()) {
            return it->second;
        }
    }
    {
        auto it = preciousMetals_.find(name);
        if (it != preciousMetals_.end()) {
            return it->second;
        }
    }
    {
        auto it = cryptoCurrencies_.find(name);
        if (it != cryptoCurrencies_.end()) {
            return it->second;
        }
    }
    QL_FAIL("Currency \"" << name << "\" not recognized");
}

QuantLib::Currency CurrencyParser::parseMinorCurrency(const std::string& name) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    auto it = minorCurrencies_.find(name);
    if (it != minorCurrencies_.end()) {
        return it->second;
    }
    QL_FAIL("Currency \"" << name << "\" not recognized");
}

QuantLib::Currency CurrencyParser::parseCurrencyWithMinors(const std::string& name) const {
    try {
        return parseCurrency(name);
    } catch (...) {
    }
    return parseMinorCurrency(name);
}

std::pair<QuantLib::Currency, QuantLib::Currency>
CurrencyParser::parseCurrencyPair(const std::string& name, const std::string& delimiters) const {
    std::vector<std::string> tokens;
    tokens = boost::split(tokens, name, boost::is_any_of(delimiters));
    if (tokens.size() == 1) {
        if (tokens[0].size() > 6) {
            QL_FAIL("Failed to parse currency pair (" << tokens[0] << ")");
        }

        QuantLib::Currency ccy1 = parseCurrency(tokens[0].substr(0, 3));
        QuantLib::Currency ccy2 = parseCurrency(tokens[0].substr(3));
        return std::make_pair(ccy1, ccy2);
    } else if (tokens.size() == 2) {
        try {
            QuantLib::Currency ccy1 = parseCurrency(tokens[0]);
            QuantLib::Currency ccy2 = parseCurrency(tokens[1]);
            return std::make_pair(ccy1, ccy2);
        } catch (const std::exception& e) {
            QL_FAIL("Failed to parse currency pair (" << name << "): " << e.what());
        }
    } else {
        QL_FAIL("Failed to parse currency pair (" << name << ")");
    }
}

bool CurrencyParser::isValidCurrency(const std::string& name) const {
    try {
        parseCurrencyWithMinors(name);
        return true;
    } catch (...) {
    }
    return false;
}

bool CurrencyParser::isMinorCurrency(const std::string& name) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return minorCurrencies_.find(name) != minorCurrencies_.end();
}

bool CurrencyParser::isPseudoCurrency(const std::string& name) const {
    return isPreciousMetal(name) || isCryptoCurrency(name);
}

bool CurrencyParser::isPreciousMetal(const std::string& name) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return preciousMetals_.find(name) != preciousMetals_.end();
}

bool CurrencyParser::isCryptoCurrency(const std::string& name) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return cryptoCurrencies_.find(name) != cryptoCurrencies_.end();
}

bool CurrencyParser::hasMinorCurrency(const std::string& name) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    for (auto const& c : minorCurrencies_) {
        if (c.second.code() == name)
            return true;
    }
    return false;
}

std::string CurrencyParser::getMinorCurrency(const std::string& name) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    for (auto const& c : minorCurrencies_) {
        if (c.second.code() == name)
            return c.first;
    }
    QL_FAIL("no minor currency found for '" << name << "'");
}

std::set<std::string> CurrencyParser::pseudoCurrencyCodes() const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    std::set<std::string> tmp;
    for (auto const& c : preciousMetals_)
        tmp.insert(c.first);
    for (auto const& c : cryptoCurrencies_)
        tmp.insert(c.first);
    return tmp;
}

QuantLib::Real CurrencyParser::convertMinorToMajorCurrency(const std::string& s, QuantLib::Real value) {
    if (isMinorCurrency(s)) {
        QuantLib::Currency ccy = parseMinorCurrency(s);
        return value / ccy.fractionsPerUnit();
    } else {
        return value;
    }
}

void CurrencyParser::addCurrency(const std::string& newName, const QuantLib::Currency& currency) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    if (currencies_.find(newName) != currencies_.end() || preciousMetals_.find(newName) != preciousMetals_.end() ||
        cryptoCurrencies_.find(newName) != cryptoCurrencies_.end())
        return;
    currencies_[newName] = currency;
    addMinorCurrencyCodes(currency);
}

void CurrencyParser::addMetal(const std::string& newName, const QuantLib::Currency& currency) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    if (currencies_.find(newName) != currencies_.end() || preciousMetals_.find(newName) != preciousMetals_.end() ||
        cryptoCurrencies_.find(newName) != cryptoCurrencies_.end())
        return;
    preciousMetals_[newName] = currency;
}

void CurrencyParser::addCrypto(const std::string& newName, const QuantLib::Currency& currency) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    if (currencies_.find(newName) != currencies_.end() || preciousMetals_.find(newName) != preciousMetals_.end() ||
        cryptoCurrencies_.find(newName) != cryptoCurrencies_.end())
        return;
    cryptoCurrencies_[newName] = currency;
}

void CurrencyParser::addMinorCurrencyCodes(const QuantLib::Currency& currency) {
    for (auto const& c : currency.minorUnitCodes()) {
        minorCurrencies_[c] = currency;
    }
}

void CurrencyParser::reset() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    currencies_ = {{"AED", AEDCurrency()}, {"AOA", AOACurrency()}, {"ARS", ARSCurrency()}, {"ATS", ATSCurrency()},
                   {"AUD", AUDCurrency()}, {"BEF", BEFCurrency()}, {"BGN", BGNCurrency()}, {"BHD", BHDCurrency()},
                   {"BRL", BRLCurrency()}, {"BWP", BWPCurrency()}, {"CAD", CADCurrency()}, {"CHF", CHFCurrency()},
                   {"CLF", CLFCurrency()}, {"CLP", CLPCurrency()}, {"CNH", CNHCurrency()}, {"CNY", CNYCurrency()},
                   {"COP", COPCurrency()}, {"COU", COUCurrency()}, {"CZK", CZKCurrency()}, {"DEM", DEMCurrency()},
                   {"DKK", DKKCurrency()}, {"EGP", EGPCurrency()}, {"ESP", ESPCurrency()}, {"ETB", ETBCurrency()},
                   {"EUR", EURCurrency()}, {"FIM", FIMCurrency()}, {"FRF", FRFCurrency()}, {"GBP", GBPCurrency()},
                   {"GEL", GELCurrency()}, {"GHS", GHSCurrency()}, {"GRD", GRDCurrency()}, {"HKD", HKDCurrency()},
                   {"HRK", HRKCurrency()}, {"HUF", HUFCurrency()}, {"IDR", IDRCurrency()}, {"IEP", IEPCurrency()},
                   {"ILS", ILSCurrency()}, {"INR", INRCurrency()}, {"ISK", ISKCurrency()}, {"ITL", ITLCurrency()},
                   {"JOD", JODCurrency()}, {"JPY", JPYCurrency()}, {"KES", KESCurrency()}, {"KRW", KRWCurrency()},
                   {"KWD", KWDCurrency()}, {"KZT", KZTCurrency()}, {"LKR", LKRCurrency()}, {"LUF", LUFCurrency()},
                   {"MAD", MADCurrency()}, {"MUR", MURCurrency()}, {"MXN", MXNCurrency()}, {"MXV", MXVCurrency()},
                   {"MYR", MYRCurrency()}, {"NGN", NGNCurrency()}, {"NLG", NLGCurrency()}, {"NOK", NOKCurrency()},
                   {"NZD", NZDCurrency()}, {"OMR", OMRCurrency()}, {"PEN", PENCurrency()}, {"PHP", PHPCurrency()},
                   {"PKR", PKRCurrency()}, {"PLN", PLNCurrency()}, {"PTE", PTECurrency()}, {"QAR", QARCurrency()},
                   {"RON", RONCurrency()}, {"RSD", RSDCurrency()}, {"RUB", RUBCurrency()}, {"SAR", SARCurrency()},
                   {"SEK", SEKCurrency()}, {"SGD", SGDCurrency()}, {"THB", THBCurrency()}, {"TND", TNDCurrency()},
                   {"TRY", TRYCurrency()}, {"TWD", TWDCurrency()}, {"UAH", UAHCurrency()}, {"UGX", UGXCurrency()},
                   {"USD", USDCurrency()}, {"UYU", UYUCurrency()}, {"VND", VNDCurrency()}, {"XOF", XOFCurrency()},
                   {"ZAR", ZARCurrency()}, {"ZMW", ZMWCurrency()}};

    minorCurrencies_ = {{"GBp", GBPCurrency()}, {"GBX", GBPCurrency()}, {"ILa", ILSCurrency()}, {"ILX", ILSCurrency()},
                        {"ILs", ILSCurrency()}, {"KWf", KWDCurrency()}, {"ILA", ILSCurrency()}, {"ZAc", ZARCurrency()},
                        {"ZAC", ZARCurrency()}, {"ZAX", ZARCurrency()}};

    preciousMetals_ = {{"XAG", XAGCurrency()}, {"XAU", XAUCurrency()}, {"XPT", XPTCurrency()}, {"XPD", XPDCurrency()}};

    cryptoCurrencies_ = {{"XBT", BTCCurrency()}, {"BTC", BTCCurrency()}, {"ETH", ETHCurrency()}, {"ETC", ETCCurrency()},
                         {"BCH", BCHCurrency()}, {"XRP", XRPCurrency()}, {"LTC", LTCCurrency()}};

    for (auto const& c : currencies_) {
        addMinorCurrencyCodes(c.second);
    }
}

} // namespace data
} // namespace ore
