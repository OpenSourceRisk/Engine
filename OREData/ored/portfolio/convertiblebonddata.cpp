/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/portfolio/convertiblebonddata.hpp>
#include <ored/portfolio/convertiblebondreferencedata.hpp>

#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

using namespace ore::data;

namespace {
template <typename T> std::string toCommaSeparatedList(const std::vector<T>& d) {
    std::ostringstream o;
    for (Size i = 0; i < d.size(); ++i)
        o << d[i] << (i < d.size() - 1 ? "," : "");
    return o.str();
}
} // namespace

void ConvertibleBondData::CallabilityData::MakeWholeData::ConversionRatioIncreaseData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ConversionRatioIncrease");
    cap_ = XMLUtils::getChildValue(node, "Cap", false);
    stockPrices_ = parseListOfValues<double>(XMLUtils::getChildValue(node, "StockPrices"), &parseReal);
    std::vector<std::string> tmp = XMLUtils::getChildrenValuesWithAttributes(node, "CrIncreases", "CrIncrease",
                                                                             "startDate", crIncreaseDates_, true);
    for (auto const& t : tmp) {
        crIncrease_.push_back(parseListOfValues<double>(t, &parseReal));
    }
    initialised_ = true;
}

XMLNode* ConvertibleBondData::CallabilityData::MakeWholeData::ConversionRatioIncreaseData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ConversionRatioIncrease");
    if (!cap_.empty()) {
        XMLUtils::addChild(doc, node, "Cap", cap_);
    }
    XMLUtils::addChild(doc, node, "StockPrices", toCommaSeparatedList(stockPrices_));
    std::vector<std::string> tmp;
    for (auto const& d : crIncrease_) {
        tmp.push_back(toCommaSeparatedList(d));
    }
    XMLUtils::addChildrenWithAttributes(doc, node, "CrIncreases", "CrIncrease", tmp, "startDate", crIncreaseDates_);
    return node;
}

void ConvertibleBondData::CallabilityData::MakeWholeData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "MakeWhole");
    if (auto tmp = XMLUtils::getChildNode(node, "ConversionRatioIncrease")) {
        conversionRatioIncreaseData_.fromXML(tmp);
    }
    initialised_ = true;
}

XMLNode* ConvertibleBondData::CallabilityData::MakeWholeData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("MakeWhole");
    if (conversionRatioIncreaseData_.initialised()) {
        XMLUtils::appendNode(node, conversionRatioIncreaseData_.toXML(doc));
    }
    return node;
}

void ConvertibleBondData::CallabilityData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, nodeName_);
    dates_.fromXML(XMLUtils::getChildNode(node, "ScheduleData"));
    styles_ = XMLUtils::getChildrenValuesWithAttributes(node, "Styles", "Style", "startDate", styleDates_, true);
    prices_ = XMLUtils::getChildrenValuesWithAttributes<double>(node, "Prices", "Price", "startDate", priceDates_,
                                                                &parseReal, true);
    priceTypes_ =
        XMLUtils::getChildrenValuesWithAttributes(node, "PriceTypes", "PriceType", "startDate", priceTypeDates_, true);
    includeAccrual_ = XMLUtils::getChildrenValuesWithAttributes<bool>(
        node, "IncludeAccruals", "IncludeAccrual", "startDate", includeAccrualDates_, &parseBool, true);
    isSoft_ = XMLUtils::getChildrenValuesWithAttributes<bool>(node, "Soft", "Soft", "startDate", isSoftDates_,
                                                              &parseBool, false);
    triggerRatios_ = XMLUtils::getChildrenValuesWithAttributes<double>(
        node, "TriggerRatios", "TriggerRatio", "startDate", triggerRatioDates_, &parseReal, false);
    nOfMTriggers_ = XMLUtils::getChildrenValuesWithAttributes(node, "NofMTriggers", "NofMTrigger", "startDate",
                                                              nOfMTriggerDates_, false);
    if (auto tmp = XMLUtils::getChildNode(node, "MakeWhole")) {
        makeWholeData_.fromXML(tmp);
    }
    initialised_ = true;
}

XMLNode* ConvertibleBondData::CallabilityData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(nodeName_);
    XMLUtils::appendNode(node, dates_.toXML(doc));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Styles", "Style", styles_, "startDate", styleDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Prices", "Price", prices_, "startDate", priceDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "PriceTypes", "PriceType", priceTypes_, "startDate",
                                                priceTypeDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "IncludeAccruals", "IncludeAccrual", includeAccrual_,
                                                "startDate", includeAccrualDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Soft", "Soft", isSoft_, "startDate", isSoftDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "TriggerRatios", "TriggerRatio", triggerRatios_, "startDate",
                                                triggerRatioDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "NOfMTriggers", "NOfMTrigger", nOfMTriggers_, "startDate",
                                                nOfMTriggerDates_);
    if (makeWholeData_.initialised()) {
        XMLUtils::appendNode(node, makeWholeData_.toXML(doc));
    }
    return node;
}

