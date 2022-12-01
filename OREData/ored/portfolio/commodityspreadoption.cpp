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
#include <ored/portfolio/builders/commodityspreadoption.hpp>

namespace ore::data{
enum CommoditySpreadOptionPosition{Begin = 0, Long = 0, Short = 1, End = 2};
void CommoditySpreadOption::build(const boost::shared_ptr<ore::data::EngineFactory>& engineFactory) {

    reset();
    npvCurrency_ = settlementCcy_;
    DLOG("CommoditySpreadOption::build() called for trade " << id());
    // build the relevant fxIndexes;
    std::vector<boost::shared_ptr<QuantExt::FxIndex>> FxIndex{nullptr, nullptr};
    commLegData_.resize(2);
    Currency ccy = parseCurrency(settlementCcy_);
    auto optionType = parseOptionType(callPut_);
    boost::shared_ptr<CommodityIndex> longIndex{nullptr}, shortIndex{nullptr};
    std::vector<bool> isAveraged{false, false};

    // init engine factory builder
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    auto engineBuilder = boost::dynamic_pointer_cast<CommoditySpreadOptionEngineBuilder>(builder);
    // get config
    auto config = builder->configuration(MarketContext::pricing);

    // Build the commodity legs
    for (int i = CommoditySpreadOptionPosition::Begin; i!=CommoditySpreadOptionPosition::End; i++) { // The order is important, the first leg is always the long position, the second is the short
        if(i == (int)CommoditySpreadOptionPosition::Long) {
            QL_REQUIRE(legData_[i].isPayer(), "CommoditySpreadOption: first leg is not relevant for long position.");
            legPayers_.push_back(true);
        }
        if(i == (int)CommoditySpreadOptionPosition::Short) {
            QL_REQUIRE(!legData_[i].isPayer(), "CommoditySpreadOption: second leg is not relevant for short position.");
            legPayers_.push_back(false);
        }

        // build legs
        auto conLegData = legData_[i].concreteLegData();
        commLegData_[i] = boost::dynamic_pointer_cast<CommodityFloatingLegData>(conLegData);
        QL_REQUIRE(commLegData_[i], "CommoditySpreadOption leg data should be of type CommodityFloating");
        isAveraged[i] = commLegData_[i]->isAveraged();
        auto legBuilder = engineFactory->legBuilder(legData_[i].legType());
        auto cflb = boost::dynamic_pointer_cast<CommodityFloatingLegBuilder>(legBuilder);
        QL_REQUIRE(cflb, "Expected a CommodityFloatingLegBuilder for leg type " << legData_[i].legType());
        Leg leg = cflb->buildLeg(legData_[i], engineFactory, requiredFixings_, config);

        // setup the cf indexes
        boost::shared_ptr<CommodityIndex> index{nullptr};
        if(isAveraged[i]) {
            index = boost::dynamic_pointer_cast<QuantExt::CommodityIndexedAverageCashFlow>(leg[0])->index();
        } else {
            index = boost::dynamic_pointer_cast<QuantExt::CommodityIndexedCashFlow>(leg[0])->index();
        }
        (i == (int)CommoditySpreadOptionPosition::Long)?longIndex=index:shortIndex=index;

        // check ccy consistency
        auto underlyingCcy = index->priceCurve()->currency();
        auto tmpFx = commLegData_[i]->fxIndex();
        if(tmpFx.empty()) {
            std::string pos = i == 0 ? "Long" : "Short";
            QL_REQUIRE(underlyingCcy.code() == settlementCcy_,
                       "CommoditySpreadOption: No FXIndex provided for the " << pos << "position");
        }else {
            auto domestic = npvCurrency_;
            auto foreign = underlyingCcy.code();
            FxIndex[i] = buildFxIndex(tmpFx, domestic, foreign, engineFactory->market(),
                                      engineFactory->configuration(MarketContext::pricing));
            // update required fixings. This is handled automatically in the leg builder in the averaging case
            if(!isAveraged[i]) {
                for (auto cf : leg) {
                    auto fixingDate = cf->date();
                    if (!FxIndex[i]->fixingCalendar().isBusinessDay(
                            fixingDate)) { // If fx index is not available for the cashflow pricing day,
                        // this ensures to require the previous valid one which will be used in pricing
                        // from fxIndex()->fixing(...)
                        Date adjustedFixingDate = FxIndex[i]->fixingCalendar().adjust(fixingDate, Preceding);
                        requiredFixings_.addFixingDate(adjustedFixingDate, tmpFx);
                    } else {
                        requiredFixings_.addFixingDate(fixingDate, tmpFx);
                    }
                }
            }
        }
        legs_.push_back(leg);
        legCurrencies_.push_back(npvCurrency_); // all legs and cf are priced with the same ccy
    }

    vector<boost::shared_ptr<Instrument>> additionalInstruments;
    //check cfs and build the instruments
    QL_REQUIRE(legs_[0].size() == legs_[1].size(), "CommoditySpreadOption: the two legs must contain the same number of cashflows.");
    for (size_t i =0; i<legs_[0].size();++i){ // I already checked the size is the same for both

        Real lq = (isAveraged[CommoditySpreadOptionPosition::Long])?
                boost::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(legs_[CommoditySpreadOptionPosition::Long][i])->quantity():
                boost::dynamic_pointer_cast<CommodityIndexedCashFlow>(legs_[CommoditySpreadOptionPosition::Long][i])->quantity();
        
        auto end_date_l = legs_[CommoditySpreadOptionPosition::Long][i]->date();

        Real sq = (isAveraged[CommoditySpreadOptionPosition::Short])?
                boost::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(legs_[CommoditySpreadOptionPosition::Short][i])->quantity():
                boost::dynamic_pointer_cast<CommodityIndexedCashFlow>(legs_[CommoditySpreadOptionPosition::Short][i])->quantity();

        auto end_date_s = legs_[CommoditySpreadOptionPosition::Short][i]->date();
       
        QL_REQUIRE(QuantLib::close_enough(lq - sq, 0.0), "CommoditySpreadOption: all cashflows must refer to the same quantity");
        QL_REQUIRE(end_date_l==end_date_s, "CommoditySpreadOption: the computation period for the two cashflows must be the same");
        quantity_ = lq;

        // set exercise date to the calculation period end
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(end_date_l);
        Date settlementDate{Date()};
        if(i==legs_[0].size()-1){ // if the last flow must be settled with delay, set it here
            settlementDate = settlementDate_.empty()?Date(): parseDate(settlementDate_);
        }
        if(settlementDate!=Date())
            QL_REQUIRE(settlementDate>=exercise->lastDate(), "CommoditySpreadOption: Trade cannot be settled before exercise");

        //maturity gets overwritten every time, and it is ok. If the last option is settled with delay, maturity is set to the settlement date.
        maturity_ = (settlementDate == Date())?exercise->lastDate() : settlementDate;

        // build the instrument for the i-th cfs
        auto spreadOption = boost::make_shared<QuantExt::CommoditySpreadOption>(
            boost::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(legs_[CommoditySpreadOptionPosition::Long][i]),
            boost::dynamic_pointer_cast<QuantExt::CommodityCashFlow>(legs_[CommoditySpreadOptionPosition::Short][i]),
            exercise, quantity_, spreadStrike_, optionType,
            settlementDate, FxIndex[CommoditySpreadOptionPosition::Long], FxIndex[CommoditySpreadOptionPosition::Short]);

        // build and assign the engine
        boost::shared_ptr<PricingEngine> engine = engineBuilder->engine(ccy, longIndex, shortIndex, id());
        spreadOption->setPricingEngine(engine);
        additionalInstruments.emplace_back(spreadOption);
    }

    // grep the last computation period instrument as main
    auto qlInstrument = additionalInstruments.back();
    additionalInstruments.pop_back(); // remove last instrument, it is already set as main one
    // set the trade instrument
    // as the first argument is always the long position and the second is always the short, multiplier is set to 1
    instrument_ = boost::make_shared<VanillaInstrument>(qlInstrument, 1.0, additionalInstruments, std::vector<Real>(legs_[0].size()-1,1.0));
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
    XMLUtils::addChild(doc, csoNode, "SpreadStrike", spreadStrike_);
    XMLUtils::addChild(doc, csoNode, "OptionType", callPut_);
    XMLUtils::addChild(doc, csoNode, "Currency", settlementCcy_);
    if(!settlementDate_.empty())
        XMLUtils::addChild(doc, csoNode, "SettlementDate", settlementDate_);
    return node;
}

}