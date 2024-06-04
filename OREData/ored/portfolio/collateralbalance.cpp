/*
  Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/portfolio/collateralbalance.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/portfolio/structuredconfigurationerror.hpp>
#include <ql/errors.hpp>

using ore::data::XMLUtils;
using ore::data::XMLNode;
using ore::data::XMLDocument;
using ore::data::parseReal;

namespace ore {
namespace data {

void CollateralBalances::add(const QuantLib::ext::shared_ptr<CollateralBalance>& cb, const bool overwrite) {
    if (collateralBalances_.find(cb->nettingSetDetails()) != collateralBalances_.end() && !overwrite)
        QL_FAIL("Cannot add collateral balances since it already exists and overwrite=false: " << cb->nettingSetDetails());

    collateralBalances_[cb->nettingSetDetails()] = cb;
}

bool CollateralBalances::has(const NettingSetDetails& nettingSetDetails) const {
    return collateralBalances_.find(nettingSetDetails) != collateralBalances_.end();
}

bool CollateralBalances::has(const std::string& nettingSetId) const {
    return has(NettingSetDetails(nettingSetId));
}

void CollateralBalances::reset() {
    collateralBalances_.clear();
}

const bool CollateralBalances::empty() {
    return collateralBalances_.empty();
}

CollateralBalance::CollateralBalance(ore::data::XMLNode* node) { 
    fromXML(node);
}

const QuantLib::ext::shared_ptr<CollateralBalance>& CollateralBalances::get(const NettingSetDetails& nettingSetDetails) const {
    if (has(nettingSetDetails))
        return collateralBalances_.find(nettingSetDetails)->second;
    else
        QL_FAIL("CollateralBalance not found in manager: " << nettingSetDetails);
}

const QuantLib::ext::shared_ptr<CollateralBalance>& CollateralBalances::get(const std::string& nettingSetId) const {
    return get(NettingSetDetails(nettingSetId));
}


void CollateralBalances::currentIM(const std::string& baseCurrency,
                                   std::map<std::string, QuantLib::Real>& currentIM) {
    for (auto& cb : collateralBalances_) {
        if (cb.second->currency() == baseCurrency) {
            currentIM[cb.first.nettingSetId()] = cb.second->initialMargin();
        }
    }
}
    
void CollateralBalance::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CollateralBalance");
    
    XMLNode* nettingSetDetailsNode = XMLUtils::getChildNode(node, "NettingSetDetails");
    if (nettingSetDetailsNode) {
        nettingSetDetails_.fromXML(nettingSetDetailsNode);
    } else {
        const string nettingSetId = XMLUtils::getChildValue(node, "NettingSetId", false);
        nettingSetDetails_ = NettingSetDetails(nettingSetId);
    }   

    currency_ = XMLUtils::getChildValue(node, "Currency", true);

    XMLNode* initialMarginNode = XMLUtils::getChildNode(node, "InitialMargin");
    if (!initialMarginNode || (initialMarginNode && XMLUtils::getNodeValue(initialMarginNode).empty()))
        im_ = QuantLib::Null<QuantLib::Real>();
    else
        im_ = parseReal(XMLUtils::getNodeValue(initialMarginNode));

    XMLNode* variationMarginNode = XMLUtils::getChildNode(node, "VariationMargin");
    if (!variationMarginNode || (variationMarginNode && XMLUtils::getNodeValue(variationMarginNode).empty()))
        vm_ = QuantLib::Null<QuantLib::Real>();
    else
        vm_ = parseReal(XMLUtils::getNodeValue(variationMarginNode));

    DLOG("Loaded collateral balances for netting set " << nettingSetId());
    DLOG("Currency:           " << currency());
    DLOG("Variation Margin:   " << variationMargin());
    DLOG("Initial Margin:     " << initialMargin());
}

XMLNode* CollateralBalance::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("CollateralBalance");
    XMLUtils::addChild(doc, node, "Currency", currency_);
    if (nettingSetDetails_.emptyOptionalFields()) {
        XMLUtils::addChild(doc, node, "NettingSetId", nettingSetDetails_.nettingSetId());
    } else {
        XMLUtils::appendNode(node, nettingSetDetails_.toXML(doc));
    }
    if (im_ != QuantLib::Null<QuantLib::Real>())
        XMLUtils::addChild(doc, node, "InitialMargin", std::round(im_));
    if (vm_ != QuantLib::Null<QuantLib::Real>())
        XMLUtils::addChild(doc, node, "VariationMargin", std::round(vm_));
    return node;
}

void CollateralBalances::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CollateralBalances");
    std::vector<XMLNode*> nettingSetNodes = XMLUtils::getChildrenNodes(node, "CollateralBalance");
    for (unsigned i = 0; i < nettingSetNodes.size(); i++) {
        try {
            QuantLib::ext::shared_ptr<CollateralBalance> cb(new CollateralBalance(nettingSetNodes[i]));
            add(cb);
        } catch (const std::exception& ex) {
            ore::data::StructuredConfigurationErrorMessage("Collateral balances", "",
                                                           "Collateral balance node failed to parse", ex.what())
                .log();
        }
    }
}

XMLNode* CollateralBalances::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("CollateralBalances");
    for (auto it = collateralBalances_.begin(); it != collateralBalances_.end(); ++it) {
        XMLUtils::appendNode(node, it->second->toXML(doc)); 
    }
    return node;
}

const std::map<NettingSetDetails, QuantLib::ext::shared_ptr<CollateralBalance>>& CollateralBalances::collateralBalances() {
    return collateralBalances_;
}

} // namespace data
} // namespace ore
