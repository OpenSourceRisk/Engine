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

void CommoditySwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    reset();

    LOG("CommoditySwap::build() called for trade " << id());

    // ISDA taxonomy, assuming Commodity follows the Equity template
    additionalData_["isdaAssetClass"] = string("Commodity");
    additionalData_["isdaBaseProduct"] = string("Swap");
    additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");  

    check();

    // Arbitrarily choose NPV currency from 1st leg. Already checked that both leg currencies equal.
    npvCurrency_ = legData_[0].currency();

    // Set notional to N/A for now, but reset this for a commodity fixed respectively floating leg below.
    notional_ = Null<Real>();
    notionalCurrency_ = legData_[0].currency();

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("CommoditySwap");
    QuantLib::ext::shared_ptr<CommoditySwapEngineBuilder> engineBuilder =
        QuantLib::ext::dynamic_pointer_cast<CommoditySwapEngineBuilder>(builder);
    const string& configuration = builder->configuration(MarketContext::pricing);

    // Build the commodity swap legs
    
    // Build the floating legs first in case we need the quantities to build the fixed legs.
    // Store the floating legs in the map with their "Tag" as key. This allows the fixed leg to find the floating leg 
    // with the matching "Tag" when retrieving the quantities if it needs them. Note the if all the tags are empty, 
    // the map entry gets overwritten and the fixed leg with empty tag matches a random floating leg with empty tag. 
    // This is by design i.e. use tags if you want to link specific legs.
    map<string, Leg> floatingLegs;
    vector<Size> legsIdx;
    for (Size t = 0; t < legData_.size(); t++) {
        const auto& legDatum = legData_.at(t);

        const string& type = legDatum.legType();
        if (type == "CommodityFixed")
            continue;

        // Build the leg and add it to legs_
        buildLeg(engineFactory, legDatum, configuration);
        legsIdx.push_back(t);

        // Only add to map if CommodityFloatingLegData
        if (auto cfld = QuantLib::ext::dynamic_pointer_cast<CommodityFloatingLegData>(legDatum.concreteLegData())) {
            floatingLegs[cfld->tag()] = legs_.back();
        }
    }

    // Build any fixed legs skipped above.
    for (Size t = 0; t < legData_.size();  t++) {
        const auto& legDatum = legData_.at(t);

        // take a copy, since we might modify the leg datum below
        auto effLegDatum = legDatum;

        const string& type = effLegDatum.legType();
        if (type != "CommodityFixed")
            continue;

        // Update the commodity fixed leg quantities if necessary.
        auto cfld = QuantLib::ext::dynamic_pointer_cast<CommodityFixedLegData>(effLegDatum.concreteLegData());
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
                if (auto cicf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(ucf)) {
                    quantities.push_back(cicf->periodQuantity());
                } else if (auto ciacf = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(ucf)) {
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
        legsIdx.push_back(t);
    }

    // Reposition the leg-based data to match the original order according to legData_
    vector<Leg> legsTmp;
    vector<bool> legPayersTmp;
    vector<string> legCurrenciesTmp;
    for (const Size idx : legsIdx) {
        legsTmp.push_back(legs_.at(idx));
        legPayersTmp.push_back(legPayers_.at(idx));
        legCurrenciesTmp.push_back(legCurrencies_.at(idx));
    }
    legs_ = legsTmp;
    legPayers_ = legPayersTmp;
    legCurrencies_ = legCurrenciesTmp;

    // Create the QuantLib swap instrument and assign pricing engine
    auto swap = QuantLib::ext::make_shared<QuantLib::Swap>(legs_, legPayers_);
    QuantLib::ext::shared_ptr<PricingEngine> engine = engineBuilder->engine(parseCurrency(npvCurrency_));
    swap->setPricingEngine(engine);
    setSensitivityTemplate(*engineBuilder);
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(swap);
}

