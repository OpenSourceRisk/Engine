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
    //trades_.clear();
    tradeLookup_.clear();
    underlyingIndicesCache_.clear();
}

void Portfolio::reset() {
    LOG("Reset portfolio of size " << tradeLookup_.size());
    for (auto [id,t] : tradeLookup_)
        t->reset();
}

void Portfolio::load(const string& fileName, const boost::shared_ptr<TradeFactory>& factory) {

    LOG("Parsing XML " << fileName.c_str());
    XMLDocument doc(fileName);
    LOG("Loaded XML file");
    XMLNode* node = doc.getFirstNode("Portfolio");
    fromXML(node, factory);
}

void Portfolio::loadFromXMLString(const string& xmlString, const boost::shared_ptr<TradeFactory>& factory) {
    LOG("Parsing XML string");
    XMLDocument doc;
    doc.fromXMLString(xmlString);
    LOG("Loaded XML string");
    XMLNode* node = doc.getFirstNode("Portfolio");
    fromXML(node, factory);
}

void Portfolio::fromXML(XMLNode* node, const boost::shared_ptr<TradeFactory>& factory) {
    XMLUtils::checkNode(node, "Portfolio");
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(node, "Trade");
    for (Size i = 0; i < nodes.size(); i++) {
        string tradeType = XMLUtils::getChildValue(nodes[i], "TradeType", true);

        // Get the id attribute
        string id = XMLUtils::getAttribute(nodes[i], "id");
        QL_REQUIRE(id != "", "No id attribute in Trade Node");
        DLOG("Parsing trade id:" << id);
        boost::shared_ptr<Trade> trade = factory->build(tradeType);

        bool failedToLoad = false;
        if (trade) {
            try {
                trade->fromXML(nodes[i]);
                trade->id() = id;
                add(trade);

                DLOG("Added Trade " << id << " (" << trade->id() << ")"
                                    << " type:" << tradeType);
            } catch (std::exception& ex) {
                ALOG(StructuredTradeErrorMessage(id, tradeType, "Error parsing Trade XML", ex.what()));
                failedToLoad = true;
            }
        } else {
            ALOG(StructuredTradeErrorMessage(id, tradeType, "Error parsing Trade XML"));
            failedToLoad = true;
        }

        // If trade loading failed, then insert a dummy trade with same id and envelope
        if (failedToLoad && buildFailedTrades_) {
            try {
                trade = factory->build("Failed");
                // this loads only type, id and envelope, but type will be set to the original trade's type
                trade->fromXML(nodes[i]);
                // create a dummy trade of type "Dummy"
                boost::shared_ptr<FailedTrade> failedTrade = boost::make_shared<FailedTrade>();
                // copy id and envelope
                failedTrade->id() = id;
                failedTrade->setUnderlyingTradeType(tradeType);
                failedTrade->envelope() = trade->envelope();
                // and add it to the portfolio
                add(failedTrade);
                WLOG("Added trade id " << failedTrade->id() << " type " << failedTrade->tradeType()
                                       << " for original trade type " << trade->tradeType());
            } catch (std::exception& ex) {
                ALOG(StructuredTradeErrorMessage(id, tradeType, "Error parsing type and envelope", ex.what()));
            }
        }
    }
    LOG("Finished Parsing XML doc");
}

void Portfolio::doc(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Portfolio");
    doc.appendNode(node);
    for (auto& [id,t] : tradeLookup_)
        XMLUtils::appendNode(node, t->toXML(doc));
}

void Portfolio::save(const string& fileName) const {
    XMLDocument document;
    LOG("Saving Portfolio to " << fileName);
    doc(document);
    document.toFile(fileName);
}

string Portfolio::saveToXMLString() const {
    XMLDocument document;
    LOG("Write Portfolio to xml string.");
    doc(document);
    return document.toString();
}

bool Portfolio::remove(const std::string& tradeID) {
    tradeLookup_.erase(tradeID);
    underlyingIndicesCache_.clear();
    /*
    for (auto it = trade_.begin(); it != trade_.end(); ++it) {
        if ((*it)->id() == tradeID) {
            trades_.erase(it);
            return true;
        }
    }
    */
    return false;
}

void Portfolio::removeMatured(const Date& asof) {
    for (auto it = tradeLookup_.begin(); it != tradeLookup_.end(); /* manual */) {
        if ((*it).second->maturity() < asof) {
            ALOG(StructuredTradeErrorMessage((*it).second, "Trade is Matured", ""));
            tradeLookup_.erase((*it).second->id());
            ++it;
            /*
            tradeLookup_.erase((*it).second->id());
            it = tradeLookup_.erase(it);
            */
        } else {
            ++it;
        }
    }
}

void Portfolio::build(const boost::shared_ptr<EngineFactory>& engineFactory, const std::string& context,
                      const bool emitStructuredError) {
    LOG("Building Portfolio of size " << tradeLookup_.size() << " for context = '" << context << "'");
    auto trade = tradeLookup_.begin();
    Size initialSize = tradeLookup_.size();
    Size failedTrades = 0;
    while (trade != tradeLookup_.end()) {
        auto [ft, success] = buildTrade((*trade).second, engineFactory, context, buildFailedTrades(), emitStructuredError);
        if(success) {
	    ++trade;
        } else if (ft) {
            (*trade).second = ft;
            ++failedTrades;
            ++trade;
        } else {
            tradeLookup_.erase((*trade).second->id());
            trade = tradeLookup_.erase(trade);
        }
    }
    LOG("Built Portfolio. Initial size = " << initialSize << ", size now " << tradeLookup_.size() << ", built "
                                           << failedTrades << " failed trades, context is " + context);

    QL_REQUIRE(tradeLookup_.size() > 0, "Portfolio does not contain any built trades, context is '" + context + "'");
}

