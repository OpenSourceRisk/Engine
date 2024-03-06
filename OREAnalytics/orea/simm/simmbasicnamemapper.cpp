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

#include <orea/simm/simmbasicnamemapper.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;

using ore::data::XMLDocument;
using ore::data::XMLNode;
using ore::data::XMLUtils;
using std::map;
using std::string;

namespace ore {
namespace analytics {

string SimmBasicNameMapper::qualifier(const string& externalName) const {
    // Returns mapping if there is one found else returns the externalName
    auto m = mapping_.find(externalName);
    if (m != mapping_.end()) {
        Date vt = validToDate(externalName);
        Date vf = validFromDate(externalName);
        Date today = Settings::instance().evaluationDate();
        if (vt >= today && vf <= today)
            return m->second;
        else {
            ALOG("Name mapping for external name " << externalName << " is expired");
            return externalName;
        }
    } else {
        return externalName;
    }
}

string SimmBasicNameMapper::validTo(const string& externalName) const {
    // Returns mapping expiry date string
    auto v = validTo_.find(externalName);
    if (v  != validTo_.end())
        return v->second;
    else
        return {};
}

string SimmBasicNameMapper::validFrom(const string& externalName) const {
    // Returns mapping start date string
    auto v = validFrom_.find(externalName);
    if (v != validFrom_.end())
        return v->second;
    else
        return {};
}

Date SimmBasicNameMapper::validToDate(const string& externalName) const {
    // Returns mapping expiry date
    string valid = validTo_.at(externalName);
    if (!valid.empty())
        return ore::data::parseDate(valid);
    else
        return Date::maxDate();
}

Date SimmBasicNameMapper::validFromDate(const string& externalName) const {
    // Returns mapping start date
    string valid = validFrom_.at(externalName);
    if (!valid.empty())
        return ore::data::parseDate(valid);
    else
        return Date::minDate();
}

bool SimmBasicNameMapper::hasQualifier(const string& externalName) const { return mapping_.count(externalName) > 0; }

bool SimmBasicNameMapper::hasValidQualifier(const std::string& externalName,
                                            const QuantLib::Date& referenceDate) const {
    return hasQualifier(externalName) && validFromDate(externalName) <= referenceDate &&
           referenceDate <= validToDate(externalName);
}

std::string SimmBasicNameMapper::externalName(const std::string& qualifier) const {
    for (auto it = mapping_.begin(); it != mapping_.end(); ++it)
        if (it->second == qualifier)
            return it->first;
    return qualifier;
}

void SimmBasicNameMapper::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "SIMMNameMappings");

    // Every time a call to fromXML is made, the map is cleared
    mapping_.clear();

    LOG("Start parsing SIMMNameMappings");

    for (XMLNode* mappingNode = XMLUtils::getChildNode(node, "Mapping"); mappingNode;
         mappingNode = XMLUtils::getNextSibling(mappingNode, "Mapping")) {

        string validTo = XMLUtils::getChildValue(mappingNode, "ValidTo", false);
        string validFrom = XMLUtils::getChildValue(mappingNode, "ValidFrom", false);
        string name = XMLUtils::getChildValue(mappingNode, "Name", false);
        string qualifier = XMLUtils::getChildValue(mappingNode, "Qualifier", false);
        if (name == "" || qualifier == "") {
            ALOG("skip name mapping for name '" << name << "' and qualifier '" << qualifier << "'"); 
            continue;
        }
        
        if (validTo != "") {
            // Check whether the provided validTo attribute is a valid date, ALERT and ignore if it is not
            try {
                ore::data::parseDate(validTo);
             } catch(std::exception&) {
                ALOG("Cannot parse name mapping validTo " << validTo << " for qualifier " << qualifier << ", ignore");
                validTo = "";
            }
        }  

        if (validFrom != "") {
            // Check whether the provided validTo attribute is a valid date, ALERT and ignore if it is not
            try {
                ore::data::parseDate(validFrom);
             } catch(std::exception&) {
                ALOG("Cannot parse name mapping validFrom " << validFrom << " for qualifier " << qualifier << ", ignore");
                validFrom = "";
            }
        }  

        mapping_[name] = qualifier;
        validTo_[name] = validTo;
        validFrom_[name] = validFrom;
    }

    LOG("Finished parsing SIMMNameMappings");
}

XMLNode* SimmBasicNameMapper::toXML(ore::data::XMLDocument& doc) const {

    LOG("Start toXML for SIMM name mappings");

    XMLNode* node = doc.allocNode("SIMMNameMappings");
    for (const auto& kv : mapping_) {
        XMLNode* mappingNode = doc.allocNode("Mapping");
        string vt = validTo(kv.first);
        if (vt != "")
            XMLUtils::addChild(doc, mappingNode, "ValidTo", vt);
        string vf = validFrom(kv.first);
        if (vf != "")
            XMLUtils::addChild(doc, mappingNode, "ValidFrom", vf);
        XMLUtils::addChild(doc, mappingNode, "Name", kv.first);
        XMLUtils::addChild(doc, mappingNode, "Qualifier", kv.second);
        XMLUtils::appendNode(node, mappingNode);
    }

    LOG("Finished toXML for SIMM name mappings");

    return node;
}

void SimmBasicNameMapper::addMapping(const string& externalName, const string& qualifier, const std::string& validFrom,
                                     const std::string& validTo) {
    // Just overwrite if name already exists
    mapping_[externalName] = qualifier;
    validTo_[externalName] = validTo;
    validFrom_[externalName] = validFrom;
    if (validTo != "") {
        try {
            ore::data::parseDate(validTo);
        } catch (std::exception&) {
            ALOG("Cannot parse name mapping validTo " << validTo << " for qualifier " << qualifier << ", ignore");
            validTo_[externalName] = "";
        }
    }
    if (validFrom != "") {
        try {
            ore::data::parseDate(validFrom);
        } catch (std::exception&) {
            ALOG("Cannot parse name mapping validTo " << validFrom << " for qualifier " << qualifier << ", ignore");
            validFrom_[externalName] = "";
        }
    }
}

} // namespace analytics
} // namespace ore
