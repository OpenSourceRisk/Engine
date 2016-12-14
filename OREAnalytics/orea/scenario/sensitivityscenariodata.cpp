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
    XMLNode* par = XMLUtils::getChildNode(child, "ParConversion");
    if (par) {
        discountParInstruments_ = XMLUtils::getChildrenValuesAsStrings(par, "Instruments", true);
        discountParInstrumentsSingleCurve_ = XMLUtils::getChildValueAsBool(par, "SingleCurve", true);
        XMLNode* conventionsNode = XMLUtils::getChildNode(par, "Conventions");
        discountParInstrumentConventions_ =
            XMLUtils::getChildrenAttributePairsAndValues(conventionsNode, "Convention", "id", true);
    }

    LOG("Get index curve sensitivity parameters");
    child = XMLUtils::getChildNode(node, "IndexCurve");
    indexNames_ = XMLUtils::getChildrenValues(child, "IndexNames", "Index", false);
    indexLabel_ = XMLUtils::getChildValue(child, "Label", true);
    indexDomain_ = XMLUtils::getChildValue(child, "Domain", true);
    indexShiftType_ = XMLUtils::getChildValue(child, "ShiftType", true);
    indexShiftSize_ = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
    indexShiftTenors_ = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftTenors", true);
    par = XMLUtils::getChildNode(child, "ParConversion");
    if (par) {
        indexParInstruments_ = XMLUtils::getChildrenValuesAsStrings(par, "Instruments", true);
        indexParInstrumentsSingleCurve_ = XMLUtils::getChildValueAsBool(par, "SingleCurve", true);
        XMLNode* conventionsNode = XMLUtils::getChildNode(par, "Conventions");
        indexParInstrumentConventions_ =
            XMLUtils::getChildrenAttributePairsAndValues(conventionsNode, "Convention", "id", true);
    }

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
    capFloorVolCurrencies_ = XMLUtils::getChildrenValues(child, "Currencies", "Currency", true);
    capFloorVolLabel_ = XMLUtils::getChildValue(child, "Label", true);
    capFloorVolShiftType_ = XMLUtils::getChildValue(child, "ShiftType", true);
    capFloorVolShiftSize_ = XMLUtils::getChildValueAsDouble(child, "ShiftSize", true);
    capFloorVolShiftExpiries_ = XMLUtils::getChildrenValuesAsPeriods(child, "ShiftExpiries", true);
    capFloorVolShiftStrikes_ = XMLUtils::getChildrenValuesAsDoublesCompact(child, "ShiftStrikes", true);
    par = XMLUtils::getChildNode(child, "ParConversion");
    if (par) {
        XMLNode* indexMappingNode = XMLUtils::getChildNode(par, "IndexMapping");
        capFloorVolIndexMapping_ = XMLUtils::getChildrenAttributesAndValues(indexMappingNode, "Index", "ccy", true);
    }

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
        crossGammaFilter_.push_back(pair<string, string>(tokens[0], tokens[1]));
        crossGammaFilter_.push_back(pair<string, string>(tokens[1], tokens[0]));
    }
}

XMLNode* SensitivityScenarioData::toXML(XMLDocument& doc) {

    XMLNode* node = doc.allocNode("SensitivityAnalysis");

    return node;
}

std::string SensitivityScenarioData::labelSeparator() { return "/"; }

std::string SensitivityScenarioData::shiftDirectionLabel(bool up) { return up ? "UP" : "DOWN"; }

std::string SensitivityScenarioData::discountShiftScenarioLabel(string ccy, Size bucket, bool up) {
    std::ostringstream o;
    QL_REQUIRE(bucket < discountShiftTenors_.size(), "bucket " << bucket << " out of range");
    o << discountLabel_ << labelSeparator() << ccy << labelSeparator() << bucket << labelSeparator()
      << discountShiftTenors_[bucket] << labelSeparator() << shiftDirectionLabel(up);
    return o.str();
}

std::string SensitivityScenarioData::indexShiftScenarioLabel(string indexName, Size bucket, bool up) {
    QL_REQUIRE(bucket < indexShiftTenors_.size(), "bucket " << bucket << " out of range");
    std::ostringstream o;
    o << indexLabel_ << labelSeparator() << indexName << labelSeparator() << bucket << labelSeparator()
      << indexShiftTenors_[bucket] << labelSeparator() << shiftDirectionLabel(up);
    return o.str();
}

