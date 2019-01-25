/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/cpicapfloor.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/cpicapfloor.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>

#include <ql/instruments/cpicapfloor.hpp>
#include <qle/indexes/inflationindexwrapper.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace ore {
namespace data {
void CPICapFloor::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("CPI CapFloor builder called for " << id());

    // Clear legs before building
    legs_.clear();

    // Make sure that the floating leg section does not have caps or floors
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("CPICapFloor");
    QL_REQUIRE(builder, "CPICapFloor EngineBuilder not set");

    Handle<ZeroInflationIndex> zeroIndex =
        engineFactory->market()->zeroInflationIndex(index_, builder->configuration(MarketContext::pricing));
    QL_REQUIRE(!zeroIndex.empty(), "Zero inflation index " << index_ << " is empty");

    QuantLib::Option::Type optionType;
    if (capFloor_ == "Cap")
        optionType = QuantLib::Option::Call;
    else if (capFloor_ == "Floor")
        optionType = QuantLib::Option::Put;
    else {
        QL_FAIL("CapFloorType " << capFloor_ << " not covered");
    }

    QuantLib::Date start = parseDate(startDate_);
    QuantLib::Date end = parseDate(maturityDate_);
    QuantLib::Calendar fixCal = parseCalendar(fixCalendar_);
    QuantLib::Calendar payCal = parseCalendar(payCalendar_);
    QuantLib::BusinessDayConvention fixCon = parseBusinessDayConvention(fixConvention_);
    QuantLib::BusinessDayConvention payCon = parseBusinessDayConvention(payConvention_);
    QuantLib::Period obsLag = parsePeriod(observationLag_);

    boost::shared_ptr<QuantLib::CPICapFloor> capFloor = boost::make_shared<QuantLib::CPICapFloor>(
        optionType, nominal_, start, baseCPI_, end, fixCal, fixCon, payCal, payCon, strike_, zeroIndex, obsLag);

    boost::shared_ptr<CPICapFloorEngineBuilderBase> capFloorBuilder =
        boost::dynamic_pointer_cast<CPICapFloorEngineBuilderBase>(builder);
    QL_REQUIRE(capFloorBuilder, "CPI CapFloor engine builder not set");
    DLOG("CPICapFloor type=" << capFloor_);
    capFloor->setPricingEngine(capFloorBuilder->engine(index_, capFloor_));

    // Wrap the QL instrument in a vanilla instrument
    Real multiplier = (parsePositionType(longShort_) == Position::Long ? 1.0 : -1.0);
    instrument_ = boost::make_shared<VanillaInstrument>(capFloor, multiplier);

    // Fill in remaining Trade member data
    maturity_ = capFloor->payDate();
    // FIXME
    // legCurrencies_.push_back(legData_.currency());
    // legPayers_.push_back(legData_.isPayer());
    npvCurrency_ = currency_;
    notional_ = nominal_;
}

void CPICapFloor::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* capFloorNode = XMLUtils::getChildNode(node, "CPICapFloorData");
    longShort_ = XMLUtils::getChildValue(capFloorNode, "LongShort", true);
    capFloor_ = XMLUtils::getChildValue(capFloorNode, "CapFloor", true);
    currency_ = XMLUtils::getChildValue(capFloorNode, "Currency", true);
    nominal_ = XMLUtils::getChildValueAsDouble(capFloorNode, "Nominal", true);
    startDate_ = XMLUtils::getChildValue(capFloorNode, "StartDate", true);
    baseCPI_ = XMLUtils::getChildValueAsDouble(capFloorNode, "BaseCPI", true);
    maturityDate_ = XMLUtils::getChildValue(capFloorNode, "MaturityDate", true);
    fixCalendar_ = XMLUtils::getChildValue(capFloorNode, "FixCalendar", true);
    fixConvention_ = XMLUtils::getChildValue(capFloorNode, "FixConvention", true);
    payCalendar_ = XMLUtils::getChildValue(capFloorNode, "PayCalendar", true);
    payConvention_ = XMLUtils::getChildValue(capFloorNode, "PayConvention", true);
    strike_ = XMLUtils::getChildValueAsDouble(capFloorNode, "Strike", true);
    index_ = XMLUtils::getChildValue(capFloorNode, "Index", true);
    observationLag_ = XMLUtils::getChildValue(capFloorNode, "ObservationLag", true);
}

XMLNode* CPICapFloor::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* capFloorNode = doc.allocNode("CPICapFloorData");
    XMLUtils::appendNode(node, capFloorNode);
    XMLUtils::addChild(doc, capFloorNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, capFloorNode, "CapFloor", capFloor_);
    XMLUtils::addChild(doc, capFloorNode, "Currency", currency_);
    XMLUtils::addChild(doc, capFloorNode, "Nominal", nominal_);
    XMLUtils::addChild(doc, capFloorNode, "StartDate", startDate_);
    XMLUtils::addChild(doc, capFloorNode, "BaseCPI", baseCPI_);
    XMLUtils::addChild(doc, capFloorNode, "MaturityDate", maturityDate_);
    XMLUtils::addChild(doc, capFloorNode, "FixCalendar", fixCalendar_);
    XMLUtils::addChild(doc, capFloorNode, "FixConvention", fixConvention_);
    XMLUtils::addChild(doc, capFloorNode, "PayCalendar", payCalendar_);
    XMLUtils::addChild(doc, capFloorNode, "PayConvention", payConvention_);
    XMLUtils::addChild(doc, capFloorNode, "Strike", strike_);
    XMLUtils::addChild(doc, capFloorNode, "Index", index_);
    XMLUtils::addChild(doc, capFloorNode, "ObservationLag", observationLag_);
    return node;
}
} // namespace data
} // namespace ore
