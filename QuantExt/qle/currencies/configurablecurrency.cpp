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

#include <qle/currencies/configurablecurrency.hpp>

using namespace QuantLib;

namespace QuantExt {
using namespace QuantLib;
  
ConfigurableCurrency::ConfigurableCurrency(const std::string& name, const std::string& code, Integer numericCode,
                                           const std::string& symbol, const std::string& fractionSymbol,
                                           Integer fractionsPerUnit, const Rounding& rounding,
                                           const std::string& formatString, const std::set<std::string>& minorUnitCodes,
                                           ConfigurableCurrency::Type currencyType)
    : Currency(name, code, numericCode, symbol, fractionSymbol, fractionsPerUnit, rounding, formatString, Currency(),
               minorUnitCodes),
      currencyType_(currencyType) {
    data_ = QuantLib::ext::make_shared<Currency::Data>(name, code, numericCode, symbol, fractionSymbol, fractionsPerUnit,
                                               rounding, formatString, Currency(), minorUnitCodes);
}

std::ostream& operator<<(std::ostream& os, ConfigurableCurrency::Type ccytype) {
    switch (ccytype) {
		case ConfigurableCurrency::Type::Major:
			return os << "Major";
        case ConfigurableCurrency::Type::Metal:
            return os <<"Metal";
        case ConfigurableCurrency::Type::Crypto:
            return os << "Crypto";
        default:
            QL_FAIL("Unknown AssetClass");
    }
}

} // namespace QuantExt
