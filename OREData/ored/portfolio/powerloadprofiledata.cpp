/*
    Copyright (C) 2026 AcadiaSoft Inc.
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

#include <ored/portfolio/powerloadprofiledata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <qle/utilities/intradaypower.hpp>
#include <vector>

namespace ore {
namespace data {

namespace {
// Helper struct for parsed load factor
struct ParsedLoadFactor {
    int from;
    int to;
    QuantLib::Real load;
    bool dst;
};

// Helper function to parse a LoadFactor XML node
ParsedLoadFactor parseLoadFactor(XMLNode* lfNode) {
    std::string fromStr = XMLUtils::getAttribute(lfNode, "from");
    std::string toStr = XMLUtils::getAttribute(lfNode, "to");
    int from = parseInteger(fromStr);
    int to = parseInteger(toStr);

    std::string unitStr = XMLUtils::getAttribute(lfNode, "unit");
    auto unit = unitStr.empty() ? IntradayPowerTimeUnit::SECOND : parseIntradayPowerTimeUnit(unitStr);

    std::string dstStr = XMLUtils::getAttribute(lfNode, "dst");
    bool dst = dstStr.empty() ? false : parseBool(dstStr);

    QuantLib::Real load = parseReal(XMLUtils::getNodeValue(lfNode));

    return ParsedLoadFactor{from * static_cast<int>(unit), to * static_cast<int>(unit), load, dst};
}

// Helper function to write load factors to an XML node
void writeLoadFactorsToNode(XMLDocument& doc, XMLNode* parentNode, const QuantExt::LoadFactors& loadFactors,
                            const QuantExt::LoadFactors& loadFactorsDST) {
    for (const auto& [from, to, loadValue] : loadFactors) {
        std::vector<std::string> attributesKeys = {"from", "to"};
        std::vector<std::string> attributesValues = {ore::data::to_string(from), ore::data::to_string(to)};
        XMLUtils::addChild(doc, parentNode, "LoadFactor", to_string(loadValue), attributesKeys, attributesValues);
    }
    for (const auto& [from, to, loadValue] : loadFactorsDST) {
        std::vector<std::string> attributesKeys = {"from", "to", "dst"};
        std::vector<std::string> attributesValues = {ore::data::to_string(from), ore::data::to_string(to), "y"};
        XMLUtils::addChild(doc, parentNode, "LoadFactor", to_string(loadValue), attributesKeys, attributesValues);
    }
}
} // anonymous namespace

// ExplicitData Implementation
void ExplicitData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ExplicitDates");

    std::map<QuantLib::Date, QuantLib::ext::shared_ptr<QuantExt::IntradayLoadProfile>> loadProfiles;

    // Get all LoadProfileDatum nodes
    std::vector<XMLNode*> datumNodes = XMLUtils::getChildrenNodes(node, "LoadProfileDatum");

    for (XMLNode* datumNode : datumNodes) {
        // Parse the date
        std::string dateStr = XMLUtils::getChildValue(datumNode, "Date", true);
        Date date = parseDate(dateStr);

        // Get LoadFactors container
        XMLNode* loadFactorsNode = XMLUtils::getChildNode(datumNode, "LoadFactors");
        if (!loadFactorsNode) {
            QL_FAIL("LoadFactors node not found for date " << dateStr);
        }

        std::vector<XMLNode*> loadFactorNodes = XMLUtils::getChildrenNodes(loadFactorsNode, "LoadFactor");

        QuantExt::LoadFactors profileDatumForDate;
        QuantExt::LoadFactors profileDatumDSTForDate;

        for (XMLNode* lfNode : loadFactorNodes) {
            auto parsed = parseLoadFactor(lfNode);
            if (parsed.dst) {
                profileDatumDSTForDate.emplace_back(parsed.from, parsed.to, parsed.load);
            } else {
                profileDatumForDate.emplace_back(parsed.from, parsed.to, parsed.load);
            }
        }
        loadProfiles[date] =
            QuantLib::ext::make_shared<QuantExt::IntradayLoadProfile>(profileDatumForDate, profileDatumDSTForDate);
    }
    loadTermStructure_ =
        QuantLib::ext::make_shared<QuantExt::IntradayPowerLoadTermStructureExplicit>(std::move(loadProfiles));
}

XMLNode* ExplicitData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ExplicitDates");

    // Group load factors by date
    for (const auto& [date, loadProfile] : loadTermStructure_->loadProfiles()) {
        XMLNode* datumNode = XMLUtils::addChild(doc, node, "LoadProfileDatum");

        // Add date in YYYY-MM-DD format
        XMLUtils::addChild(doc, datumNode, "Date", ore::data::to_string(date));

        // Add load factors container
        XMLNode* loadFactorsNode = XMLUtils::addChild(doc, datumNode, "LoadFactors");

        writeLoadFactorsToNode(doc, loadFactorsNode, loadProfile->loadProfile(), loadProfile->loadProfileDST());
    }

    return node;
}

// BusinessDayRuleData Implementation
void BusinessDayRuleData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "BusinessDayRules");

    std::map<QuantLib::Date, QuantLib::ext::shared_ptr<
                                 QuantExt::IntradayPowerLoadTermStructureBusinessDayRule::BusinessDayRuleLoadProfile>>
        loadProfiles;

    // Get all LoadProfileBusinessDayRule nodes
    std::vector<XMLNode*> ruleNodes = XMLUtils::getChildrenNodes(node, "LoadProfileBusinessDayRule");

    for (XMLNode* ruleNode : ruleNodes) {
        // Parse the date
        std::string dateStr = XMLUtils::getChildValue(ruleNode, "Date", true);
        Date date = parseDate(dateStr);

        // Parse calendar name
        std::string calendarName = XMLUtils::getChildValue(ruleNode, "Calendar", true);
        QuantLib::Calendar calendar = parseCalendar(calendarName);

        // Parse business day load factors
        XMLNode* bdlfNode = XMLUtils::getChildNode(ruleNode, "BusinessDayLoadFactors");
        QuantExt::LoadFactors bdProfiles;
        QuantExt::LoadFactors bdProfilesDST;

        if (bdlfNode) {
            std::vector<XMLNode*> bdlfFactorNodes = XMLUtils::getChildrenNodes(bdlfNode, "LoadFactor");
            for (XMLNode* lfNode : bdlfFactorNodes) {
                auto parsed = parseLoadFactor(lfNode);
                if (parsed.dst) {
                    bdProfilesDST.emplace_back(parsed.from, parsed.to, parsed.load);
                } else {
                    bdProfiles.emplace_back(parsed.from, parsed.to, parsed.load);
                }
            }
        }

        // Parse non-business day load factors
        XMLNode* nbdlfNode = XMLUtils::getChildNode(ruleNode, "NonBusinessDayLoadFactors");
        QuantExt::LoadFactors nbdProfiles;
        QuantExt::LoadFactors nbdProfilesDST;

        if (nbdlfNode) {
            std::vector<XMLNode*> nbdlfFactorNodes = XMLUtils::getChildrenNodes(nbdlfNode, "LoadFactor");
            for (XMLNode* lfNode : nbdlfFactorNodes) {
                auto parsed = parseLoadFactor(lfNode);
                if (parsed.dst) {
                    nbdProfilesDST.emplace_back(parsed.from, parsed.to, parsed.load);
                } else {
                    nbdProfiles.emplace_back(parsed.from, parsed.to, parsed.load);
                }
            }
        }

        auto bdProfile = QuantLib::ext::make_shared<
            QuantExt::IntradayPowerLoadTermStructureBusinessDayRule::BusinessDayRuleLoadProfile>();
        bdProfile->calendar = calendar;
        bdProfile->businessDayProfile =
            QuantLib::ext::make_shared<QuantExt::IntradayLoadProfile>(bdProfiles, bdProfilesDST);
        bdProfile->nonBusinessDayProfile =
            QuantLib::ext::make_shared<QuantExt::IntradayLoadProfile>(nbdProfiles, nbdProfilesDST);

        loadProfiles[date] = bdProfile;
    }
    loadTermStructure_ =
        QuantLib::ext::make_shared<QuantExt::IntradayPowerLoadTermStructureBusinessDayRule>(std::move(loadProfiles));
}

XMLNode* BusinessDayRuleData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("BusinessDayRules");

    for (const auto& [date, bdProfile] : loadTermStructure_->loadProfiles()) {
        XMLNode* ruleNode = XMLUtils::addChild(doc, node, "LoadProfileBusinessDayRule");

        // Add date
        XMLUtils::addChild(doc, ruleNode, "Date", ore::data::to_string(date));

        // Add calendar name
        // TODO: Extract calendar name properly
        XMLUtils::addChild(doc, ruleNode, "Calendar", ""); // Placeholder

        // Add business day load factors
        XMLNode* bdlfNode = XMLUtils::addChild(doc, ruleNode, "BusinessDayLoadFactors");
        writeLoadFactorsToNode(doc, bdlfNode, bdProfile->businessDayProfile->loadProfile(),
                               bdProfile->businessDayProfile->loadProfileDST());

        // Add non-business day load factors
        XMLNode* nbdlfNode = XMLUtils::addChild(doc, ruleNode, "NonBusinessDayLoadFactors");
        writeLoadFactorsToNode(doc, nbdlfNode, bdProfile->nonBusinessDayProfile->loadProfile(),
                               bdProfile->nonBusinessDayProfile->loadProfileDST());
    }

    return node;
}

// PowerLoadProfileData Implementation
void PowerLoadProfileData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "PowerLoadProfileData");

    // Check which child node exists
    XMLNode* explicitNode = XMLUtils::getChildNode(node, "ExplicitDates");
    XMLNode* businessDayNode = XMLUtils::getChildNode(node, "BusinessDayRules");

    if (explicitNode && businessDayNode) {
        QL_FAIL("PowerLoadProfileData cannot have both ExplicitDates and BusinessDayRules nodes");
    }

    if (!explicitNode && !businessDayNode) {
        QL_FAIL("PowerLoadProfileData must have either ExplicitDates or BusinessDayRules node");
    }

    if (explicitNode) {
        concreteData_ = QuantLib::ext::make_shared<ExplicitData>();
        concreteData_->fromXML(explicitNode);
    } else if (businessDayNode) {
        concreteData_ = QuantLib::ext::make_shared<BusinessDayRuleData>();
        concreteData_->fromXML(businessDayNode);
    }
}

XMLNode* PowerLoadProfileData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("PowerLoadProfileData");

    if (concreteData_) {
        XMLNode* childNode = concreteData_->toXML(doc);
        XMLUtils::appendNode(node, childNode);
    }

    return node;
}

QuantLib::ext::shared_ptr<QuantExt::IntradayPowerLoadTermStructure> PowerLoadProfileData::loadTermStructure() const {
    if (concreteData_) {
        return concreteData_->loadTermStructure();
    }
    return nullptr;
}

} // namespace data
} // namespace ore
