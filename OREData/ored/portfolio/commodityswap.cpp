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

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/portfolio/builders/commodityswap.hpp>
#include <ored/portfolio/commoditylegdata.hpp>
#include <ored/portfolio/commodityswap.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/indexes/commodityindex.hpp>

using namespace ore::data;
using namespace QuantExt;
using namespace QuantLib;
using std::max;

namespace ore {
namespace data {

void CommoditySwap::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    reset();

    LOG("CommoditySwap::build() called for trade " << id());
    check();

    const boost::shared_ptr<Market> market = engineFactory->market();
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("CommoditySwap");
    boost::shared_ptr<CommoditySwapEngineBuilder> engineBuilder =
        boost::dynamic_pointer_cast<CommoditySwapEngineBuilder>(builder);
    const string& configuration = builder->configuration(MarketContext::pricing);

    // Arbitrarily choose NPV currency from 1st leg. Already checked that both leg currencies equal.
    npvCurrency_ = legData_[0].currency();

    // Set notional to N/A for now, but reset this for a commodity fixed respectively floating leg below.
    notional_ = Null<Real>();
    notionalCurrency_ = legData_[0].currency();

    // Build the commodity swap legs
    
    // Build the floating legs first in case we need the quantities to build the fixed legs.
    // Store the floating legs in the map with their "Tag" as key. This allows the fixed leg to find the floating leg 
    // with the matching "Tag" when retrieving the quantities if it needs them. Note the if all the tags are empty, 
    // the map entry gets overwritten and the fixed leg with empty tag matches a random floating leg with empty tag. 
    // This is by design i.e. use tags if you want to link specific legs.
    map<string, Leg> floatingLegs;
    for (const auto& legDatum : legData_) {

        const string& type = legDatum.legType();
        if (type == "CommodityFixed")
            continue;

        // Build the leg and add it to legs_
        buildLeg(engineFactory, legDatum, configuration);

        // Only add to map if CommodityFloatingLegData
        if (auto cfld = boost::dynamic_pointer_cast<CommodityFloatingLegData>(legDatum.concreteLegData())) {
            floatingLegs[cfld->tag()] = legs_.back();
        }
    }

    // Build any fixed legs skipped above.
    for (const auto& legDatum : legData_) {

        // take a copy, since we might modify the leg datum below
        auto effLegDatum = legDatum;

        const string& type = effLegDatum.legType();
        if (type != "CommodityFixed")
            continue;

        // Update the commodity fixed leg quantities if necessary.
        auto cfld = boost::dynamic_pointer_cast<CommodityFixedLegData>(effLegDatum.concreteLegData());
        QL_REQUIRE(cfld, "CommodityFixed leg should have valid CommodityFixedLegData");
        if (cfld->quantities().empty()) {

            auto it = floatingLegs.find(cfld->tag());
            QL_REQUIRE(it != floatingLegs.end(), "Did not find a commodity floating leg corresponding to the" <<
                " fixed leg with tag '" << cfld->tag() << "' from which to take the quantities.");

            const Leg& leg = it->second;
            vector<Real> quantities;
            quantities.reserve(leg.size());
            for (const auto& cf : leg) {
                // If more options arise here, may think of visitor to get the commodity notional.
                auto ucf = unpackIndexWrappedCashFlow(cf);
                if (auto cicf = boost::dynamic_pointer_cast<CommodityIndexedCashFlow>(ucf)) {
                    quantities.push_back(cicf->periodQuantity());
                } else if (auto ciacf = boost::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(ucf)) {
                    quantities.push_back(ciacf->periodQuantity());
                } else {
                    QL_FAIL("Expected a commodity indexed cashflow while building commodity fixed" <<
                        " leg quantities for trade " << id() << ".");
                }
            }

            cfld->setQuantities(quantities);
        }
        // overwrite payment dates if pay relative to future expiry of floating leg is specified
        if (effLegDatum.paymentDates().empty() &&
            cfld->commodityPayRelativeTo() == CommodityPayRelativeTo::FutureExpiryDate) {
            auto it = floatingLegs.find(cfld->tag());
            QL_REQUIRE(it != floatingLegs.end(),
                       "Did not find a commodity floating leg correpsonding to the fixed leg with tag '"
                           << cfld->tag() << "' from which to take the payment dates.");
            vector<string> tmp;
            for(auto const& cf: it->second) {
                tmp.push_back(ore::data::to_string(cf->date()));
            }
            effLegDatum.paymentDates() = tmp;
        }

        // Build the leg and add it to legs_
        buildLeg(engineFactory, effLegDatum, configuration);
    }


