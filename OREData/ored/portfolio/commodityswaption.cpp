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

#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/exercise.hpp>

#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/instruments/genericswaption.hpp>
#include <ored/portfolio/builders/commodityswap.hpp>
#include <ored/portfolio/builders/commodityswaption.hpp>
#include <ored/portfolio/commodityswaption.hpp>

#include <boost/algorithm/string/case_conv.hpp>

#include <algorithm>

using namespace QuantLib;
using std::lower_bound;
using std::sort;

namespace ore {
namespace data {

void CommoditySwaption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    reset();

    DLOG("CommoditySwaption::build() called for trade " << id());

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = std::string("Commodity");
    additionalData_["isdaBaseProduct"] = std::string("Other");
    additionalData_["isdaSubProduct"] = std::string("");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = std::string("");

    // Swaption details
    Settlement::Type settleType = parseSettlementType(option_.settlement());

    // Just set a consistent method here if it is left empty
    Settlement::Method settleMethod = Settlement::PhysicalOTC;
    if (option_.settlementMethod().empty()) {
        if (settleType == Settlement::Cash) {
            settleMethod = Settlement::CollateralizedCashPrice;
        }
    } else {
        settleMethod = parseSettlementMethod(option_.settlementMethod());
    }

    QL_REQUIRE(option_.exerciseDates().size() == 1, "Commodity swaption must be European");
    Date exDate = parseDate(option_.exerciseDates().front());
    QL_REQUIRE(exDate >= Settings::instance().evaluationDate(),
               "Exercise date, " << io::iso_date(exDate) << ", should be in the future relative to the valuation date "
                                 << io::iso_date(Settings::instance().evaluationDate()));

    // Build the underyling swap and check exercise date
    QuantLib::ext::shared_ptr<QuantLib::Swap> swap = buildSwap(engineFactory);
    QL_REQUIRE(exDate <= startDate_, "Expected the expiry date, " << io::iso_date(exDate)
                                                                  << " to be on or before the swap start date "
                                                                  << io::iso_date(startDate_));

    // Build the swaption
    exercise_ = QuantLib::ext::make_shared<EuropeanExercise>(exDate);
    QuantLib::ext::shared_ptr<QuantExt::GenericSwaption> swaption =
        QuantLib::ext::make_shared<QuantExt::GenericSwaption>(swap, exercise_, settleType, settleMethod);

    // Set the swaption's pricing engine
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QuantLib::ext::shared_ptr<CommoditySwaptionEngineBuilder> engineBuilder =
        QuantLib::ext::dynamic_pointer_cast<CommoditySwaptionEngineBuilder>(builder);
    auto configuration = builder->configuration(MarketContext::pricing);
    Currency currency = parseCurrency(ccy_);
    QuantLib::ext::shared_ptr<PricingEngine> engine = engineBuilder->engine(currency, name_);
    setSensitivityTemplate(*engineBuilder);
    swaption->setPricingEngine(engine);

