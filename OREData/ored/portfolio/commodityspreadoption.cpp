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
#include <qle/instruments/commodityspreadoption.hpp>
#include <ored/portfolio/commodityspreadoption.hpp>
#include <ored/portfolio/commoditylegbuilder.hpp>
#include <ored/utilities/marketdata.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/indexes/fxindex.hpp>

namespace ore::data{
enum CommoditySpreadOptionPosition{Long = 0, Short = 1};
void CommoditySpreadOption::build(const boost::shared_ptr<ore::data::EngineFactory>& engineFactory) {
    reset();
    DLOG("CommoditySpreadOption::build() called for trade " << id());
    // build the relevant fxIndexes;
    std::vector<boost::shared_ptr<QuantExt::FxIndex>> FxIndex{nullptr, nullptr};
    commLegData_.resize(2);
    // Build the commodity legs
    for (size_t i = 0; i<legData_.size(); ++i) { // The order is important, the first leg is always the long position, the second is the short
        auto conLegData = legData_[i].concreteLegData();
        commLegData_[i] = boost::dynamic_pointer_cast<CommodityFloatingLegData>(conLegData);
        QL_REQUIRE(commLegData_[i], "CommoditySpreadOption leg data should be of type CommodityFloating");
        auto legBuilder = engineFactory->legBuilder(legData_[i].legType());
        auto cflb = boost::dynamic_pointer_cast<CommodityFloatingLegBuilder>(legBuilder);
        QL_REQUIRE(cflb, "Expected a CommodityFloatingLegBuilder for leg type " << legData_[i].legType());
        Leg leg = cflb->buildLeg(legData_[i], engineFactory, requiredFixings_, "");
        QL_REQUIRE(leg.size() == 1, "CommoditySpreadOption is currently implemented for exactly one pair of contracts.");
        auto underlyingCcy =
            boost::dynamic_pointer_cast<QuantExt::CommodityIndexedCashFlow>(leg[0])->index()->priceCurve()->currency();
        auto tmpFx = commLegData_[i]->fxIndex();
        if(tmpFx.empty()) {
            std::string pos = i == 0 ? "Long" : "Short";
            QL_REQUIRE(underlyingCcy.code() == settlementCcy_,
                       "CommoditySpreadOption: No FXIndex provided for the " << pos << "position");
        }else{
            FxIndex[i] = buildFxIndex(tmpFx, npvCurrency_, underlyingCcy.code(), engineFactory->market(),
                           engineFactory->configuration(MarketContext::pricing));
        }
        legs_.push_back(leg);

    }
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(parseDate(exerciseDate_));
    Date settlementDate = settlementDate_.empty()?Date(): parseDate(settlementDate_);
    auto spreadOption = boost::make_shared<QuantExt::CommoditySpreadOption>(
        boost::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(legs_[CommoditySpreadOptionPosition::Long][0]),
        boost::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(legs_[CommoditySpreadOptionPosition::Short][0]),
        exercise, quantity_, spreadStrike_, parseOptionType(callPut_),
        settlementDate, FxIndex[CommoditySpreadOptionPosition::Long], FxIndex[CommoditySpreadOptionPosition::Short]);
    instrument_ = boost::make_shared<VanillaInstrument>(spreadOption);
}

void CommoditySpreadOption::fromXML(XMLNode* node){
    Trade::fromXML(node);

    XMLNode* csoNode = XMLUtils::getChildNode(node, "CommoditySpreadOptionData");
    QL_REQUIRE(csoNode, "No CommoditySpreadOptionData Node");
    legData_.resize(2); // two legs expected
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(csoNode, "LegData");
    QL_REQUIRE(nodes.size() == 2, "CommoditySpreadOption: Exactly two LegData nodes expected");
    for (auto & node : nodes) {
        auto ld = createLegData();
        ld->fromXML(node);
        legData_[!ld->isPayer()] = (*ld); // isPayer true = long position -> 0th element
    }
    QL_REQUIRE(legData_[CommoditySpreadOptionPosition::Long].isPayer() !=
               legData_[CommoditySpreadOptionPosition::Short].isPayer(), "CommoditySpreadOption: both a long and a short Assets are required.");
    exerciseDate_ = XMLUtils::getChildValue(csoNode, "ExerciseDate", true);
    quantity_ = XMLUtils::getChildValueAsDouble(csoNode, "Quantity", false,1.);
    spreadStrike_ = XMLUtils::getChildValueAsDouble(csoNode, "SpreadStrike", true);
    callPut_ = XMLUtils::getChildValue(csoNode, "OptionType", false, "Call");
    settlementDate_ = XMLUtils::getChildValue(csoNode, "SettlementDate",false,"");
    settlementCcy_ = XMLUtils::getChildValue(csoNode, "Currency", true);
}

XMLNode* CommoditySpreadOption::toXML(XMLDocument& doc){
    XMLNode* node = Trade::toXML(doc);

    XMLNode* csoNode = doc.allocNode("CommoditySpreadOptionData");
    XMLUtils::appendNode(node, csoNode);
    for (size_t i = 0; i < legData_.size(); ++i) {
        XMLUtils::appendNode(csoNode, legData_[i].toXML(doc));
    }
//    XMLUtils::appendNode(node, )
    return node;
}

}