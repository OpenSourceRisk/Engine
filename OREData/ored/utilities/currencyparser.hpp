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

/*! \file ored/utilities/currencyparser.hpp
    \brief currency parser singleton class
    \ingroup utilities
*/

#pragma once

#include <map>
#include <ql/currency.hpp>
#include <ql/patterns/singleton.hpp>
#include <ql/time/calendar.hpp>

#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>

namespace ore {
namespace data {

class CurrencyParser : public QuantLib::Singleton<CurrencyParser, std::integral_constant<bool, true>> {
public:
    CurrencyParser();

    QuantLib::Currency parseCurrency(const std::string& name) const;
    QuantLib::Currency parseMinorCurrency(const std::string& name) const;
    QuantLib::Currency parseCurrencyWithMinors(const std::string& name) const;
    std::pair<QuantLib::Currency, QuantLib::Currency> parseCurrencyPair(const std::string& name,
                                                                        const std::string& delimiters) const;

    void addCurrency(const std::string& newName, const QuantLib::Currency& currency);

    bool isValidCurrency(const std::string& name) const;
    bool isMinorCurrency(const std::string& name) const;
    bool isPseudoCurrency(const std::string& name) const;
    bool isPreciousMetal(const std::string& name) const;
    bool isCryptoCurrency(const std::string& name) const;

    bool hasMinorCurrency(const std::string& name) const;
    std::string getMinorCurrency(const std::string& name) const;

    std::set<std::string> pseudoCurrencyCodes() const;

    QuantLib::Real convertMinorToMajorCurrency(const std::string& s, QuantLib::Real value);

    void reset();

private:
    void addMinorCurrencyCodes(const QuantLib::Currency& currency);

    mutable boost::shared_mutex mutex_;
    std::map<std::string, QuantLib::Currency> currencies_;
    std::map<std::string, QuantLib::Currency> minorCurrencies_;
    std::map<std::string, QuantLib::Currency> preciousMetals_;
    std::map<std::string, QuantLib::Currency> cryptoCurrencies_;
};

} // namespace data
} // namespace ore
