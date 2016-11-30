/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <orea/scenario/sensitivityscenariodata.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ored/utilities/log.hpp>

#include <boost/lexical_cast.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

void SensitivityScenarioData::fromXML(XMLNode* root) {
    XMLNode* node = XMLUtils::locateNode(root, "SensitivityAnalysis");
    XMLUtils::checkNode(node, "SensitivityAnalysis");

    LOG("Get discount curve sensitivity parameters");
    XMLNode* child = XMLUtils::getChildNode(node, "DiscountCurve");
    discountCurrencies_ = XMLUtils::getChildrenValues(child, "Currencies", "Currency", false);
    discountLabel_ = XMLUtils::getChildValue(child, "Label", true);
    discountDomain_ = XMLUtils::getChildValue(child, "Domain", true);
    discountShiftType_ = XMLUtils::getChildValue(child, "ShiftType", true);
    discountShiftSize_ = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
    discountShiftTenors_ = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);

    LOG("Get index curve sensitivity parameters");
    child = XMLUtils::getChildNode(node, "IndexCurve");
    indexNames_ = XMLUtils::getChildrenValues(child, "IndexNames", "Index", false);
    indexLabel_ = XMLUtils::getChildValue(child, "Label", true);
    indexDomain_ = XMLUtils::getChildValue(child, "Domain", true);
    indexShiftType_ = XMLUtils::getChildValue(child, "ShiftType", true);
    indexShiftSize_ = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
    indexShiftTenors_ = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);

    LOG("Get FX spot sensitivity parameters");
    child = XMLUtils::getChildNode(node, "FxSpot");
    fxCcyPairs_ = XMLUtils::getChildrenValues(child, "CurrencyPairs", "CurrencyPair", false);
    fxLabel_ = XMLUtils::getChildValue(child, "Label", true);
    fxShiftType_ = XMLUtils::getChildValue(child, "ShiftType", true);
    fxShiftSize_ = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);

    LOG("Get swaption vol sensitivity parameters");
    child = XMLUtils::getChildNode(node, "SwaptionVolatility");
    swaptionVolCurrencies_ = XMLUtils::getChildrenValues(child, "Currencies", "Currency", false);
    swaptionVolLabel_ = XMLUtils::getChildValue(child, "Label", true);
    swaptionVolShiftType_ = XMLUtils::getChildValue(child, "ShiftType", true);
    swaptionVolShiftSize_ = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
    swaptionVolShiftTerms_ = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTerms", true);
    swaptionVolShiftExpiries_ = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
    swaptionVolShiftStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(child, "ShiftStrikes", true);

    LOG("Get cap/floor vol sensitivity parameters");
    child = XMLUtils::getChildNode(node, "CapFloorVolatility");
    capFloorVolCurrencies_ = XMLUtils::getChildrenValues(child, "Currencies", "Currency", false);
    capFloorVolLabel_ = XMLUtils::getChildValue(child, "Label", true);
    capFloorVolShiftType_ = XMLUtils::getChildValue(child, "ShiftType", true);
    capFloorVolShiftSize_ = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
    capFloorVolShiftExpiries_ = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
    capFloorVolShiftStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(child, "ShiftStrikes", true);

    LOG("Get fx vol sensitivity parameters");
    child = XMLUtils::getChildNode(node, "FxVolatility");
    fxVolCcyPairs_ = XMLUtils::getChildrenValues(child, "CurrencyPairs", "CurrencyPair", false);
    fxVolLabel_ = XMLUtils::getChildValue(child, "Label", true);
    fxVolShiftType_ = XMLUtils::getChildValue(child, "ShiftType");
    fxVolShiftSize_ = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
    fxVolShiftExpiries_ = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
    fxVolShiftStrikes_ = XMLUtils::getChildrenValuesAsDoubles(child, "ShiftStrikes", "Strike", true);

    // LOG("Get inflation curve sensitivity parameters");
    // child = XMLUtils::getChildNode(node, "InflationCurve");
    // infIndices_ = XMLUtils::getChildrenValues(child, "IndexNames", "Index", false);
    // infLabel_ = XMLUtils::getChildValue(child, "Label");
    // infDomain_ = XMLUtils::getChildValue(child, "Domain");
    // infShiftType_ = XMLUtils::getChildValue(child, "ShiftType");
    // infShiftSize_ = XMLUtils::getChildValueAsDouble(child, "ShiftSize");
    // infShiftTenors_ = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors");

    // LOG("Get credit curve sensitivity parameters");
    // child = XMLUtils::getChildNode(node, "CreditCurves");
    // crNames_ = XMLUtils::getChildrenValues(child, "CreditNames", "Name", false);
    // crLabel_ = XMLUtils::getChildValue(child, "Label");
    // crDomain_ = XMLUtils::getChildValue(child, "Domain");
    // crShiftType_ = XMLUtils::getChildValue(child, "ShiftType");
    // crShiftSize_ = XMLUtils::getChildValueAsDouble(child, "ShiftSize");
    // crShiftTenors_ = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors");  

    LOG("Get cross gamma parameters");
    vector<string> filter = XMLUtils::getChildrenValues(node, "CrossGammaFilter", "Pair", true);
    for (Size i = 0; i < filter.size(); ++i) {
      vector<string> tokens;
      boost::split(tokens, filter[i], boost::is_any_of(","));
      QL_REQUIRE(tokens.size() == 2, "expected 2 tokens, found " << tokens.size() << " in " << filter[i]);
      crossGammaFilter_.push_back(pair<string,string>(tokens[0],tokens[1]));
      crossGammaFilter_.push_back(pair<string,string>(tokens[1],tokens[0]));
    }
}

XMLNode* SensitivityScenarioData::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("SensitivityAnalysis");

    return node;
}
}
}
