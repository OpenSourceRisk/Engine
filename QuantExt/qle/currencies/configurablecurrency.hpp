/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/currencies/configurablecurrency.hpp
    \brief Configurable currency
    \ingroup currencies
*/

#ifndef quantext_currencies_configurable_hpp
#define quantext_currencies_configurable_hpp

#include <ql/currency.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Configurable currency class
/*!
  \ingroup currencies
 */
class ConfigurableCurrency : public Currency {
public:
    enum class Type { Major, Metal, Crypto };
    ConfigurableCurrency(const std::string& name, const std::string& code, Integer numericCode,
                         const std::string& symbol, const std::string& fractionSymbol, Integer fractionsPerUnit,
                         const Rounding& rounding, const std::string& formatString, 
                         const std::set<std::string>& minorUnitCodes, Type currencyType = Type::Major);
    Type currencyType() const { return currencyType_; }

private:
    ConfigurableCurrency::Type currencyType_;
};

std::ostream& operator<<(std::ostream& os, QuantExt::ConfigurableCurrency::Type ccytype);

} // namespace QuantExt
#endif
