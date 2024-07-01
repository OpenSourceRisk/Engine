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

#include <ored/model/crcirdata.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace data {

bool CrCirData::operator==(const CrCirData& rhs) {

    if (name_ != rhs.name_ || currency_ != rhs.currency_ ||
        calibrationType_ != rhs.calibrationType_ || calibrationStrategy_ != rhs.calibrationStrategy_ ||
        startValue_ != rhs.startValue_ ||
        reversionValue_ != rhs.reversionValue_ || longTermValue_ != rhs.longTermValue_ ||
        volatility_ != rhs.volatility_ || relaxedFeller_ != rhs.relaxedFeller_ ||
        fellerFactor_ != rhs.fellerFactor_ || tolerance_ != rhs.tolerance_ ||
        optionExpiries_ != rhs.optionExpiries_ || optionTerms_ != rhs.optionTerms_ ||
        optionStrikes_ != rhs.optionStrikes_) {
        return false;
    }
    return true;
}

bool CrCirData::operator!=(const CrCirData& rhs) { return !(*this == rhs); }

CrCirData::CalibrationStrategy parseCirCalibrationStrategy(const string& s) {
    if (s == "None")
        return CrCirData::CalibrationStrategy::None;
    else if (s == "CurveAndFlatVol")
        return CrCirData::CalibrationStrategy::CurveAndFlatVol;
    else {
        QL_FAIL("CrCirData::CalibrationStrategy " << s << " not recognised.");
    }
}

std::ostream& operator<<(std::ostream& oss, const CrCirData::CalibrationStrategy& s) {
    if (s == CrCirData::CalibrationStrategy::None)
        oss << "None";
    else if (s == CrCirData::CalibrationStrategy::CurveAndFlatVol)
        oss << "CurveAndFlatVol";
    else
        QL_FAIL("CIR Calibration strategy(" << ((int)s) << ") not covered");
    return oss;
}

void CrCirData::fromXML(XMLNode* node) {

    name_ = XMLUtils::getAttribute(node, "name");
    LOG("CIR with attribute (name) = " << name_);

    currency_ = XMLUtils::getChildValue(node, "Currency", true);
    LOG("CIR currency = " << currency_);

    std::string calibTypeString = XMLUtils::getChildValue(node, "CalibrationType", true);
    calibrationType_ = parseCalibrationType(calibTypeString);
    LOG("CIR calibration type = " << calibTypeString);

    std::string calibStratString = XMLUtils::getChildValue(node, "CalibrationStrategy", true);
    calibrationStrategy_ = parseCirCalibrationStrategy(calibStratString);
    LOG("CIR calibration strategy = " << calibStratString);

    startValue_ = XMLUtils::getChildValueAsDouble(node, "StartValue", true);
    LOG("CIR start value_ = " << startValue_);

    reversionValue_ = XMLUtils::getChildValueAsDouble(node, "ReversionValue", true);
    LOG("CIR reversion value = " << reversionValue_);

    longTermValue_ = XMLUtils::getChildValueAsDouble(node, "LongTermValue", true);
    LOG("CIR long term value = " << longTermValue_);

    volatility_ = XMLUtils::getChildValueAsDouble(node, "Volatility", true);
    LOG("CIR volatility = " << volatility_);

    relaxedFeller_ = XMLUtils::getChildValueAsBool(node, "RelaxedFeller", true);
    LOG("CIR relaxed feller = " << relaxedFeller_);

    fellerFactor_ = XMLUtils::getChildValueAsDouble(node, "FellerFactor", true);
    LOG("CIR feller factor = " << fellerFactor_);

    tolerance_ = XMLUtils::getChildValueAsDouble(node, "Tolerance", true);
    LOG("CIR tolerance = " << tolerance_);

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
            LOG("CrCir calibration cds option " << optionExpiries()[i] << " x " << optionTerms()[i] << " "
                                                << optionStrikes()[i]);
        }
    }

    LOG("CrCirData done");
}

XMLNode* CrCirData::toXML(XMLDocument& doc) const {

    XMLNode* cirNode = doc.allocNode("CIR");
    XMLUtils::addAttribute(doc, cirNode, "name", name_);

    XMLUtils::addChild(doc, cirNode, "Currency", currency_);

    XMLUtils::addGenericChild(doc, cirNode, "CalibrationType", calibrationType_);
    XMLUtils::addGenericChild(doc, cirNode, "CalibrationStrategy", calibrationStrategy_);

    XMLUtils::addChild(doc, cirNode, "StartValue", startValue_);
    XMLUtils::addChild(doc, cirNode, "ReversionValue", reversionValue_);
    XMLUtils::addChild(doc, cirNode, "LongTermValue", longTermValue_);
    XMLUtils::addChild(doc, cirNode, "Volatility", volatility_);

    XMLUtils::addChild(doc, cirNode, "RelaxedFeller", relaxedFeller_);
    XMLUtils::addChild(doc, cirNode, "FellerFactor", fellerFactor_);
    XMLUtils::addChild(doc, cirNode, "Tolerance", tolerance_);

    // swaption calibration
    XMLNode* calibrationSwaptionsNode = XMLUtils::addChild(doc, cirNode, "CalibrationCdsOptions");
    XMLUtils::addGenericChildAsList(doc, calibrationSwaptionsNode, "Expiries", optionExpiries_);
    XMLUtils::addGenericChildAsList(doc, calibrationSwaptionsNode, "Terms", optionTerms_);
    XMLUtils::addGenericChildAsList(doc, calibrationSwaptionsNode, "Strikes", optionStrikes_);

    return cirNode;
}

} // namespace data
} // namespace ore
