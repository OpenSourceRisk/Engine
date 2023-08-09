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
#include <ored/portfolio/builders/commodityspreadoption.hpp>
#include <ored/portfolio/commodityspreadoptionstrip.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore::data {

using namespace QuantExt;
using namespace QuantLib;
using std::max;
using std::string;
using std::vector;

void CommoditySpreadOptionStrip::build(const boost::shared_ptr<ore::data::EngineFactory>& engineFactory) {

    DLOG("CommoditySpreadOption::build() called for trade " << id());
    reset();
    auto legData_ = csoData_.legData();
    auto optionData_ = csoData_.optionData();
    
    // Get template option
    auto comLegData = ext::dynamic_pointer_cast<CommodityFloatingLegData>(legData_[0].concreteLegData());
    
    // Build Schedule for each leg
    auto schedule = makeSchedule(scheduleData_);
    QL_REQUIRE(schedule.size() >= 2, "Require min 2 dates");

    vector<boost::shared_ptr<Instrument>> additionalInstruments;
    vector<Real> additionalMultipliers;
    Currency ccy;
    std::string configuration;
    Position::Type positionType = parsePositionType(optionData_.longShort());  

    for (size_t i = 0; i < schedule.size() - 1; i++) {
        Date start = schedule[i];
        Date end = schedule[i + 1];
        ScheduleRules newScheduleRule(to_string(start), to_string(end), to_string(tenor_), to_string(cal_),
                                      to_string(bdc_), to_string(termBdc_), to_string(rule_));

        for (auto& leg : legData_) {
            leg.schedule() = ScheduleData(newScheduleRule);
        }
        auto optData = csoData_.optionData();
        if (paymentRelativeTo_ == LastExpiryInStrip) {
            auto newSchedule = makeSchedule(ScheduleData(newScheduleRule));
            Date lastExpiry = newSchedule.dates().back();
            Date paymentDate = paymentCalendar_.advance(lastExpiry, paymentLag_, QuantLib::Preceding);
            optData.setPaymentData(OptionPaymentData({to_string(paymentDate)}));
        } else {
            optData.setPaymentData(OptionPaymentData(to_string(paymentLag_), to_string(paymentCalendar_), "Preceding"));
        }
        CommoditySpreadOption option(CommoditySpreadOptionData(legData_, optData, csoData_.strike()));
        option.build(engineFactory);
        // To retrieve the config, there should be a easier way to do this
        boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(option.tradeType());
        auto engineBuilder = boost::dynamic_pointer_cast<CommoditySpreadOptionEngineBuilder>(builder);
        configuration = builder->configuration(MarketContext::pricing);
        ccy = parseCurrency(option.npvCurrency());
        
        npvCurrency_ = option.npvCurrency();

        maturity_ = std::max(maturity_, option.maturity());

        additionalInstruments.push_back(option.instrument()->qlInstrument());
        additionalInstruments.insert(additionalInstruments.end(), option.instrument()->additionalInstruments().begin(),
                                     option.instrument()->additionalInstruments().end());
        additionalMultipliers.push_back(1.0);
        additionalMultipliers.insert(additionalMultipliers.end(), option.instrument()->additionalMultipliers().begin(),
                                     option.instrument()->additionalMultipliers().end());
    }
    auto qlInst = additionalInstruments.back();
    auto qlInstMult = additionalMultipliers.back();
    additionalInstruments.pop_back();
    additionalMultipliers.pop_back();
    // TODO fix this
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    // Add premium
    maturity_ = std::max(maturity_, addPremiums(additionalInstruments, additionalMultipliers, bsInd,
                                                optionData_.premiumData(), -bsInd, ccy, engineFactory, configuration));

    instrument_ =
        boost::make_shared<VanillaInstrument>(qlInst, qlInstMult, additionalInstruments, additionalMultipliers);

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = std::string("Commodity");
    additionalData_["isdaBaseProduct"] = std::string("Other");
    additionalData_["isdaSubProduct"] = std::string("");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = std::string("");
    if (!optionData_.premiumData().premiumData().empty()) {
        auto premium = optionData_.premiumData().premiumData().front();
        additionalData_["premiumAmount"] = -bsInd * premium.amount;
        additionalData_["premiumPaymentDate"] = premium.payDate;
        additionalData_["premiumCurrency"] = premium.ccy;
    }
}

void CommoditySpreadOptionStrip::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* csoNode = XMLUtils::getChildNode(node, "CommoditySpreadOptionData");
    csoData_.fromXML(csoNode);
    XMLNode* premiumNode = XMLUtils::getChildNode(node, "Premiums");
    if (premiumNode)
        premiumData_.fromXML(premiumNode);
    XMLNode* scheduleNode = XMLUtils::getChildNode(node, "ScheduleData");
    scheduleData_.fromXML(scheduleNode);

    tenor_ = XMLUtils::getChildValueAsPeriod(node, "OptionStripTenor", false, 1 * Days);
    bdc_ = parseBusinessDayConvention(XMLUtils::getChildValue(node, "OptionStripConvention", false, "Unadjusted"));
    termBdc_ =
        parseBusinessDayConvention(XMLUtils::getChildValue(node, "OptionStripTermConvention", false, "Unadjusted"));
    rule_ = parseDateGenerationRule(XMLUtils::getChildValue(node, "OptionStripRule", false, "Backward"));
    cal_ = parseCalendar(XMLUtils::getChildValue(node, "OptionStripCalendar", false, "NullCalendar"));
    
    std::string payRelative = XMLUtils::getChildValue(node, "OptionStripPaymentRelativeTo", false, "ExpiryDate");
    if (payRelative == "LastExpiryInStrip") {
        paymentRelativeTo_ = LastExpiryInStrip;
    } else {
        paymentRelativeTo_ = Expiry;
    }
    paymentLag_ =  XMLUtils::getChildValueAsPeriod(node, "OptionStripPaymentLag", false, 0 * Days);
    paymentCalendar_ = parseCalendar(XMLUtils::getChildValue(node, "OptionStripPaymentCalendar", false, "NullCalendar"));
}
XMLNode* CommoditySpreadOptionStrip::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    auto csoNode = csoData_.toXML(doc);
    XMLUtils::appendNode(node, csoNode);
    auto scheduleNode = scheduleData_.toXML(doc);
    XMLUtils::appendNode(node, scheduleNode);
    auto premiumNode = premiumData_.toXML(doc);
    XMLUtils::appendNode(node, premiumNode);
    XMLUtils::addChild(doc, node, "OptionStripCalendar", to_string(cal_));
    XMLUtils::addChild(doc, node, "OptionStripTenor", to_string(tenor_));
    XMLUtils::addChild(doc, node, "OptionStripConvention", to_string(bdc_));
    XMLUtils::addChild(doc, node, "OptionStripTermConvention", to_string(termBdc_));
    XMLUtils::addChild(doc, node, "OptionStripRule", to_string(rule_));
    XMLUtils::addChild(doc, node, "OptionStripPaymentRelativeTo",
                       paymentRelativeTo_ == LastExpiryInStrip ? "LastExpiryInStrip" : "Expiry");
    XMLUtils::addChild(doc, node, "OptionStripPaymentLag", to_string(paymentLag_));
    XMLUtils::addChild(doc, node, "OptionStripPaymentCalendar", to_string(paymentCalendar_));
    return node;
}

} // namespace ore::data
