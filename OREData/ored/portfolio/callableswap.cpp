/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/callableswap.hpp>

#include <ored/portfolio/compositeinstrumentwrapper.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace data {

void CallableSwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    LOG("Building CallableSwap " << id());
    swap_.reset();
    swaption_.reset();
    // we need to do set the id manually because it otherwise remains blank
    swap_.id() = id() + "_Swap";
    swap_.build(engineFactory);
    // likewise here, and now it is essential because the engine is stored by id if the option style is Bermudan
    swaption_.id() = id() + "_Swaption";
    swaption_.build(engineFactory);

    setSensitivityTemplate(swaption_.sensitivityTemplate());

    instrument_ = QuantLib::ext::make_shared<CompositeInstrumentWrapper>(
        std::vector<QuantLib::ext::shared_ptr<InstrumentWrapper>>{swap_.instrument(), swaption_.instrument()});

    legs_ = swap_.legs();
    legCurrencies_ = swap_.legCurrencies();
    legPayers_ = swap_.legPayers();
    if (swaption_.isExercised()) {
        legs_.insert(legs_.end(), swaption_.legs().begin(), swaption_.legs().end());
        legCurrencies_.insert(legCurrencies_.end(), swaption_.legCurrencies().begin(), swaption_.legCurrencies().end());
        bool isShort = parsePositionType(swaption_.optionData().longShort()) == QuantLib::Position::Short;
        for (auto const& p : swaption_.legPayers()) {
            legPayers_.push_back(isShort ? !p : p);
        }
    }

    npvCurrency_ = swap_.npvCurrency();
    notional_ = swap_.notional();
    notionalCurrency_ = swap_.notionalCurrency();
    maturity_ = swap_.maturity();

    requiredFixings_.addData(swap_.requiredFixings());
}

const std::map<std::string, boost::any>& CallableSwap::additionalData() const { return swap_.additionalData(); }

void CallableSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* swapNode = XMLUtils::getChildNode(node, "CallableSwapData");

    vector<LegData> legData;
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    for (Size i = 0; i < nodes.size(); i++) {
        auto ld = QuantLib::ext::make_shared<ore::data::LegData>();
        ld->fromXML(nodes[i]);
        legData.push_back(*ld);
    }
    swap_ = ore::data::Swap(envelope(), legData);

    OptionData optionData;
    optionData.fromXML(XMLUtils::getChildNode(swapNode, "OptionData"));
    if (parsePositionType(optionData.longShort()) == QuantLib::Position::Long) {
        vector<LegData> reversedLegData(legData);
        for (auto& l : reversedLegData)
            l.isPayer() = !l.isPayer();
        swaption_ = ore::data::Swaption(envelope(), optionData, reversedLegData);
    } else {
        swaption_ = ore::data::Swaption(envelope(), optionData, legData);
    }
}

XMLNode* CallableSwap::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* swapNode = doc.allocNode("CallableSwapData");
    XMLUtils::appendNode(node, swapNode);

    for (Size i = 0; i < swap_.legData().size(); i++) {
        // poor const correctness in ORE, so we copy...
        LegData ld = swap_.legData()[i];
        XMLUtils::appendNode(swapNode, ld.toXML(doc));
    }

    // same as above...
    OptionData od = swaption_.optionData();
    XMLUtils::appendNode(swapNode, od.toXML(doc));

    return node;
}

} // namespace data
} // namespace ore