void ConvertibleBondData::ConversionData::ContingentConversionData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ContingentConversion");
    observations_ = XMLUtils::getChildrenValuesWithAttributes(node, "Observations", "Observation", "startDate",
                                                              observationDates_, true);
    barriers_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Barriers", "Barrier", "startDate", barrierDates_,
                                                                &parseReal, true);
    initialised_ = true;
}

XMLNode* ConvertibleBondData::ConversionData::ContingentConversionData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ContingentConversion");
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Observations", "Observation", observations_, "startDate",
                                                observationDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Barriers", "Barrier", barriers_, "startDate",
                                                barrierDates_);
    return node;
}

void ConvertibleBondData::ConversionData::MandatoryConversionData::PepsData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "PepsData");
    upperBarrier_ = XMLUtils::getChildValueAsDouble(node, "UpperBarrier", true);
    lowerBarrier_ = XMLUtils::getChildValueAsDouble(node, "LowerBarrier", true);
    upperConversionRatio_ = XMLUtils::getChildValueAsDouble(node, "UpperConversionRatio", true);
    lowerConversionRatio_ = XMLUtils::getChildValueAsDouble(node, "LowerConversionRatio", true);
    initialised_ = true;
}

XMLNode* ConvertibleBondData::ConversionData::MandatoryConversionData::PepsData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("PepsData");
    XMLUtils::addChild(doc, node, "UpperBarrier", upperBarrier_);
    XMLUtils::addChild(doc, node, "LowerBarrier", upperBarrier_);
    XMLUtils::addChild(doc, node, "UpperConversionRatio", upperConversionRatio_);
    XMLUtils::addChild(doc, node, "LowerConversionRatio", lowerConversionRatio_);
    return node;
}

void ConvertibleBondData::ConversionData::MandatoryConversionData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "MandatoryConversion");
    date_ = XMLUtils::getChildValue(node, "Date", true);
    type_ = XMLUtils::getChildValue(node, "Type", true);
    if (auto tmp = XMLUtils::getChildNode(node, "PepsData")) {
        if (!XMLUtils::getChildrenNodes(tmp, "").empty())
            pepsData_.fromXML(tmp);
    }
    initialised_ = true;
}

XMLNode* ConvertibleBondData::ConversionData::MandatoryConversionData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("MandatoryConversion");
    XMLUtils::addChild(doc, node, "Date", date_);
    XMLUtils::addChild(doc, node, "Type", type_);
    if (pepsData_.initialised())
        XMLUtils::appendNode(node, pepsData_.toXML(doc));
    return node;
}

void ConvertibleBondData::ConversionData::ConversionResetData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ConversionResets");
    dates_.fromXML(XMLUtils::getChildNode(node, "ScheduleData"));
    references_ =
        XMLUtils::getChildrenValuesWithAttributes(node, "References", "Reference", "startDate", referenceDates_, true);
    thresholds_ = XMLUtils::getChildrenValuesWithAttributes<double>(node, "Thresholds", "Threshold", "startDate",
                                                                    thresholdDates_, &parseReal, true);
    gearings_ = XMLUtils::getChildrenValuesWithAttributes<double>(node, "Gearings", "Gearing", "startDate",
                                                                  gearingDates_, &parseReal, true);
    floors_ = XMLUtils::getChildrenValuesWithAttributes<double>(node, "Floors", "Floor", "startDate", floorDates_,
                                                                &parseReal, false);
    globalFloors_ = XMLUtils::getChildrenValuesWithAttributes<double>(node, "GlobalFloors", "GlobalFloor", "startDate",
                                                                      globalFloorDates_, &parseReal, false);

    initialised_ = true;
}

XMLNode* ConvertibleBondData::ConversionData::ConversionResetData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ConversionResets");
    XMLUtils::appendNode(node, dates_.toXML(doc));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "References", "Reference", references_, "startDate",
                                                referenceDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Thresholds", "Threshold", thresholds_, "startDate",
                                                thresholdDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate",
                                                gearingDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Floors", "Floor", floors_, "startDate", floorDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "GlobalFloors", "GlobalFloor", globalFloors_, "startDate",
                                                globalFloorDates_);
    return node;
}

void ConvertibleBondData::ConversionData::ExchangeableData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Exchangeable");
    isExchangeable_ = XMLUtils::getChildValueAsBool(node, "IsExchangeable", true);
    equityCreditCurve_ = XMLUtils::getChildValue(node, "EquityCreditCurve", isExchangeable_);
    secured_ = false;
    if (auto tmp = XMLUtils::getChildNode(node, "Secured")) {
        if (!XMLUtils::getNodeValue(tmp).empty())
            secured_ = parseBool(XMLUtils::getNodeValue(tmp));
    }
    initialised_ = true;
}

