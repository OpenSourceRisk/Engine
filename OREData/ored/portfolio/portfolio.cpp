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

void Portfolio::reset() {
    LOG("Reset portfolio of size " << trades_.size());
    for (auto t : trades_)
        t->reset();
}

void Portfolio::load(const string& fileName, const boost::shared_ptr<TradeFactory>& factory,
                     const bool checkForDuplicateIds) {

    LOG("Parsing XML " << fileName.c_str());
    XMLDocument doc(fileName);
    LOG("Loaded XML file");
    XMLNode* node = doc.getFirstNode("Portfolio");
    fromXML(node, factory, checkForDuplicateIds);
}

void Portfolio::loadFromXMLString(const string& xmlString, const boost::shared_ptr<TradeFactory>& factory,
                                  const bool checkForDuplicateIds) {
    LOG("Parsing XML string");
    XMLDocument doc;
    doc.fromXMLString(xmlString);
    LOG("Loaded XML string");
    XMLNode* node = doc.getFirstNode("Portfolio");
    fromXML(node, factory, checkForDuplicateIds);
}

void Portfolio::fromXML(XMLNode* node, const boost::shared_ptr<TradeFactory>& factory,
                        const bool checkForDuplicateIds) {
    XMLUtils::checkNode(node, "Portfolio");
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(node, "Trade");
    for (Size i = 0; i < nodes.size(); i++) {
        string tradeType = XMLUtils::getChildValue(nodes[i], "TradeType", true);

        // Get the id attribute
        string id = XMLUtils::getAttribute(nodes[i], "id");
        QL_REQUIRE(id != "", "No id attribute in Trade Node");
        DLOG("Parsing trade id:" << id);
        boost::shared_ptr<Trade> trade = factory->build(tradeType);

        if (trade) {
            try {
                trade->fromXML(nodes[i]);
                trade->id() = id;
                add(trade, checkForDuplicateIds);

                DLOG("Added Trade " << id << " (" << trade->id() << ")"
                                    << " type:" << tradeType);
            } catch (std::exception& ex) {
                ALOG(StructuredTradeErrorMessage(id, tradeType, "Error parsing Trade XML", ex.what()));
            }
        } else {
            WLOG("Unable to build Trade for tradeType=" << tradeType);
        }
    }
    LOG("Finished Parsing XML doc");
}

void Portfolio::save(const string& fileName) const {
    LOG("Saving Portfolio to " << fileName);

    XMLDocument doc;
    XMLNode* node = doc.allocNode("Portfolio");
    doc.appendNode(node);
    for (auto t : trades_)
        XMLUtils::appendNode(node, t->toXML(doc));
    // Write doc out.
    doc.toFile(fileName);
}

bool Portfolio::remove(const std::string& tradeID) {
    for (auto it = trades_.begin(); it != trades_.end(); ++it) {
        if ((*it)->id() == tradeID) {
            trades_.erase(it);
            return true;
        }
    }
    return false;
}

void Portfolio::removeMatured(const Date& asof) {
    for (auto it = trades_.begin(); it != trades_.end(); /* manual */) {
        if ((*it)->maturity() < asof) {
            ALOG(StructuredTradeErrorMessage(*it, "Trade is Matured", ""));
            it = trades_.erase(it);
        } else {
            ++it;
        }
    }
}

void Portfolio::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    LOG("Building Portfolio of size " << trades_.size());
    auto trade = trades_.begin();
    while (trade != trades_.end()) {
        try {
            (*trade)->reset();
            (*trade)->build(engineFactory);
            TLOG("Required Fixings for trade " << (*trade)->id() << ":");
            TLOGGERSTREAM << (*trade)->requiredFixings();
            ++trade;
        } catch (std::exception& e) {
            ALOG(StructuredTradeErrorMessage(*trade, "Error building trade", e.what()));
            trade = trades_.erase(trade);
        }
    }
    LOG("Built Portfolio. Size now " << trades_.size());

    QL_REQUIRE(trades_.size() > 0, "Portfolio does not contain any built trades");
}

Date Portfolio::maturity() const {
    QL_REQUIRE(trades_.size() > 0, "Cannot get maturity of an empty portfolio");
    Date mat = trades_.front()->maturity();
    for (const auto& t : trades_)
        mat = std::max(mat, t->maturity());
    return mat;
}

vector<string> Portfolio::ids() const {
    vector<string> ids;
    for (auto t : trades_)
        ids.push_back(t->id());
    return ids;
}

map<string, string> Portfolio::nettingSetMap() const {
    map<string, string> nettingSetMap;
    for (auto t : trades_)
        nettingSetMap[t->id()] = t->envelope().nettingSetId();
    return nettingSetMap;
}

std::vector<std::string> Portfolio::counterparties() const {
    vector<string> counterparties;
    for (auto t : trades_)
        counterparties.push_back(t->envelope().counterparty());
    sort(counterparties.begin(), counterparties.end());
    counterparties.erase(unique(counterparties.begin(), counterparties.end()), counterparties.end());
    return counterparties;
}

map<string, set<string>> Portfolio::counterpartyNettingSets() const {
    map<string, set<string>> cpNettingSets;
    for (auto t : trades_)
        cpNettingSets[t->envelope().counterparty()].insert(t->envelope().nettingSetId());
    return cpNettingSets;
}

void Portfolio::add(const boost::shared_ptr<Trade>& trade, const bool checkForDuplicateIds) {
    QL_REQUIRE(!checkForDuplicateIds || !has(trade->id()),
               "Attempted to add a trade to the portfolio with an id, which already exists.");
    trades_.push_back(trade);
}

bool Portfolio::has(const string& id) {
    return find_if(trades_.begin(), trades_.end(),
                   [id](const boost::shared_ptr<Trade>& trade) { return trade->id() == id; }) != trades_.end();
}

boost::shared_ptr<Trade> Portfolio::get(const string& id) const {
    auto it = find_if(trades_.begin(), trades_.end(),
                      [id](const boost::shared_ptr<Trade>& trade) { return trade->id() == id; });

    if (it == trades_.end()) {
        return nullptr;
    } else {
        return *it;
    }
}

std::set<std::string> Portfolio::portfolioIds() const {
    std::set<std::string> portfolioIds;
    for (auto const& t : trades_)
        portfolioIds.insert(t->portfolioIds().begin(), t->portfolioIds().end());
    return portfolioIds;
}

map<string, set<Date>> Portfolio::fixings(const Date& settlementDate) const {

    map<string, set<Date>> result;

    for (const auto& t : trades_) {
        auto fixings = t->fixings(settlementDate);
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

    for (const auto& t : trades_) {
        auto underlyings = t->underlyingIndices(referenceDataManager);
        for (const auto& kv : underlyings) {
            result[kv.first].insert(kv.second.begin(), kv.second.end());
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

} // namespace data
} // namespace ore
