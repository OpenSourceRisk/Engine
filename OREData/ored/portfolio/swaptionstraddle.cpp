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

#include <ored/portfolio/compositeinstrumentwrapper.hpp>
#include <ored/portfolio/swaptionstraddle.hpp>

namespace ore {
namespace data {

void SwaptionStraddle::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("SwaptionStraddle::build() for " << id());

    QL_REQUIRE(!legData_.empty(), "SwaptionStraddle requires at least one leg");

    // Create swaption 1
    swaption1_ = QuantLib::ext::make_shared<Swaption>(envelope(), optionData_, legData_);
    // we need to do set the id manually because it otherwise remains blank - it is essential because the engine is stored by id if the option style is Bermudan
    swaption1_->setId(id() + "_1");
    swaption1_->build(engineFactory);

    // Create swaption 2 with opposite legs than swaption 1.
    std::vector<LegData> receiverLegData;
    for (auto const& ld: legData_) {
        LegData newLeg = ld;
        newLeg.isPayer() = !ld.isPayer(); //Reverse each legs payer/receiver
        receiverLegData.push_back(newLeg);
    }
    swaption2_ = QuantLib::ext::make_shared<Swaption>(envelope(), optionData_, receiverLegData);
    // we need to do set the id manually because it otherwise remains blank - it is essential because the engine is stored by id if the option style is Bermudan
    swaption2_->setId(id() + "_2");
    swaption2_->build(engineFactory);

    // They both have the same maturity
    maturity_ = swaption1_->maturity();
    notionalCurrency_ = swaption1_->notionalCurrency();
    npvCurrency_ = swaption1_->notionalCurrency();
    maturityType_ = swaption1_->maturityType();

    // Combine both swaptions in a composite instrument
    instrument_ = QuantLib::ext::make_shared<CompositeInstrumentWrapper>(
        std::vector<QuantLib::ext::shared_ptr<InstrumentWrapper>>{swaption1_->instrument(), swaption2_->instrument()});

    requiredFixings_.addData(swaption1_->requiredFixings());
    requiredFixings_.addData(swaption2_->requiredFixings());

    additionalData_["isdaAssetClass"] = std::string("Interest Rate");
    additionalData_["isdaBaseProduct"] = std::string("Option");
    additionalData_["isdaSubProduct"] = std::string("Swaption");
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
    if (swaption1_) {
        for (const auto& [key, value] : swaption1_->additionalData()) {
            if (additionalData_.find("payer_" + key) == additionalData_.end()) {
                additionalData_["payer_" + key] = value;
            }
        }
    }
    if (swaption2_) {
        for (const auto& [key, value] : swaption2_->additionalData()) {
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
    if (swaption1_) {
        auto payerIndices = swaption1_->underlyingIndices(referenceDataManager);
        for (const auto& [assetClass, indices] : payerIndices) {
            result[assetClass].insert(indices.begin(), indices.end());
        }
    }
    return result;
}

} // namespace data
} // namespace ore