const std::map<std::string,boost::any>& CommoditySwap::additionalData() const {
    Size numLegs = legData_.size();
    // use the build time as of date to determine current notionals
    Date asof = Settings::instance().evaluationDate();
    QuantLib::ext::shared_ptr<QuantLib::Swap> swap = QuantLib::ext::dynamic_pointer_cast<QuantLib::Swap>(instrument_->qlInstrument());
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
            QuantLib::ext::shared_ptr<CashFlow> flow = legs_[i][j];
            if (flow->date() > asof) {
                std::string label = legID + ":" + std::to_string(j + 1);
                // CommodityFloatingLeg consists of indexed or indexed average cash flows
                QuantLib::ext::shared_ptr<CommodityIndexedCashFlow> indexedFlow =
                    QuantLib::ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(unpackIndexWrappedCashFlow(flow));
                if (indexedFlow) {
                    additionalData_["quantity[" + label + "]"] = indexedFlow->quantity();
                    additionalData_["periodQuantity[" + label + "]"] = indexedFlow->periodQuantity();
                    additionalData_["gearing[" + label + "]"] = indexedFlow->gearing();
                    additionalData_["spread[" + label + "]"] = indexedFlow->spread();
                    if (indexedFlow->isAveragingFrontMonthCashflow(asof)) {
                        std::vector<Real> priceVec;
                        std::vector<std::string> indexVec;
                        std::vector<Date> indexExpiryVec, pricingDateVec;
                        double averagePrice = 0;
                        for (const auto& pd : indexedFlow->spotAveragingPricingDates()) {
                            if (pd > asof) {
                                auto index = indexedFlow->index();
                                auto pricingDate = indexedFlow->lastPricingDate();
                                indexVec.push_back(index->name());
                                indexExpiryVec.push_back(index->expiryDate());
                                pricingDateVec.push_back(pd);
                                priceVec.push_back(index->fixing(pricingDate));
                                averagePrice += priceVec.back();
                            }
                            else {
                                auto index = indexedFlow->spotIndex();
                                indexVec.push_back(index->name());
                                indexExpiryVec.push_back(index->expiryDate());
                                pricingDateVec.push_back(pd);
                                priceVec.push_back(index->fixing(pd));
                                averagePrice += priceVec.back();
                            }
                        }
                        averagePrice /= indexedFlow->spotAveragingPricingDates().size();
                        additionalData_["index[" + label + "]"] = indexVec;
                        additionalData_["indexExpiry[" + label + "]"] = indexExpiryVec;
                        additionalData_["price[" + label + "]"] = priceVec;
                        additionalData_["averagePrice[" + label + "]"] = averagePrice;
                        additionalData_["pricingDate[" + label + "]"] = pricingDateVec;
                    }
                    else {
                    additionalData_["index[" + label + "]"] = indexedFlow->index()->name();
                    additionalData_["indexExpiry[" + label + "]"] = indexedFlow->index()->expiryDate();
                    additionalData_["price[" + label + "]"] = indexedFlow->index()->fixing(indexedFlow->pricingDate());
                    additionalData_["pricingDate[" + label + "]"] = to_string(indexedFlow->pricingDate());
                    }
                    additionalData_["paymentDate[" + label + "]"] = to_string(indexedFlow->date());
                }
                QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow> indexedAvgFlow =
                    QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(unpackIndexWrappedCashFlow(flow));
                if (indexedAvgFlow) {
                    additionalData_["quantity[" + label + "]"] = indexedAvgFlow->quantity();
                    additionalData_["periodQuantity[" + label + "]"] = indexedAvgFlow->periodQuantity();
                    additionalData_["gearing[" + label + "]"] = indexedAvgFlow->gearing();
                    additionalData_["spread[" + label + "]"] = indexedAvgFlow->spread();
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
            QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(legs_[i][0]);
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
    for (Size i = 0; i < legs_.size(); ++i) {
        for (Size j = 0; j < legs_[i].size(); ++j) {
            QuantLib::ext::shared_ptr<CashFlow> flow = legs_[i][j];
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
CommoditySwap::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {

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

XMLNode* CommoditySwap::toXML(XMLDocument& doc) const {
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

void CommoditySwap::buildLeg(const QuantLib::ext::shared_ptr<EngineFactory>& ef,
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
