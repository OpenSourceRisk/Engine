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

void Portfolio::load(const string& fileName, const boost::shared_ptr<TradeFactory>& factory) {

    LOG("Parsing XML " << fileName.c_str());
    XMLDocument doc(fileName);
    LOG("Loaded XML file");

    load(doc, factory);
}

void Portfolio::loadFromXMLString(const string& xmlString, const boost::shared_ptr<TradeFactory>& factory) {
    LOG("Parsing XML string");
    XMLDocument doc;
    doc.fromXMLString(xmlString);
    LOG("Loaded XML string");

    load(doc, factory);
}

void Portfolio::load(XMLDocument& doc, const boost::shared_ptr<TradeFactory>& factory) {
    XMLNode* node = doc.getFirstNode("Portfolio");
    if (node) {
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
                    add(trade);

                    DLOG("Added Trade " << id << " (" << trade->id() << ")"
                                        << " class:" << tradeType);
                } catch (std::exception& ex) {
                    ALOG("Exception parsing Trade XML Node (id=" << id
                                                                 << ") "
                                                                    "(class="
                                                                 << tradeType << ") : " << ex.what());
                }
            } else {
                WLOG("Unable to build Trade for tradeType=" << tradeType);
            }
        }
    } else {
        WLOG("No Portfolio Node in XML doc");
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

void Portfolio::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    LOG("Building Portfolio of size " << trades_.size());
    auto trade = trades_.begin();
    while (trade != trades_.end()) {
        try {
            (*trade)->build(engineFactory);
            ++trade;
        } catch (std::exception& e) {
            ALOG("Error building trade (" << (*trade)->id() << ") : " << e.what());
            trade = trades_.erase(trade);
        }
    }
    LOG("Built Portfolio. Size now " << trades_.size());
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

void Portfolio::add(const boost::shared_ptr<Trade>& trade) {
    QL_REQUIRE(!has(trade->id()), "Attempted to add a trade to the portfolio with an id, which already exists.");
    trades_.push_back(trade);
}

bool Portfolio::has(const string &id) {
    return find_if(trades_.begin(),
                   trades_.end(),
                   [id](const boost::shared_ptr<Trade> trade) {return trade->id() == id; }) != trades_.end();
}

} // namespace data
} // namespace ore
