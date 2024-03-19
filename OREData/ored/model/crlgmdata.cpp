/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/model/crlgmdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

void CrLgmData::fromXML(XMLNode* node) {
    name_ = XMLUtils::getAttribute(node, "name");
    LOG("LGM with attribute (name) = " << name_);

    // Calibration Swaptions

    XMLNode* optionsNode = XMLUtils::getChildNode(node, "CalibrationCdsOptions");

    if (optionsNode) {

        optionExpiries() = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Expiries", false);
        optionTerms() = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Terms", false);
        QL_REQUIRE(optionExpiries().size() == optionTerms().size(),
                "vector size mismatch in cds option expiries/terms for name " << name_);
        optionStrikes() = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Strikes", false);
        if (optionStrikes().size() > 0) {
            QL_REQUIRE(optionStrikes().size() == optionExpiries().size(),
                    "vector size mismatch in cds option expiries/strikes for name " << name_);
        } else // Default: ATM
            optionStrikes().resize(optionExpiries().size(), "ATM");

        for (Size i = 0; i < optionExpiries().size(); i++) {
            LOG("LGM calibration cds option " << optionExpiries()[i] << " x " << optionTerms()[i] << " "
                                            << optionStrikes()[i]);
        }
    }

    LgmData::fromXML(node);
}

XMLNode* CrLgmData::toXML(XMLDocument& doc) const {
    XMLNode* node = LgmData::toXML(doc);
    XMLUtils::addAttribute(doc, node, "name", name_);

    // swaption calibration
    XMLNode* calibrationSwaptionsNode = XMLUtils::addChild(doc, node, "CalibrationCdsOptions");
    XMLUtils::addGenericChildAsList(doc, calibrationSwaptionsNode, "Expiries", optionExpiries());
    XMLUtils::addGenericChildAsList(doc, calibrationSwaptionsNode, "Terms", optionTerms());
    XMLUtils::addGenericChildAsList(doc, calibrationSwaptionsNode, "Strikes", optionStrikes());

    return node;
}
} // namespace data
} // namespace ore
