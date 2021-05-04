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

#include <ored/configuration/iborfallbackconfig.hpp>

#include <ored/utilities/to_string.hpp>

namespace ore {
namespace data {

IborFallbackConfig::IborFallbackConfig() { clear(); }
IborFallbackConfig::IborFallbackConfig(const bool useRfrCurveInTodaysMarket, const bool useRfrCurveInSimulationMarket,
                                       const bool enableIborFallbacks,
                                       const std::map<std::string, FallbackData>& fallbacks)
    : enableIborFallbacks_(enableIborFallbacks), useRfrCurveInTodaysMarket_(useRfrCurveInTodaysMarket),
      useRfrCurveInSimulationMarket_(useRfrCurveInSimulationMarket), fallbacks_(fallbacks) {}

void IborFallbackConfig::clear() {
    enableIborFallbacks_ = false;
    useRfrCurveInTodaysMarket_ = true;
    useRfrCurveInSimulationMarket_ = false;
    fallbacks_.clear();
}

bool IborFallbackConfig::useRfrCurveInTodaysMarket() const { return useRfrCurveInTodaysMarket_; }
bool IborFallbackConfig::useRfrCurveInSimulationMarket() const { return useRfrCurveInSimulationMarket_; }
bool IborFallbackConfig::enableIborFallbacks() const { return enableIborFallbacks_; }

void IborFallbackConfig::addIndexFallbackRule(const string& iborIndex, const FallbackData& fallbackData) {
    fallbacks_[iborIndex] = fallbackData;
}

bool IborFallbackConfig::isIndexReplaced(const string& iborIndex, const Date& asof) const {
    auto i = fallbacks_.find(iborIndex);
    return !(i == fallbacks_.end() || i->second.switchDate > asof);
}

const IborFallbackConfig::FallbackData& IborFallbackConfig::fallbackData(const string& iborIndex) const {
    auto i = fallbacks_.find(iborIndex);
    QL_REQUIRE(
        i != fallbacks_.end(),
        "No fallback data found for ibor index '"
            << iborIndex
            << "', client code should check whether an index is replaced with IsIndexReplaced() before querying data.");
    return i->second;
}

void IborFallbackConfig::fromXML(XMLNode* node) {
    clear();
    XMLUtils::checkNode(node, "IborFallbackConfig");
    if (auto global = XMLUtils::getChildNode(node, "GlobalSettings")) {
        enableIborFallbacks_ = XMLUtils::getChildValueAsBool(global, "EnableIborFallbacks", true);
        useRfrCurveInTodaysMarket_ = XMLUtils::getChildValueAsBool(global, "UseRfrCurveInTodaysMarket", true);
        useRfrCurveInSimulationMarket_ = XMLUtils::getChildValueAsBool(global, "UseRfrCurveInSimulationMarket", true);
    }
    if (auto fallbacks = XMLUtils::getChildNode(node, "Fallbacks")) {
        for (auto const repl : XMLUtils::getChildrenNodes(fallbacks, "Fallback")) {
            XMLUtils::checkNode(repl, "Fallback");
            string ibor = XMLUtils::getChildValue(repl, "IborIndex", true);
            fallbacks_[ibor].rfrIndex = XMLUtils::getChildValue(repl, "RfrIndex", true);
            fallbacks_[ibor].spread = parseReal(XMLUtils::getChildValue(repl, "Spread", true));
            fallbacks_[ibor].switchDate = parseDate(XMLUtils::getChildValue(repl, "SwitchDate", true));
        }
    }
}

XMLNode* IborFallbackConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("IborFallbackConfig");
    XMLNode* global = XMLUtils::addChild(doc, node, "GlobalSettings");
    XMLUtils::addChild(doc, global, "EnableIborFallbacks", enableIborFallbacks_);
    XMLUtils::addChild(doc, global, "UseRfrCurveInTodaysMarket", useRfrCurveInTodaysMarket_);
    XMLUtils::addChild(doc, global, "UseRfrCurveInSimulationMarket", useRfrCurveInSimulationMarket_);
    XMLNode* repl = XMLUtils::addChild(doc, node, "Fallbacks");
    for (auto const& r : fallbacks_) {
        XMLNode* tmp = XMLUtils::addChild(doc, repl, "Fallback");
        XMLUtils::addChild(doc, tmp, "IborIndex", r.first);
        XMLUtils::addChild(doc, tmp, "RfrIndex", r.second.rfrIndex);
        XMLUtils::addChild(doc, tmp, "Spread", r.second.spread);
        XMLUtils::addChild(doc, tmp, "SwitchDate", ore::data::to_string(r.second.switchDate));
    }
    return node;
}

IborFallbackConfig IborFallbackConfig::defaultConfig() {
    // https://assets.bbhub.io/professional/sites/10/IBOR-Fallbacks-LIBOR-Cessation_Announcement_20210305.pdf
    // https://www.isda.org/2021/03/05/isda-statement-on-uk-fca-libor-announcement/
    // https://www.fca.org.uk/publication/documents/future-cessation-loss-representativeness-libor-benchmarks.pdf
    static IborFallbackConfig c = {false,
                                   true,
                                   false,
                                   {{"CHF-LIBOR-SN", FallbackData{"CHF-SARON", -0.000551, Date(1, Jan, 2022)}},
                                    {"CHF-LIBOR-1W", FallbackData{"CHF-SARON", -0.000705, Date(1, Jan, 2022)}},
                                    {"CHF-LIBOR-1M", FallbackData{"CHF-SARON", -0.000571, Date(1, Jan, 2022)}},
                                    {"CHF-LIBOR-2M", FallbackData{"CHF-SARON", -0.000231, Date(1, Jan, 2022)}},
                                    {"CHF-LIBOR-3M", FallbackData{"CHF-SARON", 0.000031, Date(1, Jan, 2022)}},
                                    {"CHF-LIBOR-6M", FallbackData{"CHF-SARON", 0.000741, Date(1, Jan, 2022)}},
                                    {"CHF-LIBOR-12M", FallbackData{"CHF-SARON", 0.002048, Date(1, Jan, 2022)}},
                                    {"EUR-LIBOR-ON", FallbackData{"EUR-ESTER", 0.000017, Date(1, Jan, 2022)}},
                                    {"EUR-LIBOR-1W", FallbackData{"EUR-ESTER", 0.000243, Date(1, Jan, 2022)}},
                                    {"EUR-LIBOR-1M", FallbackData{"EUR-ESTER", 0.000456, Date(1, Jan, 2022)}},
                                    {"EUR-LIBOR-2M", FallbackData{"EUR-ESTER", 0.000753, Date(1, Jan, 2022)}},
                                    {"EUR-LIBOR-3M", FallbackData{"EUR-ESTER", 0.000962, Date(1, Jan, 2022)}},
                                    {"EUR-LIBOR-6M", FallbackData{"EUR-ESTER", 0.001537, Date(1, Jan, 2022)}},
                                    {"EUR-LIBOR-12M", FallbackData{"EUR-ESTER", 0.002993, Date(1, Jan, 2022)}},
                                    {"GBP-LIBOR-ON", FallbackData{"GBP-SONIA", -0.000024, Date(1, Jan, 2022)}},
                                    {"GBP-LIBOR-1W", FallbackData{"GBP-SONIA", 0.000168, Date(1, Jan, 2022)}},
                                    {"GBP-LIBOR-1M", FallbackData{"GBP-SONIA", 0.000326, Date(1, Jan, 2022)}},
                                    {"GBP-LIBOR-2M", FallbackData{"GBP-SONIA", 0.000633, Date(1, Jan, 2022)}},
                                    {"GBP-LIBOR-3M", FallbackData{"GBP-SONIA", 0.001193, Date(1, Jan, 2022)}},
                                    {"GBP-LIBOR-6M", FallbackData{"GBP-SONIA", 0.002766, Date(1, Jan, 2022)}},
                                    {"GBP-LIBOR-12M", FallbackData{"GBP-SONIA", 0.004644, Date(1, Jan, 2022)}},
                                    {"JPY-LIBOR-SN", FallbackData{"JPY-TONAR", -0.0001839, Date(1, Jan, 2022)}},
                                    {"JPY-LIBOR-1W", FallbackData{"JPY-TONAR", -0.0001981, Date(1, Jan, 2022)}},
                                    {"JPY-LIBOR-1M", FallbackData{"JPY-TONAR", -0.0002923, Date(1, Jan, 2022)}},
                                    {"JPY-LIBOR-2M", FallbackData{"JPY-TONAR", -0.0000449, Date(1, Jan, 2022)}},
                                    {"JPY-LIBOR-3M", FallbackData{"JPY-TONAR", 0.0000835, Date(1, Jan, 2022)}},
                                    {"JPY-LIBOR-6M", FallbackData{"JPY-TONAR", 0.0005809, Date(1, Jan, 2022)}},
                                    {"JPY-LIBOR-12M", FallbackData{"JPY-TONAR", 0.00166, Date(1, Jan, 2022)}},
                                    {"USD-LIBOR-ON", FallbackData{"USD-SOFR", 0.0000644, Date(1, Jul, 2023)}},
                                    {"USD-LIBOR-1W", FallbackData{"USD-SOFR", 0.0003839, Date(1, Jul, 2023)}},
                                    {"USD-LIBOR-1M", FallbackData{"USD-SOFR", 0.0011448, Date(1, Jul, 2023)}},
                                    {"USD-LIBOR-2M", FallbackData{"USD-SOFR", 0.0018456, Date(1, Jul, 2023)}},
                                    {"USD-LIBOR-3M", FallbackData{"USD-SOFR", 0.0026161, Date(1, Jul, 2023)}},
                                    {"USD-LIBOR-6M", FallbackData{"USD-SOFR", 0.0042826, Date(1, Jul, 2023)}},
                                    {"USD-LIBOR-12M", FallbackData{"USD-SOFR", 0.0071513, Date(1, Jul, 2023)}}}};
    return c;
}

} // namespace data
} // namespace ore
