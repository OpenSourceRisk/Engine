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

/*! \file ored/portfolio/pairwisevarianceswap.hpp
\brief pairwise variance swap representation
\ingroup tradedata
*/

#include <ored/utilities/parsers.hpp>

#include <ored/portfolio/pairwisevarianceswap.hpp>
#include <qle/instruments/pairwisevarianceswap.hpp>

#include <ored/portfolio/referencedata.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void PairwiseVarSwap::build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) {

    // Build schedules
    ScheduleBuilder scheduleBuilder;

    Schedule valuationSchedule;
    scheduleBuilder.add(valuationSchedule, valuationSchedule_);

    Schedule laggedValuationSchedule;
    ScheduleData laggedValuationScheduleData =
        laggedValuationSchedule_.hasData()
            ? laggedValuationSchedule_
            : ScheduleData(ScheduleDerived(valuationSchedule_.name(), "NullCalendar", "Unadjusted", "1D"));
    scheduleBuilder.add(laggedValuationSchedule, laggedValuationScheduleData);

    scheduleBuilder.makeSchedules();

    // For the PairwiseVarSwap::toXML() to return an entry for LaggedValuationSchedule
    laggedValuationSchedule_ = laggedValuationScheduleData;

    const Currency ccy = parseCurrency(currency_);
    const Position::Type longShort = parsePositionType(longShort_);

    QL_REQUIRE(valuationSchedule.dates().size() == laggedValuationSchedule.dates().size(),
               "Trade " << id()
                        << " ValuationSchedule and LaggedValuationSchedule must have the same number of dates.");

    const Date settlementDate = parseDate(settlementDate_);

    QL_REQUIRE(basketStrike_ > 0 && !close_enough(basketStrike_, 0.0),
               "Trade " << id() << ": PairwiseVarSwap::build() basket strike must be positive (" << basketStrike_
                        << ")");
    QL_REQUIRE(basketNotional_ >= 0.0,
               "Trade " << id() << ": PairwiseVarSwap::build() basket notional must be non-negative (" << notional_
                        << ")");

    QuantLib::ext::shared_ptr<QuantExt::PairwiseVarianceSwap> pairwiseVarSwap(new QuantExt::PairwiseVarianceSwap(
        longShort, underlyingStrikes_[0], underlyingStrikes_[1], basketStrike_, underlyingNotionals_[0],
        underlyingNotionals_[1], basketNotional_, cap_, floor_, payoffLimit_, accrualLag_, valuationSchedule,
        laggedValuationSchedule, settlementDate));

    // Pricing Engine
    QuantLib::ext::shared_ptr<ore::data::EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<PairwiseVarSwapEngineBuilder> pairwiseVarSwapBuilder =
        QuantLib::ext::dynamic_pointer_cast<PairwiseVarSwapEngineBuilder>(builder);

    pairwiseVarSwap->setPricingEngine(pairwiseVarSwapBuilder->engine(
        name(0), name(1), ccy, laggedValuationSchedule.dates().back(), assetClassUnderlyings()));
    setSensitivityTemplate(*pairwiseVarSwapBuilder);

    // set up other Trade details
    instrument_ = QuantLib::ext::shared_ptr<ore::data::InstrumentWrapper>(new ore::data::VanillaInstrument(pairwiseVarSwap));

    npvCurrency_ = currency_;
    notionalCurrency_ = currency_;
    maturity_ = settlementDate;

    // add required fixings
    requiredFixings_.addFixingDates(valuationSchedule.dates(), indexNames_[0], maturity_);
    requiredFixings_.addFixingDates(laggedValuationSchedule.dates(), indexNames_[0], maturity_);
    requiredFixings_.addFixingDates(valuationSchedule.dates(), indexNames_[1], maturity_);
    requiredFixings_.addFixingDates(laggedValuationSchedule.dates(), indexNames_[1], maturity_);
}

void PairwiseVarSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* vNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    longShort_ = XMLUtils::getChildValue(vNode, "LongShort", true);

    XMLNode* underlyingsNode = XMLUtils::getChildNode(vNode, "Underlyings");
    QL_REQUIRE(underlyingsNode, "Trade " << id() << ": Must provide an Underlyings node");

    // We use "Value" sub-nodes here for backward compatibility with the scripted pairwise variance swap
    const string underlyingNodeName = "Value";
    auto underlyings = XMLUtils::getChildrenNodes(underlyingsNode, underlyingNodeName);
    QL_REQUIRE(underlyings.size() == 2,
               "Trade " << id() << ": Must provide two \"Value\" sub-nodes in the Underlyings node");

    // Build underlyings - not using the usual UnderlyingBuilder here again for compatibility with the scripted version
    string uVal, uName, acString;
    vector<AssetClass> assetClasses;
    AssetClass ac;
    for (auto const& u : underlyings) {
        uVal = XMLUtils::getNodeValue(u);
        if (boost::starts_with(uVal, "COMM-")) {
            ac = AssetClass::COM;
            acString = "Commodity";
            uName = uVal.substr(5);
        } else if (boost::starts_with(uVal, "EQ-")) {
            ac = AssetClass::EQ;
            acString = "Equity";
            uName = uVal.substr(3);
        } else if (boost::starts_with(uVal, "FX-")) {
            ac = AssetClass::FX;
            acString = "FX";
            uName = uVal.substr(3);
        } else {
            QL_FAIL("Unsupported underlying type for " << uVal);
        }

        indexNames_.push_back(uVal);
        QuantLib::ext::shared_ptr<ore::data::Underlying> underlying(new ore::data::Underlying(acString, uName, 1.0));
        assetClasses.push_back(ac);
        underlyings_.push_back(underlying);
    }

    QL_REQUIRE(assetClasses[0] == assetClasses[1],
               "Trade " << id() << ": Both underlyings must belong to the same asset class.");
    assetClassUnderlyings_ = assetClasses[0];

    underlyingStrikes_ = XMLUtils::getChildrenValuesAsDoubles(vNode, "UnderlyingStrikes", "Value", true);
    underlyingNotionals_ = XMLUtils::getChildrenValuesAsDoubles(vNode, "UnderlyingNotionals", "Value", true);
    basketNotional_ = XMLUtils::getChildValueAsDouble(vNode, "BasketNotional", true);
    basketStrike_ = XMLUtils::getChildValueAsDouble(vNode, "BasketStrike", true);
    settlementDate_ = XMLUtils::getChildValue(vNode, "SettlementDate", true);
    currency_ = XMLUtils::getChildValue(vNode, "PayCcy", true);

    // Optional parameters
    accrualLag_ = XMLUtils::getChildValueAsInt(vNode, "AccrualLag", false, 1);
    payoffLimit_ = XMLUtils::getChildValueAsDouble(vNode, "PayoffLimit", false, 0.0);
    cap_ = XMLUtils::getChildValueAsDouble(vNode, "Cap", false, 0.0);
    floor_ = XMLUtils::getChildValueAsDouble(vNode, "Floor", false, 0.0);

    XMLNode* valuationSchedule = XMLUtils::getChildNode(vNode, "ValuationSchedule");
    QL_REQUIRE(valuationSchedule, "Trade " << id() << ": Must provide a \"ValuationSchedule\" node");
    valuationSchedule_.fromXML(valuationSchedule);

    // If valuation schedule is derived (from lagged valuation schedule), the offset must be negative
    if (valuationSchedule_.hasDerived()) {
        for (const ScheduleDerived& s : laggedValuationSchedule_.derived()) {
            QL_REQUIRE(parsePeriod(s.shift()) <= -1 * Days,
                       "Trade " << id() << " Shift value for ValutionSchedule must be at least -1D or less");
        }
    }

    XMLNode* laggedValuationSchedule = XMLUtils::getChildNode(vNode, "LaggedValuationSchedule");
    if (laggedValuationSchedule) {
        laggedValuationSchedule_.fromXML(laggedValuationSchedule);
        // If lagged valuation schedule is derived (from valuation schedule), the offset must be positive
        if (laggedValuationSchedule_.hasDerived()) {
            for (const ScheduleDerived& s : laggedValuationSchedule_.derived()) {
                QL_REQUIRE(parsePeriod(s.shift()) >= 1 * Days,
                           "Trade " << id()
                                    << " Shift value for LaggedValuationSchedule must be at least 1D or greater");
            }
        }
    } else {
        // Defaulting will be dealt with later in PairwiseVarSwap::build()
        laggedValuationSchedule_ = ScheduleData();
    }
}

XMLNode* PairwiseVarSwap::toXML(ore::data::XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* vNode = doc.allocNode(tradeType() + "Data");

    XMLUtils::appendNode(node, vNode);
    XMLUtils::addChild(doc, vNode, "LongShort", longShort_);
    XMLNode* underlyingsNode = doc.allocNode("Underlyings");
    XMLNode* underlyingStrikesNode = doc.allocNode("UnderlyingStrikes");
    XMLNode* underlyingNotionalsNode = doc.allocNode("UnderlyingNotionals");
    for (int i = 0; i < 2; i++) {
        XMLUtils::addChild(doc, underlyingsNode, "Value", indexNames_[i]);
        XMLUtils::addChild(doc, underlyingStrikesNode, "Value", underlyingStrikes_[i]);
        XMLUtils::addChild(doc, underlyingNotionalsNode, "Value", underlyingNotionals_[i]);
    }
    XMLUtils::appendNode(vNode, underlyingsNode);
    XMLUtils::appendNode(vNode, underlyingStrikesNode);
    XMLUtils::appendNode(vNode, underlyingNotionalsNode);
    XMLUtils::addChild(doc, vNode, "BasketNotional", basketNotional_);
    XMLUtils::addChild(doc, vNode, "BasketStrike", basketStrike_);
    XMLNode* valuationSchedule = valuationSchedule_.toXML(doc);
    XMLUtils::appendNode(vNode, valuationSchedule);
    XMLUtils::setNodeName(doc, valuationSchedule, "ValuationSchedule");
    XMLNode* laggedValuationSchedule = laggedValuationSchedule_.toXML(doc);
    XMLUtils::appendNode(vNode, laggedValuationSchedule);
    XMLUtils::setNodeName(doc, laggedValuationSchedule, "LaggedValuationSchedule");
    XMLUtils::addChild(doc, vNode, "AccrualLag", accrualLag_);
    XMLUtils::addChild(doc, vNode, "PayoffLimit", payoffLimit_);
    XMLUtils::addChild(doc, vNode, "Cap", cap_);
    XMLUtils::addChild(doc, vNode, "Floor", floor_);
    XMLUtils::addChild(doc, vNode, "SettlementDate", settlementDate_);
    XMLUtils::addChild(doc, vNode, "PayCcy", currency_);

    return node;
}

std::map<AssetClass, std::set<std::string>>
EqPairwiseVarSwap::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::EQ, std::set<std::string>({name(0), name(1)})}};
}

} // namespace data
} // namespace ore
