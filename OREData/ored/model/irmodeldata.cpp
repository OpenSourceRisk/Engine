/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ored/model/irmodeldata.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/lexical_cast.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/indexparser.hpp>

namespace ore {
namespace data {

std::ostream& operator<<(std::ostream& oss, const ParamType& type) {
    if (type == ParamType::Constant)
        oss << "CONSTANT";
    else if (type == ParamType::Piecewise)
        oss << "PIECEWISE";
    else
        QL_FAIL("Parameter type not covered by <<");
    return oss;
}

ParamType parseParamType(const string& s) {
    if (boost::algorithm::to_upper_copy(s) == "CONSTANT")
        return ParamType::Constant;
    else if (boost::algorithm::to_upper_copy(s) == "PIECEWISE")
        return ParamType::Piecewise;
    else
        QL_FAIL("Parameter type " << s << " not recognized");
}

CalibrationType parseCalibrationType(const string& s) {
    if (boost::algorithm::to_upper_copy(s) == "BOOTSTRAP")
        return CalibrationType::Bootstrap;
    else if (boost::algorithm::to_upper_copy(s) == "BESTFIT")
        return CalibrationType::BestFit;
    else if (boost::algorithm::to_upper_copy(s) == "NONE")
        return CalibrationType::None;
    else
        QL_FAIL("Calibration type " << s << " not recognized");
}

std::ostream& operator<<(std::ostream& oss, const CalibrationType& type) {
    if (type == CalibrationType::Bootstrap)
        oss << "BOOTSTRAP";
    else if (type == CalibrationType::BestFit)
        oss << "BESTFIT";
    else if (type == CalibrationType::None)
        oss << "NONE";
    else
        QL_FAIL("Calibration type not covered");
    return oss;
}

CalibrationStrategy parseCalibrationStrategy(const string& s) {
    if (boost::algorithm::to_upper_copy(s) == "COTERMINALATM")
        return CalibrationStrategy::CoterminalATM;
    else if (boost::algorithm::to_upper_copy(s) == "COTERMINALDEALSTRIKE")
        return CalibrationStrategy::CoterminalDealStrike;
    else if (boost::algorithm::to_upper_copy(s) == "UNDERLYINGATM")
        return CalibrationStrategy::UnderlyingATM;
    else if (boost::algorithm::to_upper_copy(s) == "UNDERLYINGDEALSTRIKE")
        return CalibrationStrategy::UnderlyingDealStrike;
    else if (boost::algorithm::to_upper_copy(s) == "NONE")
        return CalibrationStrategy::None;
    else
        QL_FAIL("Calibration strategy " << s << " not recognized");
}

std::ostream& operator<<(std::ostream& oss, const CalibrationStrategy& type) {
    if (type == CalibrationStrategy::CoterminalATM)
        oss << "COTERMINALATM";
    else if (type == CalibrationStrategy::CoterminalDealStrike)
        oss << "COTERMINALDEALSTRIKE";
    else if (type == CalibrationStrategy::UnderlyingATM)
        oss << "UNDERLYINGATM";
    else if (type == CalibrationStrategy::UnderlyingDealStrike)
        oss << "UNDERLYINGDEALSTRIKE";
    else if (type == CalibrationStrategy::None)
        oss << "NONE";
    else
        QL_FAIL("Calibration strategy not covered");
    return oss;
}

void IrModelData::clear() {}

void IrModelData::reset() {
    clear();
    qualifier_ = "";
    calibrationType_ = CalibrationType::Bootstrap;
}

void IrModelData::fromXML(XMLNode* node) {
    std::string calibTypeString = XMLUtils::getChildValue(node, "CalibrationType", true);
    
    calibrationType_ = parseCalibrationType(calibTypeString);
    LOG(name_ + " with calibrationType_ = " << qualifier_);
}

XMLNode* IrModelData::toXML(XMLDocument& doc) const {

    XMLNode* irModelNode = doc.allocNode(name_);

    XMLUtils::addGenericChild(doc, irModelNode, "CalibrationType", calibrationType_);
    return irModelNode;
}

std::string IrModelData::ccy() const{
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> index;
    return tryParseIborIndex(qualifier_, index) ? index->currency().code() : qualifier_;
}

} // namespace data
} // namespace ore
