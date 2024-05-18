/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ored/portfolio/failedtrade.hpp>
#include <ored/portfolio/fxforward.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <ql/errors.hpp>
#include <ql/time/date.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

using namespace data;

void Portfolio::clear() {
    trades_.clear();
    underlyingIndicesCache_.clear();
}

void Portfolio::reset() {
    LOG("Reset portfolio of size " << trades_.size());
    for (auto [id, t] : trades_)
        t->reset();
}

void Portfolio::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Portfolio");
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(node, "Trade");
    for (Size i = 0; i < nodes.size(); i++) {
        string tradeType = XMLUtils::getChildValue(nodes[i], "TradeType", true);

        // Get the id attribute
        string id = XMLUtils::getAttribute(nodes[i], "id");
        QL_REQUIRE(id != "", "No id attribute in Trade Node");
        DLOG("Parsing trade id:" << id);
        
        QuantLib::ext::shared_ptr<Trade> trade;
        bool failedToLoad = true;
        try {
            trade = TradeFactory::instance().build(tradeType);
            trade->fromXML(nodes[i]);
            trade->id() = id;
            add(trade);
            DLOG("Added Trade " << id << " (" << trade->id() << ")"
                                << " type:" << tradeType);
            failedToLoad = false;
        } catch (std::exception& ex) {
            StructuredTradeErrorMessage(id, tradeType, "Error parsing Trade XML", ex.what()).log();
        }

        // If trade loading failed, then insert a dummy trade with same id and envelope
        if (failedToLoad && buildFailedTrades_) {
            try {
                trade = TradeFactory::instance().build("Failed");
                // this loads only type, id and envelope, but type will be set to the original trade's type
                trade->fromXML(nodes[i]);
                // create a dummy trade of type "Dummy"
                QuantLib::ext::shared_ptr<FailedTrade> failedTrade = QuantLib::ext::make_shared<FailedTrade>();
                // copy id and envelope
                failedTrade->id() = id;
                failedTrade->setUnderlyingTradeType(tradeType);
                failedTrade->setEnvelope(trade->envelope());
                // and add it to the portfolio
                add(failedTrade);
                WLOG("Added trade id " << failedTrade->id() << " type " << failedTrade->tradeType()
                                       << " for original trade type " << trade->tradeType());
            } catch (std::exception& ex) {
                StructuredTradeErrorMessage(id, tradeType, "Error parsing type and envelope", ex.what()).log();
            }
        }
    }
    LOG("Finished Parsing XML doc");
}

XMLNode* Portfolio::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Portfolio");
    for (auto& t : trades_)
        XMLUtils::appendNode(node, t.second->toXML(doc));
    return node;
}

bool Portfolio::remove(const std::string& tradeID) {
    underlyingIndicesCache_.clear();
    return trades_.erase(tradeID) > 0;
}

void Portfolio::removeMatured(const Date& asof) {
    for (auto it = trades_.begin(); it != trades_.end(); /* manual */) {
        if ((*it).second->isExpired(asof)) {
            StructuredTradeWarningMessage((*it).second, "", "Trade is Matured").log();
            it=trades_.erase(it);
        } else {
            ++it;
        }
    }
}

void Portfolio::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const std::string& context,
                      const bool emitStructuredError) {
    LOG("Building Portfolio of size " << trades_.size() << " for context = '" << context << "'");
    auto trade = trades_.begin();
    Size initialSize = trades_.size();
    Size failedTrades = 0;
    while (trade != trades_.end()) {
        auto [ft, success] = buildTrade((*trade).second, engineFactory, context, ignoreTradeBuildFail(),
                                        buildFailedTrades(), emitStructuredError);
        if (success) {
            ++trade;
        } else if (ft) {
            (*trade).second = ft;
            ++failedTrades;
            ++trade;
        } else {
            trade = trades_.erase(trade);
        }
    }
    LOG("Built Portfolio. Initial size = " << initialSize << ", size now " << trades_.size() << ", built "
                                           << failedTrades << " failed trades, context is " + context);

    QL_REQUIRE(trades_.size() > 0, "Portfolio does not contain any built trades, context is '" + context + "'");
}

Date Portfolio::maturity() const {
    QL_REQUIRE(trades_.size() > 0, "Cannot get maturity of an empty portfolio");
    Date mat = Date::minDate();
    for (const auto& t : trades_)
        mat = std::max(mat, t.second->maturity());
    return mat;
}

set<string> Portfolio::ids() const {
    set<string> ids;
    for (const auto& [tradeId, _] : trades_)
        ids.insert(tradeId);
    return ids;
}

const std::map<std::string, QuantLib::ext::shared_ptr<Trade>>& Portfolio::trades() const { return trades_; }

map<string, string> Portfolio::nettingSetMap() const {
    map<string, string> nettingSetMap;
    for (const auto& t : trades_)
        nettingSetMap[t.second->id()] = t.second->envelope().nettingSetId();
    return nettingSetMap;
}

