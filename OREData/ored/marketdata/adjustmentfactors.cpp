/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/marketdata/adjustmentfactors.hpp>

namespace ore {
namespace data {

bool AdjustmentFactors::hasFactor(const string& name) const { return data_.find(name) != data_.end(); }

Real AdjustmentFactors::getFactor(const string& name, const QuantLib::Date& d) const {
    Real baseFactor = 1.0;
    // If no adjustments return a factor of 1
    if (!hasFactor(name))
        return baseFactor;

    // for the given name loop through all the adjustments
    // adjustments are applied backwards to a time series, 
    // if the date is before asof date:
    // we multiply by the factor from any future adjustments but before the asof date,
    // this ensures all data is on the same scale at the asof date
    // if date is after asof date:
    // we divide by the factor from any historical adjustments between asof and date
    for (auto f : data_.at(name)) {
        if (d < f.first && asof_ > f.first) {
            baseFactor = baseFactor * f.second;
        }
        if (asof_ < f.first && f.first <= d) {
            baseFactor = baseFactor / f.second;
        }
    }
    return baseFactor;
}

void AdjustmentFactors::addFactor(string name, QuantLib::Date d, Real factor) {
    data_[name].push_back(std::pair<Date, Real>(d, factor));
}

void AdjustmentFactors::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "AdditionalData");

    XMLNode* childNode = XMLUtils::locateNode(node, "AdjustmentFactors");
    for (XMLNode* child = XMLUtils::getChildNode(childNode); child; child = XMLUtils::getNextSibling(child)) {

        Date date = parseDate(XMLUtils::getChildValue(child, "Date", true));
        std::string quote = XMLUtils::getChildValue(child, "Quote", true);
        Real factor = XMLUtils::getChildValueAsDouble(child, "Factor", true);

        addFactor(quote, date, factor);
    }
}

XMLNode* AdjustmentFactors::toXML(ore::data::XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("AdjustmentFactors");
    for (auto d : data_) {
        for (auto f : d.second) {
            XMLNode* factorNode = XMLUtils::addChild(doc, node, "AdjustmentFactor");
            XMLUtils::addChild(doc, factorNode, "Date", ore::data::to_string(f.first));
            XMLUtils::addChild(doc, factorNode, "Quote", d.first);
            XMLUtils::addChild(doc, factorNode, "Factor", f.second);
        }
    }
    return node;
}

std::set<std::string> AdjustmentFactors::names() const {
    std::set<std::string> result;
    for (auto const& m : data_)
        result.insert(m.first);
    return result;
}

std::set<QuantLib::Date> AdjustmentFactors::dates(const std::string& name) const {
    std::set<QuantLib::Date> result;
    auto d = data_.find(name);
    if (d != data_.end()) {
        for (auto const& m : d->second)
            result.insert(m.first);
    }
    return result;
}

QuantLib::Real AdjustmentFactors::getFactorContribution(const std::string& name, const QuantLib::Date& d) const {
    auto adj = data_.find(name);
    if (adj != data_.end()) {
        auto it = std::find_if(adj->second.begin(), adj->second.end(),
                               [&d](const std::pair<QuantLib::Date, QuantLib::Real>& m) { return m.first == d; });
        if (it != adj->second.end())
            return it->second;
    }
    return 1.0;
}

} // namespace data
} // namespace ore
