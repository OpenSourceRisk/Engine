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

#include <ored/configuration/currencyconfig.hpp>
#include <ored/utilities/currencyparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/currency.hpp>
#include <qle/currencies/configurablecurrency.hpp>
#include <string>

namespace ore {
namespace data {

CurrencyConfig::CurrencyConfig() {}

void CurrencyConfig::fromXML(XMLNode* baseNode) {
    currencies_.clear();
    XMLUtils::checkNode(baseNode, "CurrencyConfig");

    for (auto node : XMLUtils::getChildrenNodes(baseNode, "Currency")) {
        string name = XMLUtils::getChildValue(node, "Name", false);
        string isoCode = XMLUtils::getChildValue(node, "ISOCode", false);
	auto tmp = parseListOfValues(XMLUtils::getChildValue(node, "MinorUnitCodes", false));
        std::set<std::string> minorUnitCodes(tmp.begin(), tmp.end());
        try {
            DLOG("Loading external currency configuration for " << isoCode);
            Integer numericCode = parseInteger(XMLUtils::getChildValue(node, "NumericCode", false));
            string symbol = XMLUtils::getChildValue(node, "Symbol", false);
            string fractionSymbol = XMLUtils::getChildValue(node, "Symbol", false);
            Integer fractionsPerUnit = parseInteger(XMLUtils::getChildValue(node, "FractionsPerUnit", false));
            Rounding::Type roundingType = parseRoundingType(XMLUtils::getChildValue(node, "RoundingType", false));
            Integer precision = parseInteger(XMLUtils::getChildValue(node, "RoundingPrecision", false));
            // the digit where we switch form roundng down to rounding up, typically 5 and used across all
            // Integer digit = parseInteger(XMLUtils::getChildValue(node, "Digit", false));
            string format = XMLUtils::getChildValue(node, "Format", false);
            QuantExt::ConfigurableCurrency::Type currencyType = parseCurrencyType(XMLUtils::getChildValue(node, "CurrencyType", false, "Major"));
            Rounding rounding(precision, roundingType);
            QuantExt::ConfigurableCurrency c(name, isoCode, numericCode, symbol, fractionSymbol, fractionsPerUnit,
                                             rounding, format, minorUnitCodes, currencyType);
            DLOG("loading configuration for currency code " << isoCode);

            switch (currencyType) { 
                case QuantExt::ConfigurableCurrency::Type::Crypto:
                    CurrencyParser::instance().addCrypto(c.code(), c);
                    break;
                case QuantExt::ConfigurableCurrency::Type::Metal:
                    CurrencyParser::instance().addMetal(c.code(), c);
                    break;
                default:
                    CurrencyParser::instance().addCurrency(c.code(), c);
                    break;
            }
            currencies_.push_back(c);

        } catch (std::exception&) {
            ALOG("error loading currency config for name " << name << " iso code " << isoCode);
        }
    }
}

XMLNode* CurrencyConfig::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("CurrencyConfig");
    for (auto ccy : currencies_) {
        XMLNode* ccyNode = XMLUtils::addChild(doc, node, "Currency");
        XMLUtils::addChild(doc, ccyNode, "Name", ccy.name());
        XMLUtils::addChild(doc, ccyNode, "ISOCode", ccy.code());
        XMLUtils::addGenericChildAsList(
            doc, ccyNode, "MinorUnitCodes",
            std::vector<std::string>(ccy.minorUnitCodes().begin(), ccy.minorUnitCodes().end()));
        XMLUtils::addChild(doc, ccyNode, "NumericCode", to_string(ccy.numericCode()));
        XMLUtils::addChild(doc, ccyNode, "Symbol", ccy.symbol());
        XMLUtils::addChild(doc, ccyNode, "FractionSymbol", ccy.fractionSymbol());
        XMLUtils::addChild(doc, ccyNode, "RoundingType", to_string(ccy.rounding().type()));
        XMLUtils::addChild(doc, ccyNode, "RoundingPrecision", to_string(ccy.rounding().precision()));
        // XMLUtils::addChild(doc, ccyNode, "Digit", to_string(ccy.rounding().roundingDigit()));
        XMLUtils::addChild(doc, ccyNode, "Format", ccy.format());
        XMLUtils::addChild(doc, ccyNode, "CurrencyType", to_string(ccy.currencyType()));
    }
    return node;
}

} // namespace data
} // namespace ore