XMLNode* ConvertibleBondData::ConversionData::ExchangeableData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Exchangeable");
    XMLUtils::addChild(doc, node, "IsExchangeable", isExchangeable_);
    XMLUtils::addChild(doc, node, "EquityCreditCurve", equityCreditCurve_);
    XMLUtils::addChild(doc, node, "Secured", secured_);
    return node;
}

void ConvertibleBondData::ConversionData::FixedAmountConversionData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "FixedAmountConversion");
    currency_ = XMLUtils::getChildValue(node, "Currency", true);
    amounts_ = XMLUtils::getChildrenValuesWithAttributes<double>(node, "Amounts", "Amount", "startDate", amountDates_,
                                                                 &parseReal, true);
    initialised_ = true;
}

XMLNode* ConvertibleBondData::ConversionData::FixedAmountConversionData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("FixedAmountConversion");
    XMLUtils::addChild(doc, node, "Currency", currency_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Amounts", "Amount", amounts_, "startDate", amountDates_);
    return node;
}

void ConvertibleBondData::ConversionData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ConversionData");
    if (auto tmp = XMLUtils::getChildNode(node, "ScheduleData"))
        dates_.fromXML(tmp);
    styles_ = XMLUtils::getChildrenValuesWithAttributes(node, "Styles", "Style", "startDate", styleDates_, false);
    conversionRatios_ = XMLUtils::getChildrenValuesWithAttributes<double>(
        node, "ConversionRatios", "ConversionRatio", "startDate", conversionRatioDates_, &parseReal, false);
    if (auto tmp = XMLUtils::getChildNode(node, "ContingentConversion")) {
        if (!XMLUtils::getChildrenNodes(tmp, "").empty())
            contingentConversionData_.fromXML(tmp);
    }
    if (auto tmp = XMLUtils::getChildNode(node, "MandatoryConversion")) {
        if (!XMLUtils::getChildrenNodes(tmp, "").empty())
            mandatoryConversionData_.fromXML(tmp);
    }
    if (auto tmp = XMLUtils::getChildNode(node, "ConversionResets")) {
        if (!XMLUtils::getChildrenNodes(tmp, "").empty())
            conversionResetData_.fromXML(tmp);
    }
    if (auto tmp = XMLUtils::getChildNode(node, "Underlying")) {
        equityUnderlying_.fromXML(tmp);
    }
    fxIndex_ = XMLUtils::getChildValue(node, "FXIndex", false);
    if (XMLUtils::getChildNode(node, "FXIndexFixingDays")) {
        WLOG("ConvertibleBondData::fromXML, node FXIndexFixingDays has been deprecated, fixing days are "
             "taken from conventions.");
    }
    if (XMLUtils::getChildNode(node, "FXIndexCalendar")) {
        WLOG("ConvertibleBondData::fromXML, node FXIndexCalendar has been deprecated, fixing calendar is "
             "taken from conventions.");
    }
    if (auto tmp = XMLUtils::getChildNode(node, "Exchangeable")) {
        if (!XMLUtils::getChildrenNodes(tmp, "").empty())
            exchangeableData_.fromXML(tmp);
    }
    if (auto tmp = XMLUtils::getChildNode(node, "FixedAmountConversion")) {
        if (!XMLUtils::getChildrenNodes(tmp, "").empty())
            fixedAmountConversionData_.fromXML(tmp);
    }
    initialised_ = true;
}

XMLNode* ConvertibleBondData::ConversionData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ConversionData");
    XMLUtils::appendNode(node, dates_.toXML(doc));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Styles", "Style", styles_, "startDate", styleDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "ConversionRatios", "ConversionRatio", conversionRatios_,
                                                "startDate", conversionRatioDates_);
    if (contingentConversionData_.initialised())
        XMLUtils::appendNode(node, contingentConversionData_.toXML(doc));
    if (mandatoryConversionData_.initialised())
        XMLUtils::appendNode(node, mandatoryConversionData_.toXML(doc));
    if (conversionResetData_.initialised())
        XMLUtils::appendNode(node, conversionResetData_.toXML(doc));
    XMLUtils::appendNode(node, equityUnderlying_.toXML(doc));
    if (!fxIndex_.empty())
        XMLUtils::addChild(doc, node, "FXIndex", fxIndex_);
    if (exchangeableData_.initialised())
        XMLUtils::appendNode(node, exchangeableData_.toXML(doc));
    if (fixedAmountConversionData_.initialised())
        XMLUtils::appendNode(node, fixedAmountConversionData_.toXML(doc));
    return node;
}

