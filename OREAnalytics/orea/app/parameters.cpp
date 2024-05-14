/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <orea/app/parameters.hpp>

#include <ored/utilities/log.hpp>

#include <ql/errors.hpp>

using QuantLib::Date;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

bool Parameters::hasGroup(const string& groupName) const { return (data_.find(groupName) != data_.end()); }

bool Parameters::has(const string& groupName, const string& paramName) const {
    QL_REQUIRE(hasGroup(groupName), "param group '" << groupName << "' not found");
    auto it = data_.find(groupName);
    return (it->second.find(paramName) != it->second.end());
}

string Parameters::get(const string& groupName, const string& paramName, bool fail) const {
    if (fail) {    
        QL_REQUIRE(has(groupName, paramName), "parameter " << paramName << " not found in param group " << groupName);
        auto it = data_.find(groupName);
        return it->second.find(paramName)->second;
    } else {
        if (!hasGroup(groupName) || !has(groupName,paramName))
            return "";
        else {
            auto it = data_.find(groupName);
            return it->second.find(paramName)->second;
        }
    }
}

const map<string, string>& Parameters::data(const string& groupName) const {
    auto it = data_.find(groupName);
    QL_REQUIRE(it != data_.end(), "param group '" << groupName << "' not found");
    return it->second;
}
    
const map<string, string>& Parameters::markets() const {
    return data("markets");
}

void Parameters::fromFile(const string& fileName) {
    LOG("load ORE configuration from " << fileName);
    clear();
    XMLDocument doc(fileName);
    fromXML(doc.getFirstNode("ORE"));
    LOG("load ORE configuration from " << fileName << " done.");
}

void Parameters::clear() { data_.clear(); }

void Parameters::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ORE");

    XMLNode* setupNode = XMLUtils::getChildNode(node, "Setup");
    QL_REQUIRE(setupNode, "node Setup not found in parameter file");
    map<string, string> setupMap;
    for (XMLNode* child = XMLUtils::getChildNode(setupNode); child; child = XMLUtils::getNextSibling(child)) {
        string key = XMLUtils::getAttribute(child, "name");
        string value = XMLUtils::getNodeValue(child);
        setupMap[key] = value;
    }
    data_["setup"] = setupMap;

    XMLNode* loggingNode = XMLUtils::getChildNode(node, "Logging");
    if (loggingNode) {
        map<string, string> loggingMap;
        for (XMLNode* child = XMLUtils::getChildNode(loggingNode); child; child = XMLUtils::getNextSibling(child)) {
            string key = XMLUtils::getAttribute(child, "name");
            string value = XMLUtils::getNodeValue(child);
            loggingMap[key] = value;
        }
        data_["logging"] = loggingMap;
    }

    XMLNode* marketsNode = XMLUtils::getChildNode(node, "Markets");
    if (marketsNode) {
        map<string, string> marketsMap;
        for (XMLNode* child = XMLUtils::getChildNode(marketsNode); child; child = XMLUtils::getNextSibling(child)) {
            string key = XMLUtils::getAttribute(child, "name");
            string value = XMLUtils::getNodeValue(child);
            marketsMap[key] = value;
        }
        data_["markets"] = marketsMap;
    }

    XMLNode* analyticsNode = XMLUtils::getChildNode(node, "Analytics");
    if (analyticsNode) {
        for (XMLNode* child = XMLUtils::getChildNode(analyticsNode); child; child = XMLUtils::getNextSibling(child)) {
            string groupName = XMLUtils::getAttribute(child, "type");
            map<string, string> analyticsMap;
            for (XMLNode* paramNode = XMLUtils::getChildNode(child); paramNode;
                 paramNode = XMLUtils::getNextSibling(paramNode)) {
                string key = XMLUtils::getAttribute(paramNode, "name");
                string value = XMLUtils::getNodeValue(paramNode);
                analyticsMap[key] = value;
            }
            data_[groupName] = analyticsMap;
        }
    }
}

XMLNode* Parameters::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ORE");
    QL_FAIL("Parameters::toXML not implemented yet");
    return node;
}

void Parameters::log() {
    LOG("Parameters:");
    for (auto p : data_)
        for (auto pp : p.second)
            LOG("group = " << p.first << " : " << pp.first << " = " << pp.second);
}
} // namespace analytics
} // namespace ore