std::string SensitivityScenarioData::fxShiftScenarioLabel(string ccypair, bool up) {
    std::ostringstream o;
    o << fxLabel_ << labelSeparator() << ccypair << labelSeparator() << shiftDirectionLabel(up);
    return o.str();
}

string SensitivityScenarioData::swaptionVolShiftScenarioLabel(string ccy, Size expiryBucket, Size termBucket,
                                                              Size strikeBucket, bool up) {
    QL_REQUIRE(expiryBucket < swaptionVolShiftExpiries_.size(), "expiry bucket " << expiryBucket << " out of range");
    QL_REQUIRE(termBucket < swaptionVolShiftTerms_.size(), "term bucket " << termBucket << " out of range");
    std::ostringstream o;
    if (swaptionVolShiftStrikes_.size() == 0) {
        // ignore the strike bucket in this case
        o << swaptionVolLabel_ << labelSeparator() << ccy << labelSeparator() << expiryBucket << labelSeparator()
          << swaptionVolShiftExpiries_[expiryBucket] << labelSeparator() << termBucket << labelSeparator()
          << swaptionVolShiftTerms_[termBucket] << labelSeparator() << shiftDirectionLabel(up);
    } else {
        QL_REQUIRE(strikeBucket < swaptionVolShiftStrikes_.size(), "strike bucket " << strikeBucket << " out of range");
        o << swaptionVolLabel_ << labelSeparator() << ccy << labelSeparator() << expiryBucket << labelSeparator()
          << swaptionVolShiftExpiries_[expiryBucket] << labelSeparator() << termBucket << labelSeparator()
          << swaptionVolShiftTerms_[termBucket] << labelSeparator() << strikeBucket << labelSeparator()
          << std::setprecision(4) << swaptionVolShiftStrikes_[strikeBucket] << labelSeparator()
          << shiftDirectionLabel(up);
    }
    return o.str();
}

string SensitivityScenarioData::fxVolShiftScenarioLabel(string ccypair, Size expiryBucket, Size strikeBucket, bool up) {
    QL_REQUIRE(expiryBucket < fxVolShiftExpiries_.size(), "expiry bucket " << expiryBucket << " out of range");
    std::ostringstream o;
    if (fxVolShiftStrikes_.size() == 0) {
        o << fxVolLabel_ << labelSeparator() << ccypair << labelSeparator() << expiryBucket << labelSeparator()
          << fxVolShiftExpiries_[expiryBucket] << labelSeparator() << shiftDirectionLabel(up);
    } else {
        // ignore the strike bucket in this case
        QL_REQUIRE(strikeBucket < fxVolShiftStrikes_.size(), "strike bucket " << strikeBucket << " out of range");
        o << fxVolLabel_ << labelSeparator() << ccypair << labelSeparator() << expiryBucket << labelSeparator()
          << fxVolShiftExpiries_[expiryBucket] << labelSeparator() << std::setprecision(4)
          << fxVolShiftStrikes_[strikeBucket] << labelSeparator() << shiftDirectionLabel(up);
    }
    return o.str();
}

string SensitivityScenarioData::capFloorVolShiftScenarioLabel(string ccy, Size expiryBucket, Size strikeBucket,
                                                              bool up) {
    QL_REQUIRE(expiryBucket < capFloorVolShiftExpiries_.size(), "expiry bucket " << expiryBucket << " out of range");
    QL_REQUIRE(strikeBucket < capFloorVolShiftStrikes_.size(), "strike bucket " << strikeBucket << " out of range");
    std::ostringstream o;
    o << capFloorVolLabel_ << labelSeparator() << ccy << labelSeparator() << strikeBucket << labelSeparator()
      << std::setprecision(4) << capFloorVolShiftStrikes_[strikeBucket] << labelSeparator() << expiryBucket
      << labelSeparator() << capFloorVolShiftExpiries_[expiryBucket] << labelSeparator() << shiftDirectionLabel(up);
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
