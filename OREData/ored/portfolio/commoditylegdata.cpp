/*
  Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/portfolio/commoditylegdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/make_shared.hpp>

using boost::iequals;
using ore::data::parseBool;
using ore::data::parseCommodityQuantityFrequency;
using ore::data::parseInteger;
using ore::data::parseReal;
using ore::data::to_string;
using ore::data::XMLDocument;
using ore::data::XMLNode;
using ore::data::XMLUtils;
using QuantExt::CommodityQuantityFrequency;
using QuantLib::Natural;
using QuantLib::Null;
using QuantLib::Real;
using std::ostream;
using std::string;
using std::vector;

namespace ore {
namespace data {

CommodityPayRelativeTo parseCommodityPayRelativeTo(const string& s) {
    if (iequals(s, "CalculationPeriodEndDate")) {
        return CommodityPayRelativeTo::CalculationPeriodEndDate;
    } else if (iequals(s, "CalculationPeriodStartDate")) {
        return CommodityPayRelativeTo::CalculationPeriodStartDate;
    } else if (iequals(s, "TerminationDate")) {
        return CommodityPayRelativeTo::TerminationDate;
    } else if (iequals(s, "FutureExpiryDate")) {
        return CommodityPayRelativeTo::FutureExpiryDate;
    } else {
        QL_FAIL("Could not parse " << s << " to CommodityPayRelativeTo");
    }
}

ostream& operator<<(ostream& out, const CommodityPayRelativeTo& cprt) {
    if (cprt == CommodityPayRelativeTo::CalculationPeriodEndDate) {
        return out << "CalculationPeriodEndDate";
    } else if (cprt == CommodityPayRelativeTo::CalculationPeriodStartDate) {
        return out << "CalculationPeriodStartDate";
    } else if (cprt == CommodityPayRelativeTo::TerminationDate) {
        return out << "TerminationDate";
    } else if (cprt == CommodityPayRelativeTo::FutureExpiryDate) {
        return out << "FutureExpiryDate";
    } else {
        QL_FAIL("Do not recognise CommodityPayRelativeTo " << static_cast<int>(cprt));
    }
}

CommodityPriceType parseCommodityPriceType(const string& s) {
    if (iequals(s, "Spot")) {
        return CommodityPriceType::Spot;
    } else if (iequals(s, "FutureSettlement")) {
        return CommodityPriceType::FutureSettlement;
    } else {
        QL_FAIL("Could not parse " << s << " to CommodityPriceType");
    }
}

ostream& operator<<(ostream& out, const CommodityPriceType& cpt) {
    if (cpt == CommodityPriceType::Spot) {
        return out << "Spot";
    } else if (cpt == CommodityPriceType::FutureSettlement) {
        return out << "FutureSettlement";
    } else {
        QL_FAIL("Do not recognise CommodityPriceType " << static_cast<int>(cpt));
    }
}

CommodityPricingDateRule parseCommodityPricingDateRule(const string& s) {
    if (iequals(s, "FutureExpiryDate")) {
        return CommodityPricingDateRule::FutureExpiryDate;
    } else if (iequals(s, "None")) {
        return CommodityPricingDateRule::None;
    } else {
        QL_FAIL("Could not parse " << s << " to CommodityPricingDateRule");
    }
}

ostream& operator<<(std::ostream& out, const CommodityPricingDateRule& cpdr) {
    if (cpdr == CommodityPricingDateRule::FutureExpiryDate) {
        return out << "FutureExpiryDate";
    } else if (cpdr == CommodityPricingDateRule::None) {
        return out << "None";
    } else {
        QL_FAIL("Do not recognise CommodityPricingDateRule " << static_cast<int>(cpdr));
    }
}

CommodityFixedLegData::CommodityFixedLegData()
    : ore::data::LegAdditionalData("CommodityFixed"),
      commodityPayRelativeTo_(CommodityPayRelativeTo::CalculationPeriodEndDate) {}

CommodityFixedLegData::CommodityFixedLegData(const vector<Real>& quantities, const vector<string>& quantityDates,
                                             const vector<Real>& prices, const vector<string>& priceDates,
                                             CommodityPayRelativeTo commodityPayRelativeTo, const string& tag)
    : LegAdditionalData("CommodityFixed"), quantities_(quantities), quantityDates_(quantityDates), prices_(prices),
      priceDates_(priceDates), commodityPayRelativeTo_(commodityPayRelativeTo), tag_(tag) {}

void CommodityFixedLegData::setQuantities(const vector<Real>& quantities) {
    // Ensure that the quantityDates_ are cleared also.
    quantities_ = quantities;
    quantityDates_.clear();
}

void CommodityFixedLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CommodityFixedLegData");

    quantities_.clear();
    if (XMLUtils::getChildNode(node, "Quantities")) {
        quantities_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Quantities", "Quantity",
            "startDate", quantityDates_, &parseReal, true);
    }

    prices_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Prices", "Price", "startDate", priceDates_,
                                                              &parseReal, true);

    commodityPayRelativeTo_ = CommodityPayRelativeTo::CalculationPeriodEndDate;
    if (XMLNode* n = XMLUtils::getChildNode(node, "CommodityPayRelativeTo")) {
        commodityPayRelativeTo_ = parseCommodityPayRelativeTo(XMLUtils::getNodeValue(n));
    }
    tag_ = XMLUtils::getChildValue(node, "Tag", false);
}

XMLNode* CommodityFixedLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("CommodityFixedLegData");

    if (!quantities_.empty()) {
        XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Quantities", "Quantity",
            quantities_, "startDate", quantityDates_);
    }

    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Prices", "Price", prices_, "startDate", priceDates_);
    XMLUtils::addChild(doc, node, "CommodityPayRelativeTo", to_string(commodityPayRelativeTo_));
    if (!tag_.empty())
        XMLUtils::addChild(doc, node, "Tag", tag_);

    return node;
}

CommodityFloatingLegData::CommodityFloatingLegData()
    : ore::data::LegAdditionalData("CommodityFloating"),
      priceType_(CommodityPriceType::FutureSettlement),
      commodityQuantityFrequency_(CommodityQuantityFrequency::PerCalculationPeriod),
      commodityPayRelativeTo_(CommodityPayRelativeTo::CalculationPeriodEndDate),
      pricingDateRule_(CommodityPricingDateRule::FutureExpiryDate),
      pricingLag_(0), isAveraged_(false), isInArrears_(true), futureMonthOffset_(0),
      deliveryRollDays_(0), includePeriodEnd_(true), excludePeriodStart_(true),
      hoursPerDay_(Null<Natural>()), useBusinessDays_(true), dailyExpiryOffset_(0),
      unrealisedQuantity_(false), lastNDays_(Null<Natural>()), fxIndex_("") {}

CommodityFloatingLegData::CommodityFloatingLegData(
    const string& name, CommodityPriceType priceType, const vector<Real>& quantities,
    const vector<string>& quantityDates, CommodityQuantityFrequency commodityQuantityFrequency,
    CommodityPayRelativeTo commodityPayRelativeTo, const vector<Real>& spreads, const vector<string>& spreadDates,
    const vector<Real>& gearings, const vector<string>& gearingDates, CommodityPricingDateRule pricingDateRule,
    const string& pricingCalendar, Natural pricingLag, const vector<string>& pricingDates, bool isAveraged,
    bool isInArrears, Natural futureMonthOffset, Natural deliveryRollDays, bool includePeriodEnd,
    bool excludePeriodStart, Natural hoursPerDay, bool useBusinessDays, const string& tag,
    Natural dailyExpiryOffset, bool unrealisedQuantity, QuantLib::Natural lastNDays, std::string fxIndex)
    : LegAdditionalData("CommodityFloating"), name_(name), priceType_(priceType), quantities_(quantities),
      quantityDates_(quantityDates), commodityQuantityFrequency_(commodityQuantityFrequency),
      commodityPayRelativeTo_(commodityPayRelativeTo), spreads_(spreads), spreadDates_(spreadDates),
      gearings_(gearings), gearingDates_(gearingDates), pricingDateRule_(pricingDateRule),
      pricingCalendar_(pricingCalendar), pricingLag_(pricingLag), pricingDates_(pricingDates), isAveraged_(isAveraged),
      isInArrears_(isInArrears), futureMonthOffset_(futureMonthOffset), deliveryRollDays_(deliveryRollDays),
      includePeriodEnd_(includePeriodEnd), excludePeriodStart_(excludePeriodStart), hoursPerDay_(hoursPerDay),
      useBusinessDays_(useBusinessDays), tag_(tag), dailyExpiryOffset_(dailyExpiryOffset),
      unrealisedQuantity_(unrealisedQuantity), lastNDays_(lastNDays), fxIndex_(fxIndex) {
    indices_.insert("COMM-" + name_);
}

void CommodityFloatingLegData::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "CommodityFloatingLegData");

    name_ = XMLUtils::getChildValue(node, "Name", true);
    indices_.insert("COMM-" + name_);
    priceType_ = parseCommodityPriceType(XMLUtils::getChildValue(node, "PriceType", true));
    quantities_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Quantities", "Quantity", "startDate",
                                                                  quantityDates_, &parseReal, true);

    commodityQuantityFrequency_ = CommodityQuantityFrequency::PerCalculationPeriod;
    if (XMLNode* n = XMLUtils::getChildNode(node, "CommodityQuantityFrequency")) {
        commodityQuantityFrequency_ = parseCommodityQuantityFrequency(XMLUtils::getNodeValue(n));
    }

    commodityPayRelativeTo_ = CommodityPayRelativeTo::CalculationPeriodEndDate;
    if (XMLNode* n = XMLUtils::getChildNode(node, "CommodityPayRelativeTo")) {
        commodityPayRelativeTo_ = parseCommodityPayRelativeTo(XMLUtils::getNodeValue(n));
    }

    spreads_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Spreads", "Spread", "startDate", spreadDates_,
                                                               &parseReal);
    gearings_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Gearings", "Gearing", "startDate", gearingDates_,
                                                                &parseReal);

    pricingDateRule_ = CommodityPricingDateRule::FutureExpiryDate;
    if (XMLNode* n = XMLUtils::getChildNode(node, "PricingDateRule")) {
        pricingDateRule_ = parseCommodityPricingDateRule(XMLUtils::getNodeValue(n));
    }

    pricingCalendar_ = XMLUtils::getChildValue(node, "PricingCalendar", false);
    pricingLag_ = XMLUtils::getChildValueAsInt(node, "PricingLag", false);
    pricingDates_ = XMLUtils::getChildrenValues(node, "PricingDates", "PricingDate", false);

    isAveraged_ = false;
    if (XMLNode* n = XMLUtils::getChildNode(node, "IsAveraged")) {
        isAveraged_ = parseBool(XMLUtils::getNodeValue(n));
    }

    isInArrears_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "IsInArrears")) {
        isInArrears_ = parseBool(XMLUtils::getNodeValue(n));
    }

    futureMonthOffset_ = XMLUtils::getChildValueAsInt(node, "FutureMonthOffset", false);
    deliveryRollDays_ = XMLUtils::getChildValueAsInt(node, "DeliveryRollDays", false);

    includePeriodEnd_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "IncludePeriodEnd")) {
        includePeriodEnd_ = parseBool(XMLUtils::getNodeValue(n));
    }

    excludePeriodStart_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "ExcludePeriodStart")) {
        excludePeriodStart_ = parseBool(XMLUtils::getNodeValue(n));
    }

    hoursPerDay_ = Null<Natural>();
    if (XMLNode* n = XMLUtils::getChildNode(node, "HoursPerDay")) {
        hoursPerDay_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    useBusinessDays_ = true;
    if (XMLNode* n = XMLUtils::getChildNode(node, "UseBusinessDays")) {
        useBusinessDays_ = parseBool(XMLUtils::getNodeValue(n));
    }

    tag_ = XMLUtils::getChildValue(node, "Tag", false);

    dailyExpiryOffset_ = Null<Natural>();
    if (XMLNode* n = XMLUtils::getChildNode(node, "DailyExpiryOffset")) {
        dailyExpiryOffset_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    unrealisedQuantity_ = false;
    if (XMLNode* n = XMLUtils::getChildNode(node, "UnrealisedQuantity")) {
        unrealisedQuantity_ = parseBool(XMLUtils::getNodeValue(n));
    }

    lastNDays_ = Null<Natural>();
    if (XMLNode* n = XMLUtils::getChildNode(node, "LastNDays")) {
        lastNDays_ = parseInteger(XMLUtils::getNodeValue(n));
    }

    if (XMLNode* n = XMLUtils::getChildNode(node, "FXIndex")) {
        fxIndex_ = XMLUtils::getNodeValue(n);
    }
}

XMLNode* CommodityFloatingLegData::toXML(XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("CommodityFloatingLegData");

    XMLUtils::addChild(doc, node, "Name", name_);
    XMLUtils::addChild(doc, node, "PriceType", to_string(priceType_));
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Quantities", "Quantity", quantities_, "startDate",
                                                quantityDates_);
    XMLUtils::addChild(doc, node, "CommodityQuantityFrequency", to_string(commodityQuantityFrequency_));
    XMLUtils::addChild(doc, node, "CommodityPayRelativeTo", to_string(commodityPayRelativeTo_));

    if (!spreads_.empty())
        XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Spreads", "Spread", spreads_, "startDate",
                                                    spreadDates_);

    if (!gearings_.empty())
        XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Gearings", "Gearing", gearings_, "startDate",
                                                    gearingDates_);

    XMLUtils::addChild(doc, node, "PricingDateRule", to_string(pricingDateRule_));

    if (!pricingCalendar_.empty())
        XMLUtils::addChild(doc, node, "PricingCalendar", pricingCalendar_);

    XMLUtils::addChild(doc, node, "PricingLag", static_cast<int>(pricingLag_));

    if (!pricingDates_.empty())
        XMLUtils::addChildren(doc, node, "PricingDates", "PricingDate", pricingDates_);

    XMLUtils::addChild(doc, node, "IsAveraged", isAveraged_);
    XMLUtils::addChild(doc, node, "IsInArrears", isInArrears_);
    XMLUtils::addChild(doc, node, "FutureMonthOffset", static_cast<int>(futureMonthOffset_));
    XMLUtils::addChild(doc, node, "DeliveryRollDays", static_cast<int>(deliveryRollDays_));
    XMLUtils::addChild(doc, node, "IncludePeriodEnd", includePeriodEnd_);
    XMLUtils::addChild(doc, node, "ExcludePeriodStart", excludePeriodStart_);
    if (hoursPerDay_ != Null<Natural>())
        XMLUtils::addChild(doc, node, "HoursPerDay", static_cast<int>(hoursPerDay_));
    XMLUtils::addChild(doc, node, "UseBusinessDays", useBusinessDays_);
    if (!tag_.empty())
        XMLUtils::addChild(doc, node, "Tag", tag_);
    if (dailyExpiryOffset_ != Null<Natural>())
        XMLUtils::addChild(doc, node, "DailyExpiryOffset", static_cast<int>(dailyExpiryOffset_));
    if (unrealisedQuantity_)
        XMLUtils::addChild(doc, node, "UnrealisedQuantity", unrealisedQuantity_);
    if (lastNDays_ != Null<Natural>())
        XMLUtils::addChild(doc, node, "LastNDays", static_cast<int>(lastNDays_));
    if (!fxIndex_.empty())
        XMLUtils::addChild(doc, node, "FXIndex", fxIndex_);

    return node;
}

} // namespace data
} // namespace ore
