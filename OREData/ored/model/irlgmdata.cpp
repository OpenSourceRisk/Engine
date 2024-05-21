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

#include <ored/model/irlgmdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/indexparser.hpp>

namespace ore {
namespace data {

void IrLgmData::fromXML(XMLNode* node) {
    qualifier_ = XMLUtils::getAttribute(node, "key");
    if(qualifier_.empty()) {
	std::string ccy = XMLUtils::getAttribute(node, "ccy");
	if(!ccy.empty()) {
	    qualifier_ = ccy;
	    WLOG("IrLgmData: attribute ccy is deprecated, use key instead.");
	}
    }
    LOG("LGM with attribute (key) = " << qualifier_);

    // Calibration Swaptions

    if (XMLNode* optionsNode = XMLUtils::getChildNode(node, "CalibrationSwaptions")) {
        optionExpiries() = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Expiries", false);
        optionTerms() = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Terms", false);
        QL_REQUIRE(optionExpiries().size() == optionTerms().size(),
                   "vector size mismatch in swaption expiries/terms for ccy " << qualifier_);
        optionStrikes() = XMLUtils::getChildrenValuesAsStrings(optionsNode, "Strikes", false);
        if (optionStrikes().size() > 0) {
            QL_REQUIRE(optionStrikes().size() == optionExpiries().size(),
                       "vector size mismatch in swaption expiries/strikes for ccy " << qualifier_);
        } else // Default: ATM
            optionStrikes().resize(optionExpiries().size(), "ATM");

        for (Size i = 0; i < optionExpiries().size(); i++) {
            LOG("LGM calibration swaption " << optionExpiries()[i] << " x " << optionTerms()[i] << " "
                                            << optionStrikes()[i]);
        }
    }
    LgmData::fromXML(node);
}

XMLNode* IrLgmData::toXML(XMLDocument& doc) const {
    XMLNode* node = LgmData::toXML(doc);
    XMLUtils::addAttribute(doc, node, "key", qualifier_);

    // swaption calibration
    XMLNode* calibrationSwaptionsNode = XMLUtils::addChild(doc, node, "CalibrationSwaptions");
    XMLUtils::addGenericChildAsList(doc, calibrationSwaptionsNode, "Expiries", optionExpiries());
    XMLUtils::addGenericChildAsList(doc, calibrationSwaptionsNode, "Terms", optionTerms());
    XMLUtils::addGenericChildAsList(doc, calibrationSwaptionsNode, "Strikes", optionStrikes());

    return node;
}

} // namespace data
} // namespace ore