    // Set the instrument wrapper properly
    Position::Type positionType = parsePositionType(option_.longShort());
    if (settleType == Settlement::Cash) {
        Real multiplier = positionType == Position::Long ? 1.0 : -1.0;
        instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(swaption, multiplier);
    } else {
        instrument_ = QuantLib::ext::make_shared<EuropeanOptionWrapper>(swaption, positionType == Position::Long, exDate,
                                                                settleType == Settlement::Physical, swap);
    }
    // use underlying maturity independent of settlement type, following ISDA GRID/AANA guidance
    maturity_ = swap->maturityDate();
}

QuantLib::Real CommoditySwaption::notional() const {
    return commoditySwap_->notional();
}

QuantLib::ext::shared_ptr<QuantLib::Swap> CommoditySwaption::buildSwap(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // Some checks to make sure the underlying swap is supported
    QL_REQUIRE(legData_.size() == 2, "Expected two commodity legs but found " << legData_.size());
    QL_REQUIRE(legData_[0].currency() == legData_[1].currency(), "Cross currency commodity swap not supported");
    QL_REQUIRE(legData_[0].isPayer() != legData_[1].isPayer(),
               "Both commodity legs are " << (legData_[0].isPayer() ? "paying" : "receiving"));

    QL_REQUIRE(legData_[0].legType() == "CommodityFixed" || legData_[0].legType() == "CommodityFloating",
               "Leg type needs to be CommodityFixed or CommodityFloating but 1st leg has type "
                   << legData_[0].legType());
    QL_REQUIRE(legData_[1].legType() == "CommodityFixed" || legData_[1].legType() == "CommodityFloating",
               "Leg type needs to be CommodityFixed or CommodityFloating but 2nd leg has type "
                   << legData_[1].legType());

    if (legData_[0].legType() == "CommodityFixed") {
        QL_REQUIRE(legData_[1].legType() == "CommodityFloating",
                   "1st leg is CommodityFixed so 2nd leg should be CommodityFloating but is " << legData_[1].legType());
        auto floatLeg = QuantLib::ext::dynamic_pointer_cast<CommodityFloatingLegData>(legData_[1].concreteLegData());
        name_ = floatLeg->name();
    } else {
        auto floatLeg = QuantLib::ext::dynamic_pointer_cast<CommodityFloatingLegData>(legData_[0].concreteLegData());
	QL_REQUIRE(floatLeg, "first leg has type " << legData_[0].legType() << ", expected CommodityFloating");
        name_ = floatLeg->name();
    }

    // Build the underlying commodity swap
    commoditySwap_ = QuantLib::ext::make_shared<CommoditySwap>(envelope(), legData_);
    commoditySwap_->build(engineFactory);

    // Get the QuantLib::Swap from the commodity swap
    auto qlInstrument = commoditySwap_->instrument()->qlInstrument();
    QuantLib::ext::shared_ptr<QuantLib::Swap> swap = QuantLib::ext::dynamic_pointer_cast<QuantLib::Swap>(qlInstrument);
    QL_REQUIRE(swap, "Expected an underlying swap instrument from CommoditySwap");

    // Populate relevant member variables, notonal and notional currency are set by the underlying Swap build already.
    startDate_ = swap->startDate();
    ccy_ = npvCurrency_ = legData_[0].currency();
    notional_ = Null<Real>();
    notionalCurrency_ = commoditySwap_->notionalCurrency();

    return swap;
}

std::map<AssetClass, std::set<std::string>>
CommoditySwaption::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {

    std::map<AssetClass, std::set<std::string>> result;

    for (auto ld : legData_) {
        set<string> indices = ld.indices();
        for (auto ind : indices) {
            QuantLib::ext::shared_ptr<Index> index = parseIndex(ind);
            // only handle commodity
            if (auto ci = QuantLib::ext::dynamic_pointer_cast<QuantExt::CommodityIndex>(index)) {
                result[AssetClass::COM].insert(ci->name());
            }
        }
    }
    return result;
}

void CommoditySwaption::fromXML(XMLNode* node) {

    Trade::fromXML(node);

    XMLNode* swapNode = XMLUtils::getChildNode(node, "CommoditySwaptionData");
    QL_REQUIRE(swapNode, "No CommoditySwaptionData node");

    // Get the option data
    option_.fromXML(XMLUtils::getChildNode(swapNode, "OptionData"));

    // Get the leg data i.e. the leg data describing the underlying swap
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    QL_REQUIRE(nodes.size() == 2, "Two commodity swap legs expected, found " << nodes.size());
    legData_.clear();
    for (Size i = 0; i < nodes.size(); i++) {
        auto ld = createLegData();
        ld->fromXML(nodes[i]);
        legData_.push_back(*ld);
    }
}

XMLNode* CommoditySwaption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);

    // Add the root CommoditySwaptionData node
    XMLNode* swaptionNode = doc.allocNode("CommoditySwaptionData");
    XMLUtils::appendNode(node, swaptionNode);

    // Add the OptionData node
    XMLUtils::appendNode(swaptionNode, option_.toXML(doc));

    // Add the LegData nodes
    for (Size i = 0; i < legData_.size(); i++)
        XMLUtils::appendNode(swaptionNode, legData_[i].toXML(doc));

    return node;
}

} // namespace data
} // namespace ore
