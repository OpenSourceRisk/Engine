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

#include <orea/scenario/sensitivityscenariodata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/xmlutils.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

void SensitivityScenarioData::fromXML(XMLNode* root) {
    XMLNode* node = XMLUtils::locateNode(root, "SensitivityAnalysis");
    XMLUtils::checkNode(node, "SensitivityAnalysis");

    XMLNode* parNode = XMLUtils::getChildNode(node, "ParConversion");
    parConversion_ = false;
    if (parNode)
        parConversion_ = XMLUtils::getChildValueAsBool(node, "ParConversion");

    LOG("Get discount curve sensitivity parameters");
    XMLNode* discountCurves = XMLUtils::getChildNode(node, "DiscountCurves");
    for (XMLNode* child = XMLUtils::getChildNode(discountCurves, "DiscountCurve"); child;
         child = XMLUtils::getNextSibling(child)) {
        string ccy = XMLUtils::getAttribute(child, "ccy");
        LOG("Discount curve for ccy " << ccy);
        CurveShiftData data;
        data.shiftType = XMLUtils::getChildValue(child, "ShiftType", true);
        data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
        data.shiftTenors = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
        XMLNode* par = XMLUtils::getChildNode(child, "ParConversion");
        if (par) {
            data.parInstruments = XMLUtils::getChildrenValuesAsStrings(par, "Instruments", true);
            data.parInstrumentSingleCurve = XMLUtils::getChildValueAsBool(par, "SingleCurve", true);
            XMLNode* conventionsNode = XMLUtils::getChildNode(par, "Conventions");
            data.parInstrumentConventions =
                XMLUtils::getChildrenAttributesAndValues(conventionsNode, "Convention", "id", true);
        } else if (parConversion_) {
            QL_FAIL("par conversion data not provided for discount curve " << ccy);
        }
        discountCurveShiftData_[ccy] = data;
        discountCurrencies_.push_back(ccy);
    }

    LOG("Get index curve sensitivity parameters");
    XMLNode* indexCurves = XMLUtils::getChildNode(node, "IndexCurves");
    for (XMLNode* child = XMLUtils::getChildNode(indexCurves, "IndexCurve"); child;
         child = XMLUtils::getNextSibling(child)) {
        string index = XMLUtils::getAttribute(child, "index");
        // same as discount curve sensitivity loading from here ...
        CurveShiftData data;
        data.shiftType = XMLUtils::getChildValue(child, "ShiftType", true);
        data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
        data.shiftTenors = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
        XMLNode* par = XMLUtils::getChildNode(child, "ParConversion");
        if (par) {
            data.parInstruments = XMLUtils::getChildrenValuesAsStrings(par, "Instruments", true);
            data.parInstrumentSingleCurve = XMLUtils::getChildValueAsBool(par, "SingleCurve", true);
            XMLNode* conventionsNode = XMLUtils::getChildNode(par, "Conventions");
            data.parInstrumentConventions =
                XMLUtils::getChildrenAttributesAndValues(conventionsNode, "Convention", "id", true);
        } else if (parConversion_) {
            QL_FAIL("par conversion data not provided for index curve " << index);
        }
        // ... to here
        indexCurveShiftData_[index] = data;
        indexNames_.push_back(index);
    }

    LOG("Get yield curve sensitivity parameters");
    XMLNode* yieldCurves = XMLUtils::getChildNode(node, "YieldCurves");
    for (XMLNode* child = XMLUtils::getChildNode(yieldCurves, "YieldCurve"); child;
         child = XMLUtils::getNextSibling(child)) {
        string curveName = XMLUtils::getAttribute(child, "name");
        // same as discount curve sensitivity loading from here ...
        CurveShiftData data;
        data.shiftType = XMLUtils::getChildValue(child, "ShiftType", true);
        data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
        data.shiftTenors = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
        XMLNode* par = XMLUtils::getChildNode(child, "ParConversion");
        if (par) {
            data.parInstruments = XMLUtils::getChildrenValuesAsStrings(par, "Instruments", true);
            data.parInstrumentSingleCurve = XMLUtils::getChildValueAsBool(par, "SingleCurve", true);
            XMLNode* conventionsNode = XMLUtils::getChildNode(par, "Conventions");
            data.parInstrumentConventions =
                XMLUtils::getChildrenAttributesAndValues(conventionsNode, "Convention", "id", true);
        } else if (parConversion_) {
            QL_FAIL("par conversion data not provided for yield curve " << curveName);
        }
        // ... to here
        string curveType =  XMLUtils::getChildValue(child, "CurveType", false);
        if( curveType == "EquityForecast") {
            equityForecastCurveShiftData_[curveName] = data;
            equityForecastCurveNames_.push_back(curveName);
        } else {
            yieldCurveShiftData_[curveName] = data;
            yieldCurveNames_.push_back(curveName);
        }
    }

    LOG("Get dividend yield curve sensitivity parameters");
    XMLNode* dividendYieldCurves = XMLUtils::getChildNode(node, "DividendYieldCurves");
    if (dividendYieldCurves) {
        for (XMLNode* child = XMLUtils::getChildNode(dividendYieldCurves, "DividendYieldCurve"); child;
            child = XMLUtils::getNextSibling(child)) {
            string curveName = XMLUtils::getAttribute(child, "equity");
            LOG("Add dividend yield curve data for equity " << curveName);
            // same as discount curve sensitivity loading from here ...
            CurveShiftData data;
            data.shiftType = XMLUtils::getChildValue(child, "ShiftType", true);
            data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
            data.shiftTenors = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
            dividendYieldShiftData_[curveName] = data;
            dividendYieldNames_.push_back(curveName);
        }
    }

    LOG("Get FX spot sensitivity parameters");
    XMLNode* fxSpots = XMLUtils::getChildNode(node, "FxSpots");
    for (XMLNode* child = XMLUtils::getChildNode(fxSpots, "FxSpot"); child; child = XMLUtils::getNextSibling(child)) {
        string ccypair = XMLUtils::getAttribute(child, "ccypair");
        SpotShiftData data;
        data.shiftType = XMLUtils::getChildValue(child, "ShiftType", true);
        data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
        fxShiftData_[ccypair] = data;
        fxCcyPairs_.push_back(ccypair);
    }

    LOG("Get swaption vol sensitivity parameters");
    XMLNode* swaptionVols = XMLUtils::getChildNode(node, "SwaptionVolatilities");
    for (XMLNode* child = XMLUtils::getChildNode(swaptionVols, "SwaptionVolatility"); child;
         child = XMLUtils::getNextSibling(child)) {
        string ccy = XMLUtils::getAttribute(child, "ccy");
        SwaptionVolShiftData data;
        data.shiftType = XMLUtils::getChildValue(child, "ShiftType", true);
        data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
        data.shiftTerms = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTerms", true);
        data.shiftExpiries = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
        data.shiftStrikes = XMLUtils::getChildrenValuesAsDoublesCompact(child, "ShiftStrikes", true);
        swaptionVolShiftData_[ccy] = data;
        swaptionVolCurrencies_.push_back(ccy);
    }

    LOG("Get cap/floor vol sensitivity parameters");
    XMLNode* capVols = XMLUtils::getChildNode(node, "CapFloorVolatilities");
    for (XMLNode* child = XMLUtils::getChildNode(capVols, "CapFloorVolatility"); child;
         child = XMLUtils::getNextSibling(child)) {
        string ccy = XMLUtils::getAttribute(child, "ccy");
        CapFloorVolShiftData data;
        data.shiftType = XMLUtils::getChildValue(child, "ShiftType", true);
        data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
        data.shiftExpiries = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
        data.shiftStrikes = XMLUtils::getChildrenValuesAsDoublesCompact(child, "ShiftStrikes", true);
        data.indexName = XMLUtils::getChildValue(child, "Index", true);
        capFloorVolShiftData_[ccy] = data;
        capFloorVolCurrencies_.push_back(ccy);
    }

    LOG("Get fx vol sensitivity parameters");
    XMLNode* fxVols = XMLUtils::getChildNode(node, "FxVolatilities");
    for (XMLNode* child = XMLUtils::getChildNode(fxVols, "FxVolatility"); child;
         child = XMLUtils::getNextSibling(child)) {
        string ccypair = XMLUtils::getAttribute(child, "ccypair");
        VolShiftData data;
        data.shiftType = XMLUtils::getChildValue(child, "ShiftType");
        data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
        data.shiftExpiries = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
        data.shiftStrikes = XMLUtils::getChildrenValuesAsDoubles(child, "ShiftStrikes", "Strike", true);
        fxVolShiftData_[ccypair] = data;
        fxVolCcyPairs_.push_back(ccypair);
    }

    LOG("Get credit curve sensitivity parameters");
    XMLNode* creditCurves = XMLUtils::getChildNode(node, "CreditCurves");
    if (creditCurves) {
    for (XMLNode* child = XMLUtils::getChildNode(creditCurves, "CreditCurve"); child;
         child = XMLUtils::getNextSibling(child)) {
        string name = XMLUtils::getAttribute(child, "name");
        string ccy = XMLUtils::getChildValue(child, "Currency", true);
        creditCcys_[name] = ccy;
        // same as discount curve sensitivity loading from here ...
        CurveShiftData data;
        data.shiftType = XMLUtils::getChildValue(child, "ShiftType", true);
        data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
        data.shiftTenors = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
        XMLNode* par = XMLUtils::getChildNode(child, "ParConversion");
        if (par) {
            data.parInstruments = XMLUtils::getChildrenValuesAsStrings(par, "Instruments", true);
            data.parInstrumentSingleCurve = XMLUtils::getChildValueAsBool(par, "SingleCurve", true);
            XMLNode* conventionsNode = XMLUtils::getChildNode(par, "Conventions");
            data.parInstrumentConventions =
                XMLUtils::getChildrenAttributesAndValues(conventionsNode, "Convention", "id", true);
        } else if (parConversion_) {
            QL_FAIL("par conversion data not provided for credit curve " << name);
        }
        // ... to here
        creditCurveShiftData_[name] = data;
        creditNames_.push_back(name);
    }
    }


    LOG("Get cds vol sensitivity parameters");
    XMLNode* cdsVols = XMLUtils::getChildNode(node, "CDSVolatilities");
    if (cdsVols) {
        for (XMLNode* child = XMLUtils::getChildNode(cdsVols, "CDSVolatility"); child;
            child = XMLUtils::getNextSibling(child)) {
            string name = XMLUtils::getAttribute(child, "name");
            CdsVolShiftData data;
            data.shiftType = XMLUtils::getChildValue(child, "ShiftType", true);
            data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
            data.shiftExpiries = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
            cdsVolShiftData_[name] = data;
            cdsVolNames_.push_back(name);
        }
    }

    LOG("Get Base Correlation sensitivity parameters");
    XMLNode* bcNode = XMLUtils::getChildNode(node, "BaseCorrelations");
    if (bcNode) {
        for (XMLNode* child = XMLUtils::getChildNode(bcNode, "BaseCorrelation"); child;
            child = XMLUtils::getNextSibling(child)) {
            string name = XMLUtils::getAttribute(child, "indexName");
            BaseCorrelationShiftData data;
            data.shiftType = XMLUtils::getChildValue(child, "ShiftType");
            data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
            data.shiftTerms = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTerms", true);
            data.shiftLossLevels = XMLUtils::getChildrenValuesAsDoublesCompact(child, "ShiftLossLevels", true);
            baseCorrelationShiftData_[name] = data;
            baseCorrelationNames_.push_back(name);
        }
    }

    LOG("Get Equity spot sensitivity parameters");
    XMLNode* equitySpots = XMLUtils::getChildNode(node, "EquitySpots");
    if (equitySpots) {
        for (XMLNode* child = XMLUtils::getChildNode(equitySpots, "EquitySpot"); child;
            child = XMLUtils::getNextSibling(child)) {
            string equity = XMLUtils::getAttribute(child, "equity");
            SpotShiftData data;
            data.shiftType = XMLUtils::getChildValue(child, "ShiftType", true);
            data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
            equityShiftData_[equity] = data;
            equityNames_.push_back(equity);
        }
    }

    LOG("Get Equity vol sensitivity parameters");
    XMLNode* equityVols = XMLUtils::getChildNode(node, "EquityVolatilities");
    if (equityVols) {
        for (XMLNode* child = XMLUtils::getChildNode(equityVols, "EquityVolatility"); child;
            child = XMLUtils::getNextSibling(child)) {
            string equity = XMLUtils::getAttribute(child, "equity");
            VolShiftData data;
            data.shiftType = XMLUtils::getChildValue(child, "ShiftType");
            data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
            data.shiftExpiries = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
            data.shiftStrikes = XMLUtils::getChildrenValuesAsDoubles(child, "ShiftStrikes", "Strike", true);
            equityVolShiftData_[equity] = data;
            equityVolNames_.push_back(equity);
        }
    }


    LOG("Get Zero Inflation sensitivity parameters");
    XMLNode* zeroInflation = XMLUtils::getChildNode(node, "ZeroInflationIndexCurves");
    if (zeroInflation) {
        for (XMLNode* child = XMLUtils::getChildNode(zeroInflation, "ZeroInflationIndexCurve"); child;
            child = XMLUtils::getNextSibling(child)) {
            string index = XMLUtils::getAttribute(child, "index");
            CurveShiftData data;
            data.shiftType = XMLUtils::getChildValue(child, "ShiftType");
            data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
            data.shiftTenors = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
            XMLNode* par = XMLUtils::getChildNode(child, "ParConversion");
            if (par) {
                data.parInstruments = XMLUtils::getChildrenValuesAsStrings(par, "Instruments", true);
                data.parInstrumentSingleCurve = XMLUtils::getChildValueAsBool(par, "SingleCurve", true);
                XMLNode* conventionsNode = XMLUtils::getChildNode(par, "Conventions");
                data.parInstrumentConventions =
                    XMLUtils::getChildrenAttributesAndValues(conventionsNode, "Convention", "id", true);
            }
            else if (parConversion_) {
                QL_FAIL("par conversion data not provided for zero inflation curve " << index);
            }
            zeroInflationCurveShiftData_[index] = data;
            zeroInflationIndices_.push_back(index);
        }
    }

    LOG("Get Yoy Inflation sensitivity parameters");
    XMLNode* yoyInflation = XMLUtils::getChildNode(node, "YYInflationIndexCurves");
    if (yoyInflation) {
        for (XMLNode* child = XMLUtils::getChildNode(yoyInflation, "YYInflationIndexCurve"); child;
            child = XMLUtils::getNextSibling(child)) {
            string index = XMLUtils::getAttribute(child, "index");
            CurveShiftData data;
            data.shiftType = XMLUtils::getChildValue(child, "ShiftType");
            data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
            data.shiftTenors = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
            XMLNode* par = XMLUtils::getChildNode(child, "ParConversion");
            if (par) {
                data.parInstruments = XMLUtils::getChildrenValuesAsStrings(par, "Instruments", true);
                data.parInstrumentSingleCurve = XMLUtils::getChildValueAsBool(par, "SingleCurve", true);
                XMLNode* conventionsNode = XMLUtils::getChildNode(par, "Conventions");
                data.parInstrumentConventions =
                    XMLUtils::getChildrenAttributesAndValues(conventionsNode, "Convention", "id", true);
            }
            else if (parConversion_) {
                QL_FAIL("par conversion data not provided for yoy inflation curve " << index);
            }
            yoyInflationCurveShiftData_[index] = data;
            yoyInflationIndices_.push_back(index);
        }
    }

    LOG("Get cross gamma parameters");
    vector<string> filter = XMLUtils::getChildrenValues(node, "CrossGammaFilter", "Pair", true);
    for (Size i = 0; i < filter.size(); ++i) {
        vector<string> tokens;
        boost::split(tokens, filter[i], boost::is_any_of(","));
        QL_REQUIRE(tokens.size() == 2, "expected 2 tokens, found " << tokens.size() << " in " << filter[i]);
        crossGammaFilter_.push_back(pair<string, string>(tokens[0], tokens[1]));
        crossGammaFilter_.push_back(pair<string, string>(tokens[1], tokens[0]));
    }
}

XMLNode* SensitivityScenarioData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("SensitivityAnalysis");
    // TODO
    return node;
}

string SensitivityScenarioData::getIndexCurrency(string indexName) {
    vector<string> tokens;
    boost::split(tokens, indexName, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() > 1, "expected 2 or 3 tokens, found " << tokens.size() << " in " << indexName);
    return tokens[0];
}
} // namespace analytics
} // namespace ore
