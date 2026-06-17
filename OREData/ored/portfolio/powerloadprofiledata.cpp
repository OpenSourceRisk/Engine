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

void PowerLoadProfileData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "PowerLoadProfileData");

    loadProfiles_.clear();

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
            // Parse required attributes
            std::string fromStr = XMLUtils::getAttribute(lfNode, "from");
            std::string toStr = XMLUtils::getAttribute(lfNode, "to");
            int from = parseInteger(fromStr);
            int to = parseInteger(toStr);

            // Parse optional attributes with defaults
            std::string unitStr = XMLUtils::getAttribute(lfNode, "unit");
            auto unit = unitStr.empty() ? IntradayPowerTimeUnit::SECOND : parseIntradayPowerTimeUnit(unitStr);

            std::string dstStr = XMLUtils::getAttribute(lfNode, "dst");
            bool dst = dstStr.empty() ? false : parseBool(dstStr);

            QuantLib::Real loadFactor = parseReal(XMLUtils::getNodeValue(lfNode));

            // Store in map (note: unit attribute is currently not stored)
            if (dst) {
                profileDatumDSTForDate.emplace_back(from * static_cast<int>(unit), to * static_cast<int>(unit),
                                                    loadFactor);
            } else {
                profileDatumForDate.emplace_back(from * static_cast<int>(unit), to * static_cast<int>(unit),
                                                 loadFactor);
            }
        }
        loadProfiles_[date] =
            QuantLib::ext::make_shared<QuantExt::IntradayLoadProfile>(profileDatumForDate, profileDatumDSTForDate);
    }
}

XMLNode* PowerLoadProfileData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("PowerLoadProfileData");

    // Group load factors by date
    for (const auto& [date, loadProfile] : loadProfiles_) {
        XMLNode* datumNode = XMLUtils::addChild(doc, node, "LoadProfileDatum");

        // Add date in YYYY-MM-DD format
        XMLUtils::addChild(doc, datumNode, "Date", ore::data::to_string(date));

        // Add load factors container
        XMLNode* loadFactorsNode = XMLUtils::addChild(doc, datumNode, "LoadFactors");

        for (const auto& [from, to, loadFactorValue] : loadProfile->loadProfile()) {
            std::vector<std::string> attributesKeys = {"from", "to"};
            std::vector<std::string> attributesValues = {ore::data::to_string(from), ore::data::to_string(to)};
            XMLUtils::addChild(doc, loadFactorsNode, "LoadFactor", to_string(loadFactorValue), attributesKeys, attributesValues);
        }
        for (const auto& [from, to, loadFactorValue] : loadProfile->loadProfileDST()) {
            std::vector<std::string> attributesKeys = {"from", "to", "dst"};
            std::vector<std::string> attributesValues = {ore::data::to_string(from), ore::data::to_string(to), "y"};
            XMLUtils::addChild(doc, loadFactorsNode, "LoadFactor", to_string(loadFactorValue), attributesKeys, attributesValues);
        }
    }

    return node;
}

} // namespace data
} // namespace ore