    // Create the QuantLib swap instrument and assign pricing engine
    auto swap = boost::make_shared<QuantLib::Swap>(legs_, legPayers_);
    boost::shared_ptr<PricingEngine> engine = engineBuilder->engine(parseCurrency(npvCurrency_));
    swap->setPricingEngine(engine);
    instrument_ = boost::make_shared<VanillaInstrument>(swap);
}

const std::map<std::string,boost::any>& CommoditySwap::additionalData() const {
    Size numLegs = legData_.size();
    // use the build time as of date to determine current notionals
    Date asof = Settings::instance().evaluationDate();
    boost::shared_ptr<QuantLib::Swap> swap = boost::dynamic_pointer_cast<QuantLib::Swap>(instrument_->qlInstrument());
    for (Size i = 0; i < numLegs; ++i) {
        string legID = to_string(i+1);
        additionalData_["legType[" + legID + "]"] = legData_[i].legType();
        additionalData_["isPayer[" + legID + "]"] = legData_[i].isPayer();
        additionalData_["currency[" + legID + "]"] = legData_[i].currency();
        if (swap)
            additionalData_["legNPV[" + legID + "]"] = swap->legNPV(i);
        else
            ALOG("commodity swap underlying instrument not set, skip leg npv reporting");
        for (Size j = 0; j < legs_[i].size(); ++j) {
            boost::shared_ptr<CashFlow> flow = legs_[i][j];
            if (flow->date() > asof) {
                std::string label = legID + ":" + std::to_string(j + 1);
                // CommodityFloatingLeg consists of indexed or indexed average cash flows
                boost::shared_ptr<CommodityIndexedCashFlow> indexedFlow =
                    boost::dynamic_pointer_cast<CommodityIndexedCashFlow>(unpackIndexWrappedCashFlow(flow));
                if (indexedFlow) {
                    additionalData_["index[" + label + "]"] = indexedFlow->index()->name();
                    additionalData_["indexExpiry[" + label + "]"] = indexedFlow->index()->expiryDate();
                    additionalData_["quantity[" + label + "]"] = indexedFlow->quantity();
                    additionalData_["periodQuantity[" + label + "]"] = indexedFlow->periodQuantity();
                    additionalData_["gearing[" + label + "]"] = indexedFlow->gearing();
                    additionalData_["price[" + label + "]"] = indexedFlow->index()->fixing(indexedFlow->pricingDate());
                    additionalData_["pricingDate[" + label + "]"] = to_string(indexedFlow->pricingDate());
                    additionalData_["paymentDate[" + label + "]"] = to_string(indexedFlow->date());
                }
                boost::shared_ptr<CommodityIndexedAverageCashFlow> indexedAvgFlow =
                    boost::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(unpackIndexWrappedCashFlow(flow));
                if (indexedAvgFlow) {
                    additionalData_["quantity[" + label + "]"] = indexedAvgFlow->quantity();
                    additionalData_["periodQuantity[" + label + "]"] = indexedAvgFlow->periodQuantity();
                    additionalData_["gearing[" + label + "]"] = indexedAvgFlow->gearing();
                    std::vector<Real> priceVec;
                    std::vector<std::string> indexVec;
                    std::vector<Date> indexExpiryVec, pricingDateVec;
                    double averagePrice = 0;
                    for (const auto& kv : indexedAvgFlow->indices()) {
                        indexVec.push_back(kv.second->name());
                        indexExpiryVec.push_back(kv.second->expiryDate());
                        pricingDateVec.push_back(kv.first);
                        priceVec.push_back(kv.second->fixing(kv.first));
                        averagePrice += priceVec.back();
                    }
                    averagePrice /= indexedAvgFlow->indices().size();
                    additionalData_["index[" + label + "]"] = indexVec;
                    additionalData_["indexExpiry[" + label + "]"] = indexExpiryVec;
                    additionalData_["price[" + label + "]"] = priceVec;
                    additionalData_["averagePrice[" + label + "]"] = averagePrice;
                    additionalData_["pricingDate[" + label + "]"] = pricingDateVec;
                    additionalData_["paymentDate[" + label + "]"] = to_string(indexedAvgFlow->date());
                }
                // CommodityFixedLeg consists of simple cash flows
                Real flowAmount = 0.0;
                try {
                    flowAmount = flow->amount();
                } catch (std::exception& e) {
                    ALOG("flow amount could not be determined for trade " << id() << ", set to zero: " << e.what());
                }
                additionalData_["amount[" + label + "]"] = flowAmount;
                additionalData_["paymentDate[" + label + "]"] = to_string(flow->date());
            }
        }
        if (legs_[i].size() > 0) {
            boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(legs_[i][0]);
            if (coupon) {
                Real originalNotional = 0.0;
                try { originalNotional = coupon->nominal(); }
                catch(std::exception& e) {
                    ALOG("original nominal could not be determined for trade " << id() << ", set to zero: " << e.what());
                }
                additionalData_["originalNotional[" + legID + "]"] = originalNotional;
            }
        }
    }
    return additionalData_;
}
    
QuantLib::Real CommoditySwap::notional() const {
    Date asof = Settings::instance().evaluationDate();
    Real currentAmount = Null<Real>();
    // Get maximum current cash flow amount (quantity * strike, quantity * spot/forward price) across legs
    // include gearings and spreads; note that the swap is in a single currency.
    for (Size i = 0; i < legData_.size(); ++i) {
        for (Size j = 0; j < legs_[i].size(); ++j) {
            boost::shared_ptr<CashFlow> flow = legs_[i][j];
            // pick flow with earliest payment date on this leg
            if (flow->date() > asof) {
                if (currentAmount == Null<Real>())
                    currentAmount = flow->amount();
                else // set on a previous leg already, set to maximum
                    currentAmount = std::max(currentAmount, flow->amount());
                break; // move on to the next leg
            }
        }
    }

    if (currentAmount != Null<Real>()) {
        return currentAmount;
    } else {
        ALOG("Error retrieving current notional for commodity swap " << id() << " as of " << io::iso_date(asof));
        return Null<Real>();
    }
}

std::map<AssetClass, std::set<std::string>>
CommoditySwap::underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager) const {

    std::map<AssetClass, std::set<std::string>> result;

    for (auto ld : legData_) {
        set<string> indices = ld.indices();
        for (auto ind : indices) {
            boost::shared_ptr<Index> index = parseIndex(ind);
            // only handle commodity
            if (auto ci = boost::dynamic_pointer_cast<QuantExt::CommodityIndex>(index)) {
                result[AssetClass::COM].insert(ci->name());
            }
        }
    }
    return result;
}

