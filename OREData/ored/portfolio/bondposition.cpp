/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/portfolio/bondposition.hpp>
#include <ored/portfolio/referencedata.hpp>

namespace ore {
namespace data {

void BondPositionData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "BondBasketData");
    quantity_ = XMLUtils::getChildValueAsDouble(node, "Quantity", true);
    identifier_ = XMLUtils::getChildValue(node, "Identifier", true);
    auto c = XMLUtils::getChildrenNodes(node, "Underlying");
    underlyings_.clear();
    for (auto const n : c) {
        underlyings_.push_back(BondUnderlying());
        underlyings_.back().fromXML(n);
    }
}

XMLNode* BondPositionData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("BondBasketData");
    XMLUtils::addChild(doc, n, "Quantity", quantity_);
    XMLUtils::addChild(doc, n, "Identifier", identifier_);
    for (auto& u : underlyings_) {
        XMLUtils::appendNode(n, u.toXML(doc));
    }
    return n;
}

void BondPositionData::populateFromBondBasketReferenceData(const QuantLib::ext::shared_ptr<ReferenceDataManager>& ref) {
    QL_REQUIRE(!identifier_.empty(), "BondPositionData::populateFromBondBasketReferenceData(): no identifier given");
    if (!ref || !ref->hasData(BondBasketReferenceDatum::TYPE, identifier_)) {
        DLOG("could not get BondBasketReferenceDatum for '" << identifier_ << "' leave data in trade unchanged");
    } else {
        DLOG("got BondBasketReferenceDatum for '" << identifier_ << "':");
        auto r = QuantLib::ext::dynamic_pointer_cast<BondBasketReferenceDatum>(
            ref->getData(BondBasketReferenceDatum::TYPE, identifier_));
        QL_REQUIRE(r, "BondPositionData::populateFromBondBasketReferenceData(): internal error, could not cast "
                      "reference datum to expected type.");
        underlyings_ = r->underlyingData();
        DLOG("updated " << underlyings_.size() << " Underlying nodes.");
    }
}

void BondPosition::build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) {
    DLOG("BondPosition::build() called for " << id());

    // ISDA taxonomy: not a derivative, but define the asset class at least
    // so that we can determine a TRS asset class that has a Bond position underlying
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    bonds_.clear();
    weights_.clear();
    bidAskAdjustments_.clear();
    fxConversion_.clear();

    data_ = originalData_;
    data_.populateFromBondBasketReferenceData(engineFactory->referenceData());

    QL_REQUIRE(!data_.underlyings().empty(), "BondPosition::build(): no underlyings given");

    maturity_ = Date::minDate();
    for (auto const& u : data_.underlyings()) {
        try {
            bonds_.push_back(BondFactory::instance().build(engineFactory, engineFactory->referenceData(), u.name()));
        } catch (const std::exception& e) {
            QL_FAIL("Build failed for underlying " << u.type() << " (" << u.name() << "): " << e.what());
        }
        weights_.push_back(u.weight());
        bidAskAdjustments_.push_back(u.bidAskAdjustment());
        maturity_ = std::max(bonds_.back().bond->maturityDate(), maturity_);
    }

    // get fx quotes

    isSingleCurrency_ = true;
    npvCurrency_ = bonds_.front().currency;
    for (auto const& b : bonds_) {
        fxConversion_.push_back(engineFactory->market()->fxSpot(b.currency + npvCurrency_,
                                                                engineFactory->configuration(MarketContext::pricing)));
        if (npvCurrency_ != b.currency)
            isSingleCurrency_ = false;
    }

    // set instrument
    std::vector<QuantLib::ext::shared_ptr<QuantLib::Bond>> qlbonds;
    std::transform(bonds_.begin(), bonds_.end(), std::back_inserter(qlbonds),
                   [](const BondBuilder::Result& d) { return d.bond; });
    instrument_ = QuantLib::ext::make_shared<BondPositionInstrumentWrapper>(data_.quantity(), qlbonds, weights_,
                                                                    bidAskAdjustments_, fxConversion_);

    // leave legs empty, leave notional empty for the time being
    notional_ = Null<Real>();
    notionalCurrency_ = "";

    setSensitivityTemplate(std::string());
}

void BondPosition::setNpvCurrencyConversion(const std::string& ccy, const Handle<Quote>& conversion) {
    npvCurrency_ = ccy;
    QuantLib::ext::static_pointer_cast<BondPositionInstrumentWrapper>(instrument_)->setNpvCurrencyConversion(conversion);
}

void BondPosition::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    originalData_.fromXML(XMLUtils::getChildNode(node, "BondBasketData"));
    data_ = originalData_;
}

XMLNode* BondPosition::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLUtils::appendNode(node, originalData_.toXML(doc));
    return node;
}

std::map<AssetClass, std::set<std::string>>
BondPosition::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    for (auto const& u : data_.underlyings()) {
        result[AssetClass::BOND].insert(u.name());
    }
    result[AssetClass::BOND_INDEX].insert(data().identifier());
    return result;
}

BondPositionInstrumentWrapper::BondPositionInstrumentWrapper(
    const Real quantity, const std::vector<QuantLib::ext::shared_ptr<QuantLib::Bond>>& bonds, const std::vector<Real>& weights,
    const std::vector<Real>& bidAskAdjustments, const std::vector<Handle<Quote>>& fxConversion)
    : quantity_(quantity), bonds_(bonds), weights_(weights), bidAskAdjustments_(bidAskAdjustments),
      fxConversion_(fxConversion) {
    QL_REQUIRE(bonds_.size() == weights_.size(), "BondPositionInstrumentWrapper: bonds size ("
                                                     << bonds_.size() << ") must match weights size ("
                                                     << weights_.size() << ")");
    QL_REQUIRE(bonds_.size() == bidAskAdjustments_.size(),
               "BondPositionInstrumentWrapper: bonds size (" << bonds_.size() << ") must match bidAskAdjustment size ("
                                                             << weights_.size() << ")");
    QL_REQUIRE(fxConversion_.empty() || fxConversion_.size() == bonds_.size(),
               "BondPositionInstrumentWrapper: fxConversion size ("
                   << fxConversion_.size() << ") must match bonds size (" << bonds_.size() << ")");
}

void BondPositionInstrumentWrapper::setNpvCurrencyConversion(const Handle<Quote>& npvCcyConversion) {
    npvCcyConversion_ = npvCcyConversion;
}

Real BondPositionInstrumentWrapper::NPV() const {
    Real result = 0.0;
    for (Size i = 0; i < bonds_.size(); ++i) {
        // - divide by current notional, because weights are supposed to include any amortization factors
        // - add bid ask adjustment to relative price in bond ccy
        Real tmp = quantity_ * (bonds_[i]->NPV() / bonds_[i]->notional() + bidAskAdjustments_[i]);
        if (!fxConversion_[i].empty()) {
            tmp *= fxConversion_[i]->value();
        }
        result += tmp * weights_[i];
    }
    if (!npvCcyConversion_.empty()) {
        result *= npvCcyConversion_->value();
    }
    return result;
}

const std::map<std::string, boost::any>& BondPositionInstrumentWrapper::additionalResults() const {
    static std::map<std::string, boost::any> emptyMap;
    return emptyMap;
}

} // namespace data
} // namespace ore
