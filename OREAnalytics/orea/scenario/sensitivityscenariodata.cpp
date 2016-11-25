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

    //discountCurrencies_ = XMLUtils::getChildrenValues(node, "DiscountCurrencies", "Currency", true);
    discountLabel_ = XMLUtils::getChildValue(node, "DiscountLabel", true);
    discountDomain_ = XMLUtils::getChildValue(node, "DiscountDomain", true);
    discountShiftType_ = XMLUtils::getChildValue(node, "DiscountShiftType", true);
    discountShiftSize_ = XMLUtils::getChildValueAsDouble(node, "DiscountShiftSize", true);
    discountShiftTenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "DiscountShiftTenors", true);

    //indexNames_ = XMLUtils::getChildrenValues(node, "IndexNames", "Index", true);
    indexLabel_ = XMLUtils::getChildValue(node, "IndexLabel", true);
    indexDomain_ = XMLUtils::getChildValue(node, "IndexDomain", true);
    indexShiftType_ = XMLUtils::getChildValue(node, "IndexShiftType", true);
    indexShiftSize_ = XMLUtils::getChildValueAsDouble(node, "IndexShiftSize", true);
    indexShiftTenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "IndexShiftTenors", true);

    //fxCurrencyPairs_ = XMLUtils::getChildrenValues(node, "FxCurrencyPairs", "CurrencyPair", true);
    fxLabel_ = XMLUtils::getChildValue(node, "FxLabel", true);
    fxShiftType_ = XMLUtils::getChildValue(node, "FxShiftType", true);
    fxShiftSize_ = XMLUtils::getChildValueAsDouble(node, "FxShiftSize", true);

    //infIndices_ = XMLUtils::getChildrenValues(node, "InflationIndices", "Index", true);
    infLabel_ = XMLUtils::getChildValue(node, "InflationLabel");
    infDomain_ = XMLUtils::getChildValue(node, "InflationDomain");
    infShiftType_ = XMLUtils::getChildValue(node, "InflationShiftType");
    infShiftSize_ = XMLUtils::getChildValueAsDouble(node, "InflationShiftSize");
    infShiftTenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "InflationShiftTenors");

    //swaptionVolCurrencies_ = XMLUtils::getChildrenValues(node, "SwaptionVolatilityCurrencies", "Currency", true);
    swaptionVolLabel_ = XMLUtils::getChildValue(node, "SwaptionVolatilityLabel", true);
    swaptionVolShiftType_ = XMLUtils::getChildValue(node, "SwaptionVolatilityShiftType", true);
    swaptionVolShiftSize_ = XMLUtils::getChildValueAsDouble(node, "SwaptionVolatilityShiftSize", true);
    swaptionVolShiftTerms_ = XMLUtils::getChildrenValuesAsPeriods(node, "SwaptionVolatilityShiftTerms", true);
    swaptionVolShiftExpiries_ = XMLUtils::getChildrenValuesAsPeriods(node, "SwaptionVolatilityShiftExpiries", true);
    swaptionVolShiftStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(node, "SwaptionVolatilityShiftStrikes", true);

    //capFloorVolCurrencies_ = XMLUtils::getChildrenValues(node, "CapFloorVolatilityCurrencies", "Currency", true);
    capFloorVolLabel_ = XMLUtils::getChildValue(node, "CapFloorVolatilityLabel", true);
    capFloorVolShiftType_ = XMLUtils::getChildValue(node, "CapFloorVolatilityShiftType", true);
    capFloorVolShiftSize_ = XMLUtils::getChildValueAsDouble(node, "CapFloorVolatilityShiftSize", true);
    capFloorVolShiftExpiries_ = XMLUtils::getChildrenValuesAsPeriods(node, "CapFloorVolatilityShiftExpiries", true);
    capFloorVolShiftStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(node, "CapFloorVolatilityShiftStrikes", true);

    //swaptionVolCurrencies_ = XMLUtils::getChildrenValues(node, "SwaptionVolatilityCurrencies", "Currency", true);
    fxVolLabel_ = XMLUtils::getChildValue(node, "FxVolatilityLabel", true);
    fxVolShiftType_ = XMLUtils::getChildValue(node, "FxVolatilityShiftType");
    fxVolShiftSize_ = XMLUtils::getChildValueAsDouble(node, "FxVolatilityShiftSize", true);
    fxVolShiftExpiries_ = XMLUtils::getChildrenValuesAsPeriods(node, "FxVolatilityShiftExpiries", true);
    fxVolShiftStrikes_ = XMLUtils::getChildrenValuesAsDoubles(node, "FxVolatilityShiftStrikes", "Strike", true);

    //crNames_ = XMLUtils::getChildrenValues(node, "CreditNames", "Name", true);
    crLabel_ = XMLUtils::getChildValue(node, "CreditLabel");
    crDomain_ = XMLUtils::getChildValue(node, "CreditDomain");
    crShiftType_ = XMLUtils::getChildValue(node, "CreditShiftType");
    crShiftSize_ = XMLUtils::getChildValueAsDouble(node, "CreditShiftSize");
    crShiftTenors_ = XMLUtils::getChildrenValuesAsPeriods(node, "CreditShiftTenors");  
}

XMLNode* SensitivityScenarioData::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("SensitivityAnalysis");

    return node;
}
}
}
