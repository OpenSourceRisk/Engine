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
#include <ored/utilities/xmlutils.hpp>
#include <ored/utilities/log.hpp>

#include <boost/lexical_cast.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

void SensitivityScenarioData::fromXML(XMLNode* root) {
    XMLNode* node = XMLUtils::locateNode(root, "SensitivityAnalysis");
    XMLUtils::checkNode(node, "SensitivityAnalysis");

    parConversion_ = XMLUtils::getChildValueAsBool(node, "ParConversion", true);

    LOG("Get discount curve sensitivity parameters");
    XMLNode* discountCurves = XMLUtils::getChildNode(node, "DiscountCurves");
    discountLabel_ = XMLUtils::getChildValue(discountCurves, "Label", true);
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
        }
        discountCurveShiftData_[ccy] = data;
        discountCurrencies_.push_back(ccy);
    }

    LOG("Get index curve sensitivity parameters");
    XMLNode* indexCurves = XMLUtils::getChildNode(node, "IndexCurves");
    indexLabel_ = XMLUtils::getChildValue(indexCurves, "Label", true);
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
        }
        // ... to here
        indexCurveShiftData_[index] = data;
        indexNames_.push_back(index);
    }

    LOG("Get yield curve sensitivity parameters");
    XMLNode* yieldCurves = XMLUtils::getChildNode(node, "YieldCurves");
    yieldLabel_ = XMLUtils::getChildValue(yieldCurves, "Label", true);
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
        }
        // ... to here
        yieldCurveShiftData_[curveName] = data;
        yieldCurveNames_.push_back(curveName);
    }

    LOG("Get FX spot sensitivity parameters");
    XMLNode* fxSpots = XMLUtils::getChildNode(node, "FxSpots");
    fxLabel_ = XMLUtils::getChildValue(fxSpots, "Label", true);
    for (XMLNode* child = XMLUtils::getChildNode(fxSpots, "FxSpot"); child; child = XMLUtils::getNextSibling(child)) {
        string ccypair = XMLUtils::getAttribute(child, "ccypair");
        FxShiftData data;
        data.shiftType = XMLUtils::getChildValue(child, "ShiftType", true);
        data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
        fxShiftData_[ccypair] = data;
        fxCcyPairs_.push_back(ccypair);
    }

    LOG("Get swaption vol sensitivity parameters");
    XMLNode* swaptionVols = XMLUtils::getChildNode(node, "SwaptionVolatilities");
    swaptionVolLabel_ = XMLUtils::getChildValue(swaptionVols, "Label", true);
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
    capFloorVolLabel_ = XMLUtils::getChildValue(capVols, "Label", true);
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
    fxVolLabel_ = XMLUtils::getChildValue(fxVols, "Label", true);
    for (XMLNode* child = XMLUtils::getChildNode(fxVols, "FxVolatility"); child;
         child = XMLUtils::getNextSibling(child)) {
        string ccypair = XMLUtils::getAttribute(child, "ccypair");
        FxVolShiftData data;
        data.shiftType = XMLUtils::getChildValue(child, "ShiftType");
        data.shiftSize = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
        data.shiftExpiries = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
        data.shiftStrikes = XMLUtils::getChildrenValuesAsDoubles(child, "ShiftStrikes", "Strike", true);
        fxVolShiftData_[ccypair] = data;
        fxVolCcyPairs_.push_back(ccypair);
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

std::string SensitivityScenarioData::labelSeparator() { return "/"; }

std::string SensitivityScenarioData::shiftDirectionLabel(bool up) { return up ? "UP" : "DOWN"; }

std::string SensitivityScenarioData::discountShiftScenarioLabel(string ccy, Size bucket, bool up) {
    QL_REQUIRE(discountCurveShiftData_.find(ccy) != discountCurveShiftData_.end(),
               "currency " << ccy << " not found in discount shift data");
    QL_REQUIRE(bucket < discountCurveShiftData_[ccy].shiftTenors.size(), "bucket " << bucket << " out of range");
    std::ostringstream o;
    o << discountLabel_ << labelSeparator() << ccy << labelSeparator() << bucket << labelSeparator()
      << discountCurveShiftData_[ccy].shiftTenors[bucket] << labelSeparator() << shiftDirectionLabel(up);
    return o.str();
}

std::string SensitivityScenarioData::indexShiftScenarioLabel(string indexName, Size bucket, bool up) {
    QL_REQUIRE(indexCurveShiftData_.find(indexName) != indexCurveShiftData_.end(),
               "index name " << indexName << " not found in index shift data");
    QL_REQUIRE(bucket < indexCurveShiftData_[indexName].shiftTenors.size(), "bucket " << bucket << " out of range");
    std::ostringstream o;
    o << indexLabel_ << labelSeparator() << indexName << labelSeparator() << bucket << labelSeparator()
      << indexCurveShiftData_[indexName].shiftTenors[bucket] << labelSeparator() << shiftDirectionLabel(up);
    return o.str();
}

std::string SensitivityScenarioData::yieldShiftScenarioLabel(string name, Size bucket, bool up) {
    QL_REQUIRE(yieldCurveShiftData_.find(name) != yieldCurveShiftData_.end(),
               "curve name " << name << " not found in yield shift data");
    QL_REQUIRE(bucket < yieldCurveShiftData_[name].shiftTenors.size(), "bucket " << bucket << " out of range");
    std::ostringstream o;
    o << yieldLabel_ << labelSeparator() << name << labelSeparator() << bucket << labelSeparator()
      << yieldCurveShiftData_[name].shiftTenors[bucket] << labelSeparator() << shiftDirectionLabel(up);
    return o.str();
}

std::string SensitivityScenarioData::fxShiftScenarioLabel(string ccypair, bool up) {
    std::ostringstream o;
    o << fxLabel_ << labelSeparator() << ccypair << labelSeparator() << shiftDirectionLabel(up);
    return o.str();
}

string SensitivityScenarioData::swaptionVolShiftScenarioLabel(string ccy, Size expiryBucket, Size termBucket,
                                                              Size strikeBucket, bool up) {
    QL_REQUIRE(swaptionVolShiftData_.find(ccy) != swaptionVolShiftData_.end(),
               "currency " << ccy << " not found in swaption vol shift data");
    SwaptionVolShiftData data = swaptionVolShiftData_[ccy];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    QL_REQUIRE(termBucket < data.shiftTerms.size(), "term bucket " << termBucket << " out of range");
    std::ostringstream o;
    if (data.shiftStrikes.size() == 0) {
        // ignore the strike bucket in this case
        o << swaptionVolLabel_ << labelSeparator() << ccy << labelSeparator() << expiryBucket << labelSeparator()
          << data.shiftExpiries[expiryBucket] << labelSeparator() << termBucket << labelSeparator()
          << data.shiftTerms[termBucket] << labelSeparator() << shiftDirectionLabel(up);
    } else {
        QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
        o << swaptionVolLabel_ << labelSeparator() << ccy << labelSeparator() << expiryBucket << labelSeparator()
          << data.shiftExpiries[expiryBucket] << labelSeparator() << termBucket << labelSeparator()
          << data.shiftTerms[termBucket] << labelSeparator() << strikeBucket << labelSeparator() << std::setprecision(4)
          << data.shiftStrikes[strikeBucket] << labelSeparator() << shiftDirectionLabel(up);
    }
    return o.str();
}

string SensitivityScenarioData::fxVolShiftScenarioLabel(string ccypair, Size expiryBucket, Size strikeBucket, bool up) {
    QL_REQUIRE(fxVolShiftData_.find(ccypair) != fxVolShiftData_.end(),
               "currency pair " << ccypair << " not found in fx vol shift data");
    FxVolShiftData data = fxVolShiftData_[ccypair];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    std::ostringstream o;
    if (data.shiftStrikes.size() == 0) {
        o << fxVolLabel_ << labelSeparator() << ccypair << labelSeparator() << expiryBucket << labelSeparator()
          << data.shiftExpiries[expiryBucket] << labelSeparator() << shiftDirectionLabel(up);
    } else {
        // ignore the strike bucket in this case
        QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
        o << fxVolLabel_ << labelSeparator() << ccypair << labelSeparator() << expiryBucket << labelSeparator()
          << data.shiftExpiries[expiryBucket] << labelSeparator() << std::setprecision(4)
          << data.shiftStrikes[strikeBucket] << labelSeparator() << shiftDirectionLabel(up);
    }
    return o.str();
}

string SensitivityScenarioData::capFloorVolShiftScenarioLabel(string ccy, Size expiryBucket, Size strikeBucket,
                                                              bool up) {
    QL_REQUIRE(capFloorVolShiftData_.find(ccy) != capFloorVolShiftData_.end(),
               "currency " << ccy << " not found in cap/floor vol shift data");
    CapFloorVolShiftData data = capFloorVolShiftData_[ccy];
    QL_REQUIRE(expiryBucket < data.shiftExpiries.size(), "expiry bucket " << expiryBucket << " out of range");
    QL_REQUIRE(strikeBucket < data.shiftStrikes.size(), "strike bucket " << strikeBucket << " out of range");
    std::ostringstream o;
    o << capFloorVolLabel_ << labelSeparator() << ccy << labelSeparator() << strikeBucket << labelSeparator()
      << std::setprecision(4) << data.shiftStrikes[strikeBucket] << labelSeparator() << expiryBucket << labelSeparator()
      << data.shiftExpiries[expiryBucket] << labelSeparator() << shiftDirectionLabel(up);
    return o.str();
}

string SensitivityScenarioData::labelToFactor(string label) {
    string upString = labelSeparator() + shiftDirectionLabel(true);
    string downString = labelSeparator() + shiftDirectionLabel(false);
    if (label.find(upString) != string::npos)
        return remove(label, upString);
    else if (label.find(downString) != string::npos)
        return remove(label, downString);
    else
        QL_FAIL("direction label not found");
}

string SensitivityScenarioData::remove(const string& input, const string& ending) {
    string output = input;
    std::string::size_type i = output.find(ending);
    if (i != std::string::npos)
        output.erase(i, ending.length());
    return output;
}

bool SensitivityScenarioData::isSingleShiftScenario(string label) {
    return !isCrossShiftScenario(label) && !isBaseScenario(label);
}

bool SensitivityScenarioData::isCrossShiftScenario(string label) {
    if (label.find("CROSS") != string::npos)
        return true;
    else
        return false;
}

bool SensitivityScenarioData::isBaseScenario(string label) {
    if (label.find("BASE") != string::npos)
        return true;
    else
        return false;
}

bool SensitivityScenarioData::isUpShiftScenario(string label) {
    if (label.find(shiftDirectionLabel(true)) != string::npos)
        return true;
    else
        return false;
}

bool SensitivityScenarioData::isDownShiftScenario(string label) {
    if (label.find(shiftDirectionLabel(false)) != string::npos)
        return true;
    else
        return false;
}

string SensitivityScenarioData::getCrossShiftScenarioLabel(string label, Size index) {
    QL_REQUIRE(index == 1 || index == 2, "index " << index << " out of range");
    vector<string> tokens;
    boost::split(tokens, label, boost::is_any_of(":"));
    QL_REQUIRE(tokens.size() == 3, "expected 3 tokens in cross shift scenario label " << label << ", found "
                                                                                      << tokens.size());
    return tokens[index];
}

bool SensitivityScenarioData::isYieldShiftScenario(string label) {
    if (label.find(discountLabel_) != string::npos || label.find(indexLabel_) != string::npos)
        return true;
    else
        return false;
}

bool SensitivityScenarioData::isCapFloorVolShiftScenario(string label) {
    if (label.find(capFloorVolLabel_) != string::npos)
        return true;
    else
        return false;
}

string SensitivityScenarioData::getIndexCurrency(string indexName) {
    vector<string> tokens;
    boost::split(tokens, indexName, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() > 1, "expected 2 or 3 tokens, found " << tokens.size() << " in " << indexName);
    return tokens[0];
}
}
}