std::set<std::string> Portfolio::counterparties() const {
    set<string> counterparties;
    for (const auto& t : trades_)
        counterparties.insert(t.second->envelope().counterparty());
    return counterparties;
}

map<string, set<string>> Portfolio::counterpartyNettingSets() const {
    map<string, set<string>> cpNettingSets;
    for (const auto& [tradeId, trade] : trades()) {
        cpNettingSets[trade->envelope().counterparty()].insert(trade->envelope().nettingSetId());
    }
    return cpNettingSets;
}

void Portfolio::add(const QuantLib::ext::shared_ptr<Trade>& trade) {
    QL_REQUIRE(!has(trade->id()), "Attempted to add a trade to the portfolio with an id, which already exists.");
    underlyingIndicesCache_.clear();
    trades_[trade->id()] = trade;
}

bool Portfolio::has(const string& id) { return trades_.find(id) != trades_.end(); }

QuantLib::ext::shared_ptr<Trade> Portfolio::get(const string& id) const {
    auto it = trades_.find(id);
    if (it != trades_.end())
        return it->second;
    else
        return nullptr;
}

std::set<std::string> Portfolio::portfolioIds() const {
    std::set<std::string> portfolioIds;
    for (const auto& [tradeId, trade] : trades()) {
        portfolioIds.insert(trade->portfolioIds().begin(), trade->portfolioIds().end());
    }
    return portfolioIds;
}

bool Portfolio::hasNettingSetDetails() const {
    bool hasNettingSetDetails = false;
    for (const auto& t : trades_)
        if (!t.second->envelope().nettingSetDetails().emptyOptionalFields()) {
            hasNettingSetDetails = true;
            break;
        }
    return hasNettingSetDetails;
}

map<string, RequiredFixings::FixingDates> Portfolio::fixings(const Date& settlementDate) const {
    map<string, RequiredFixings::FixingDates> result;
    for (const auto& t : trades_) {
        auto fixings = t.second->fixings(settlementDate);
        for (const auto& [index, fixingDates] : fixings) {
            if (!fixingDates.empty()) {
                result[index].addDates(fixingDates);
            }
        }
    }
    return result;
}

std::map<AssetClass, std::set<std::string>>
Portfolio::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) {

    if (!underlyingIndicesCache_.empty())
        return underlyingIndicesCache_;

    map<AssetClass, std::set<std::string>> result;

    for (const auto& t : trades_) {
        try {
            auto underlyings = t.second->underlyingIndices(referenceDataManager);
            for (const auto& kv : underlyings) {
                result[kv.first].insert(kv.second.begin(), kv.second.end());
            }
        } catch (const std::exception& e) {
            StructuredTradeErrorMessage(t.second->id(), t.second->tradeType(), "Error retrieving underlying indices",
                                        e.what())
                .log();
        }
    }
    underlyingIndicesCache_ = result;
    return underlyingIndicesCache_;
}

std::set<std::string>
Portfolio::underlyingIndices(AssetClass assetClass,
                             const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) {

    std::map<AssetClass, std::set<std::string>> indices = underlyingIndices(referenceDataManager);
    auto it = indices.find(assetClass);
    if (it != indices.end()) {
        return it->second;
    }
    return std::set<std::string>();
}

std::pair<QuantLib::ext::shared_ptr<Trade>, bool> buildTrade(QuantLib::ext::shared_ptr<Trade>& trade,
                                                     const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                                     const std::string& context, const bool ignoreTradeBuildFail,
                                                     const bool buildFailedTrades, const bool emitStructuredError) {
    try {
        trade->reset();
        trade->build(engineFactory);
        TLOG("Required Fixings for trade " << trade->id() << ":");
        TLOGGERSTREAM(trade->requiredFixings());
        return std::make_pair(nullptr, true);
    } catch (std::exception& e) {
        if (emitStructuredError) {
            StructuredTradeErrorMessage(trade, "Error building trade for context '" + context + "'", e.what()).log();
        } else {
            ALOG("Error building trade '" << trade->id() << "' for context '" + context + "': " + e.what());
        }
        if (ignoreTradeBuildFail) {
            return std::make_pair(trade, false);
        } else if (buildFailedTrades) {
            QuantLib::ext::shared_ptr<FailedTrade> failed = QuantLib::ext::make_shared<FailedTrade>();
            failed->id() = trade->id();
            failed->setUnderlyingTradeType(trade->tradeType());
            failed->setEnvelope(trade->envelope());
            failed->build(engineFactory);
            failed->resetPricingStats(trade->getNumberOfPricings(), trade->getCumulativePricingTime());
            LOG("Built failed trade with id " << failed->id());
            return std::make_pair(failed, false);
        } else {
            return std::make_pair(nullptr, false);
        }
    }
}

} // namespace data
} // namespace ore