Date Portfolio::maturity() const {
    QL_REQUIRE(tradeLookup_.size() > 0, "Cannot get maturity of an empty portfolio");
    Date mat = tradeLookup_.begin()->second->maturity();
    for (const auto& t : tradeLookup_)
        mat = std::max(mat, t.second->maturity());
    return mat;
}

vector<string> Portfolio::ids() const {
    vector<string> ids;
    for (const auto& t : tradeLookup_)
        ids.push_back(t.second->id());
    return ids;
}

vector<boost::shared_ptr<Trade>> Portfolio::trades() const {
        std::vector<boost::shared_ptr<Trade>> trades;
        for (auto& [id, t] : tradeLookup_)
        trades.push_back(t);
        return trades;
}

map<string, string> Portfolio::nettingSetMap() const {
    map<string, string> nettingSetMap;
    for (const auto& t : tradeLookup_)
        nettingSetMap[t.second->id()] = t.second->envelope().nettingSetId();
    return nettingSetMap;
}

std::vector<std::string> Portfolio::counterparties() const {
    vector<string> counterparties;
    for (const auto& t : tradeLookup_)
        counterparties.push_back(t.second->envelope().counterparty());
    sort(counterparties.begin(), counterparties.end());
    counterparties.erase(unique(counterparties.begin(), counterparties.end()), counterparties.end());
    return counterparties;
}

map<string, set<string>> Portfolio::counterpartyNettingSets() const {
    map<string, set<string>> cpNettingSets;
    for (const auto& t : tradeLookup_)
        cpNettingSets[t.second->envelope().counterparty()].insert(t.second->envelope().nettingSetId());
    return cpNettingSets;
}

void Portfolio::add(const boost::shared_ptr<Trade>& trade) {
    QL_REQUIRE(!has(trade->id()),
               "Attempted to add a trade to the portfolio with an id, which already exists.");
    underlyingIndicesCache_.clear();
    //trades_.push_back(trade);
    tradeLookup_[trade->id()] = trade;
}

bool Portfolio::has(const string& id) {
    return tradeLookup_.find(id) != tradeLookup_.end();
}

boost::shared_ptr<Trade> Portfolio::get(const string& id) const {
    auto it = tradeLookup_.find(id);
    if (it != tradeLookup_.end())
        return it->second;
    else
        return nullptr;
}

std::set<std::string> Portfolio::portfolioIds() const {
    std::set<std::string> portfolioIds;
    for (auto const& t : tradeLookup_)
        portfolioIds.insert(t.second->portfolioIds().begin(), t.second->portfolioIds().end());
    return portfolioIds;
}

bool Portfolio::hasNettingSetDetails() const {
    bool hasNettingSetDetails = false;
    auto tradesVec = trades();
    for (auto it = tradesVec.begin(); it != tradesVec.end(); it++) {
        if (!(*it)->envelope().nettingSetDetails().emptyOptionalFields()) {
            hasNettingSetDetails = true;
            break;
        }
    }
    return hasNettingSetDetails;
}

map<string, set<Date>> Portfolio::fixings(const Date& settlementDate) const {

    map<string, set<Date>> result;

    for (const auto& t : tradeLookup_) {
        auto fixings = t.second->fixings(settlementDate);
        for (const auto& kv : fixings) {
            result[kv.first].insert(kv.second.begin(), kv.second.end());
        }
    }

    return result;
}

std::map<AssetClass, std::set<std::string>>
Portfolio::underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager) {

    if (!underlyingIndicesCache_.empty())
        return underlyingIndicesCache_;

    map<AssetClass, std::set<std::string>> result;

    for (const auto& t : tradeLookup_) {
        try {
            auto underlyings = t.second->underlyingIndices(referenceDataManager);
            for (const auto& kv : underlyings) {
                result[kv.first].insert(kv.second.begin(), kv.second.end());
            }
        } catch (const std::exception& e) {
            ALOG(StructuredTradeErrorMessage(t.second->id(), t.second->tradeType(), "Error retrieving underlying indices", e.what()));
        }
    }
    underlyingIndicesCache_ = result;
    return underlyingIndicesCache_;
}

std::set<std::string>
Portfolio::underlyingIndices(AssetClass assetClass,
                             const boost::shared_ptr<ReferenceDataManager>& referenceDataManager) {

    std::map<AssetClass, std::set<std::string>> indices = underlyingIndices(referenceDataManager);
    auto it = indices.find(assetClass);
    if (it != indices.end()) {
        return it->second;
    }
    return std::set<std::string>();
}

std::pair<boost::shared_ptr<Trade>, bool> buildTrade(boost::shared_ptr<Trade>& trade,
                                                     const boost::shared_ptr<EngineFactory>& engineFactory,
                                                     const std::string& context, const bool buildFailedTrades,
                                                     const bool emitStructuredError) {
    try {
        trade->reset();
        trade->build(engineFactory);
        TLOG("Required Fixings for trade " << trade->id() << ":");
        TLOGGERSTREAM(trade->requiredFixings());
        return std::make_pair(nullptr, true);
    } catch (std::exception& e) {
        if(emitStructuredError) {
            ALOG(StructuredTradeErrorMessage(trade, "Error building trade for context '" + context + "'", e.what()));
        } else {
            ALOG("Error building trade '" << trade->id() << "' for context '" + context + "': " + e.what());
        }
        if (buildFailedTrades) {
            boost::shared_ptr<FailedTrade> failed = boost::make_shared<FailedTrade>();
            failed->id() = trade->id();
            failed->setUnderlyingTradeType(trade->tradeType());
            failed->envelope() = trade->envelope();
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