void CommoditySwap::fromXML(XMLNode* node) {
    DLOG("CommoditySwap::fromXML called");
    Trade::fromXML(node);
    legData_.clear();

    XMLNode* swapNode = XMLUtils::getChildNode(node, "SwapData");
    QL_REQUIRE(swapNode, "No SwapData Node");

    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    for (Size i = 0; i < nodes.size(); i++) {
        auto ld = createLegData();
        ld->fromXML(nodes[i]);
        legData_.push_back(*ld);
    }
}

XMLNode* CommoditySwap::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* swapNode = doc.allocNode("SwapData");
    XMLUtils::appendNode(node, swapNode);
    for (Size i = 0; i < legData_.size(); i++)
        XMLUtils::appendNode(swapNode, legData_[i].toXML(doc));
    return node;
}

void CommoditySwap::check() const {
    QL_REQUIRE(legData_.size() >= 2, "Expected at least two commodity legs but found " << legData_.size());
    auto ccy = legData_[0].currency();
    for (const auto& legDatum : legData_) {
        QL_REQUIRE(legDatum.currency() == ccy, "Cross currency commodity swaps are not supported");
    }
}

void CommoditySwap::buildLeg(const boost::shared_ptr<EngineFactory>& ef,
    const LegData& legDatum, const string& configuration) {

    auto legBuilder = ef->legBuilder(legDatum.legType());
    Leg leg = legBuilder->buildLeg(legDatum, ef, requiredFixings_, configuration);
    legs_.push_back(leg);
    legPayers_.push_back(legDatum.isPayer());
    legCurrencies_.push_back(legDatum.currency());

    // Update maturity
    maturity_ = max(CashFlows::maturityDate(leg), maturity_);

}

} // namespace data
} // namespace ore