void ConvertibleBondData::DividendProtectionData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "DividendProtectionData");
    dates_.fromXML(XMLUtils::getChildNode(node, "ScheduleData"));
    adjustmentStyles_ = XMLUtils::getChildrenValuesWithAttributes(node, "AdjustmentStyles", "AdjustmentStyle",
                                                                  "startDate", adjustmentStyleDates_, true);
    dividendTypes_ = XMLUtils::getChildrenValuesWithAttributes(node, "DividendTypes", "DividendType", "startDate",
                                                               dividendTypeDates_, true);
    thresholds_ = XMLUtils::getChildrenValuesWithAttributes<double>(node, "Thresholds", "Threshold", "startDate",
                                                                    thresholdDates_, &parseReal, true);
    initialised_ = true;
}

XMLNode* ConvertibleBondData::DividendProtectionData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("DividendProtectionData");
    XMLUtils::appendNode(node, dates_.toXML(doc));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "AdjustmentStyles", "AdjustmentStyle", adjustmentStyles_,
                                                "startDate", adjustmentStyleDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "DividendTypes", "DividendType", dividendTypes_, "startDate",
                                                dividendTypeDates_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Thresholds", "Threshold", thresholds_, "startDate",
                                                thresholdDates_);
    return node;
}

void ConvertibleBondData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ConvertibleBondData");
    bondData_.fromXML(XMLUtils::getChildNode(node, "BondData"));
    if (auto tmp = XMLUtils::getChildNode(node, "CallData")) {
        if (!XMLUtils::getChildrenNodes(tmp, "").empty()) {
            callData_.fromXML(tmp);
        }
    }
    if (auto tmp = XMLUtils::getChildNode(node, "PutData")) {
        if (!XMLUtils::getChildrenNodes(tmp, "").empty())
            putData_.fromXML(tmp);
    }
    if (auto tmp = XMLUtils::getChildNode(node, "ConversionData")) {
        if (!XMLUtils::getChildrenNodes(tmp, "").empty())
            conversionData_.fromXML(tmp);
    }
    if (auto tmp = XMLUtils::getChildNode(node, "DividendProtectionData")) {
        if (!XMLUtils::getChildrenNodes(tmp, "").empty())
            dividendProtectionData_.fromXML(tmp);
    }
    detachable_ = XMLUtils::getChildValue(node, "Detachable");
}

XMLNode* ConvertibleBondData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ConvertibleBondData");
    XMLUtils::appendNode(node, bondData_.toXML(doc));
    if (callData_.initialised())
        XMLUtils::appendNode(node, callData_.toXML(doc));
    if (putData_.initialised())
        XMLUtils::appendNode(node, putData_.toXML(doc));
    if (conversionData_.initialised())
        XMLUtils::appendNode(node, conversionData_.toXML(doc));
    if (dividendProtectionData_.initialised())
        XMLUtils::appendNode(node, dividendProtectionData_.toXML(doc));
    if (!detachable_.empty())
        XMLUtils::addChild(doc, node, "Detachable", detachable_);
    return node;
}

void ConvertibleBondData::populateFromBondReferenceData(
    const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData) {

    QL_REQUIRE(!bondData_.securityId().empty(),
               "ConvertibleBondData::populateFromBondReferenceData(): no security id given");
    if (!referenceData || !referenceData->hasData(ConvertibleBondReferenceDatum::TYPE, bondData_.securityId())) {
        DLOG("could not get ConvertibleBondReferenceDatum for name " << bondData_.securityId()
                                                                     << " leave data in trade unchanged");
    } else {
        auto bondRefData = QuantLib::ext::dynamic_pointer_cast<ConvertibleBondReferenceDatum>(
            referenceData->getData(ConvertibleBondReferenceDatum::TYPE, bondData_.securityId()));
        QL_REQUIRE(bondRefData, "could not cast to ConvertibleBondReferenceDatum, this is unexpected");
        DLOG("Got ConvertibleBondReferenceDatum for name " << bondData_.securityId()
                                                           << " overwrite empty elements in trade");
        bondData_.populateFromBondReferenceData(
            QuantLib::ext::make_shared<BondReferenceDatum>(bondData_.securityId(), bondRefData->bondData()));
        if (!callData_.initialised()) {
            DLOG("overwrite CallData from reference data")
            callData_ = bondRefData->callData();
        }
        if (!putData_.initialised()) {
            DLOG("overwrite PutData from reference data")
            putData_ = bondRefData->putData();
        }
        if (!conversionData_.initialised()) {
            DLOG("overwrite ConversionData from reference data")
            conversionData_ = bondRefData->conversionData();
        }
        if (!dividendProtectionData_.initialised()) {
            DLOG("overwrite DividendProtectionData from reference data")
            dividendProtectionData_ = bondRefData->dividendProtectionData();
        }
        if (detachable_.empty()) {
            DLOG("overwrite detachable from reference data");
            detachable_ = bondRefData->detachable();
        }
    }
}

} // namespace data
} // namespace ore
