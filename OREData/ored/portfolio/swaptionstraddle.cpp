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


#include <ql/instruments/compositeinstrument.hpp>

#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/portfolio/swaptionstraddle.hpp>

namespace ore {
namespace data {

void SwaptionStraddle::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("SwaptionStraddle::build() for " << id());

    QL_REQUIRE(!legData_.empty(), "SwaptionStraddle requires at least one leg");
    npvCurrency_ = notionalCurrency_ = legData_[0].currency();

    // Create payer swaption (Long Payer)
    OptionData payerOptionData = optionData_;
    payerOptionData.setCallPut("Call");
    payerSwaption_ = QuantLib::ext::make_shared<Swaption>(envelope(), payerOptionData, legData_);
    payerSwaption_->build(engineFactory);

    // Create receiver swaption (Long Receiver)
    OptionData receiverOptionData = optionData_;
    receiverOptionData.setCallPut("Put");
    receiverSwaption_ = QuantLib::ext::make_shared<Swaption>(envelope(), receiverOptionData, legData_);
    receiverSwaption_->build(engineFactory);

    // They both have the same maturity
    maturity_ = payerSwaption_->maturity();

    legs_ = payerSwaption_->legs();
    legCurrencies_ = payerSwaption_->legCurrencies();
    legPayers_ = payerSwaption_->legPayers();

    // Combine both swaptions in a composite instrument
    auto composite = QuantLib::ext::make_shared<CompositeInstrument>();
    double weight = longShort_ == "Long" ? 1.0 : -1.0;
    composite->add(payerSwaption_->instrument()->qlInstrument(), weight);
    composite->add(receiverSwaption_->instrument()->qlInstrument(), weight);

    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(composite);

    requiredFixings_.addData(payerSwaption_->requiredFixings());
    requiredFixings_.addData(receiverSwaption_->requiredFixings());

    additionalData_["isdaAssetClass"] = std::string("Interest Rate");
    additionalData_["isdaBaseProduct"] = std::string("Option");
    additionalData_["isdaSubProduct"] = std::string("SwaptionStraddle");
    additionalData_["isdaTransaction"] = std::string("");
    additionalData_["longShort"] = longShort_;
}

void SwaptionStraddle::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* swapNode = XMLUtils::getChildNode(node, "SwaptionStraddleData");
    optionData_.fromXML(XMLUtils::getChildNode(swapNode, "OptionData"));
    legData_.clear();
    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(swapNode, "LegData");
    for (Size i = 0; i < nodes.size(); i++) {
        LegData ld;
        ld.fromXML(nodes[i]);
        legData_.push_back(ld);
    }
    longShort_ = XMLUtils::getChildValue(swapNode, "LongShort", false, "Long");
}

XMLNode* SwaptionStraddle::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* swaptionNode = doc.allocNode("SwaptionStraddleData");
    XMLUtils::appendNode(node, swaptionNode);

    XMLUtils::addChild(doc, swaptionNode, "LongShort", longShort_);

    XMLUtils::appendNode(swaptionNode, optionData_.toXML(doc));
    for (Size i = 0; i < legData_.size(); i++)
        XMLUtils::appendNode(swaptionNode, legData_[i].toXML(doc));

    return node;
}

const std::map<std::string, QuantLib::ext::any>& SwaptionStraddle::additionalData() const {
    if (payerSwaption_) {
        for (const auto& [key, value] : payerSwaption_->additionalData()) {
            if (additionalData_.find("payer_" + key) == additionalData_.end()) {
                additionalData_["payer_" + key] = value;
            }
        }
    }
    if (receiverSwaption_) {
        for (const auto& [key, value] : receiverSwaption_->additionalData()) {
            if (additionalData_.find("receiver_" + key) == additionalData_.end()) {
                additionalData_["receiver_" + key] = value;
            }
        }
    }
    additionalData_["longShort"] = longShort_;
    return additionalData_;
}

std::map<AssetClass, std::set<std::string>>
SwaptionStraddle::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    if (payerSwaption_) {
        auto payerIndices = payerSwaption_->underlyingIndices(referenceDataManager);
        for (const auto& [assetClass, indices] : payerIndices) {
            result[assetClass].insert(indices.begin(), indices.end());
        }
    }
    return result;
}

} // namespace data
} // namespace ore
