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

#include <ored/portfolio/equityposition.hpp>

#include <ored/portfolio/tradefactory.hpp>

#include <qle/indexes/equityindex.hpp>

namespace ore {
namespace data {

void EquityPositionData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "EquityPositionData");
    quantity_ = XMLUtils::getChildValueAsDouble(node, "Quantity", true);
    auto c = XMLUtils::getChildrenNodes(node, "Underlying");
    underlyings_.clear();
    for (auto const n : c) {
        underlyings_.push_back(EquityUnderlying());
        underlyings_.back().fromXML(n);
    }
}

XMLNode* EquityPositionData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("EquityPositionData");
    XMLUtils::addChild(doc, n, "Quantity", quantity_);
    for (auto& u : underlyings_) {
        XMLUtils::appendNode(n, u.toXML(doc));
    }
    return n;
}

void EquityPosition::build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) {

    // ISDA taxonomy: not a derivative, but define the asset class at least
    // so that we can determine a TRS asset class that has an EQ position underlying
    additionalData_["isdaAssetClass"] = string("Equity");
    additionalData_["isdaBaseProduct"] = string("");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    DLOG("EquityPosition::build() called for " << id());
    QL_REQUIRE(!data_.underlyings().empty(), "EquityPosition::build(): no underlyings given");
    indices_.clear();
    weights_.clear();
    fxConversion_.clear();

    std::vector<std::string> currencies;
    for (auto const& u : data_.underlyings()) {
        indices_.push_back(
            *engineFactory->market()->equityCurve(u.name(), engineFactory->configuration(MarketContext::pricing)));
        weights_.push_back(u.weight());
        QL_REQUIRE(!indices_.back()->currency().empty(),
                   "did not get currency for equity name '" << u.name() << "', is this set up?");
        currencies.push_back(indices_.back()->currency().code());
    }

    // get fx quotes

    isSingleCurrency_ = true;
    npvCurrency_ = currencies.front();
    for (auto const& c : currencies) {
        // we use fxSpot() as opposed to fxRate() here to ensure consistency between NPV() and the fixing of an
        // equivalent index representing the same basket
        fxConversion_.push_back(
            engineFactory->market()->fxSpot(c + npvCurrency_, engineFactory->configuration(MarketContext::pricing)));
        if (npvCurrency_ != c)
            isSingleCurrency_ = false;
    }

    // set instrument
    auto qlInstr =
        QuantLib::ext::make_shared<EquityPositionInstrumentWrapper>(data_.quantity(), indices_, weights_, fxConversion_);
    qlInstr->setPricingEngine(QuantLib::ext::make_shared<EquityPositionInstrumentWrapperEngine>());
    setSensitivityTemplate(std::string());
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(qlInstr);

    // no sensible way to set these members
    maturity_ = Date::maxDate();
    notional_ = Null<Real>();
    notionalCurrency_ = "";
    
    // leave legs empty
}

void EquityPosition::setNpvCurrencyConversion(const std::string& ccy, const Handle<Quote>& conversion) {
    npvCurrency_ = ccy;
    QuantLib::ext::static_pointer_cast<EquityPositionInstrumentWrapper>(instrument_->qlInstrument())
        ->setNpvCurrencyConversion(conversion);
}

void EquityPosition::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    data_.fromXML(XMLUtils::getChildNode(node, "EquityPositionData"));
}

XMLNode* EquityPosition::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLUtils::appendNode(node, data_.toXML(doc));
    return node;
}

std::map<AssetClass, std::set<std::string>>
EquityPosition::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    for (auto const& u : data_.underlyings()) {
        result[AssetClass::EQ].insert(u.name());
    }
    return result;
}

EquityPositionInstrumentWrapper::EquityPositionInstrumentWrapper(
    const Real quantity, const std::vector<QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>>& equities,
    const std::vector<Real>& weights, const std::vector<Handle<Quote>>& fxConversion)
    : quantity_(quantity), equities_(equities), weights_(weights), fxConversion_(fxConversion) {
    QL_REQUIRE(equities_.size() == weights_.size(), "EquityPositionInstrumentWrapper: equities size ("
                                                        << equities_.size() << ") must match weights size ("
                                                        << weights_.size() << ")");
    QL_REQUIRE(fxConversion_.empty() || fxConversion_.size() == equities_.size(),
               "EquityPositionInstrumentWrapper: fxConversion size ("
                   << fxConversion_.size() << ") must match equities size (" << equities_.size() << ")");
    for (auto const& i : equities_)
        registerWith(i);
    for (auto const& f : fxConversion_)
        registerWith(f);
    registerWith(npvCcyConversion_);
}

void EquityPositionInstrumentWrapper::setNpvCurrencyConversion(const Handle<Quote>& npvCcyConversion) {
    npvCcyConversion_ = npvCcyConversion;
}

bool EquityPositionInstrumentWrapper::isExpired() const { return false; }

void EquityPositionInstrumentWrapper::setupExpired() const { Instrument::setupExpired(); }

void EquityPositionInstrumentWrapper::setupArguments(PricingEngine::arguments* args) const {
    EquityPositionInstrumentWrapper::arguments* a = dynamic_cast<EquityPositionInstrumentWrapper::arguments*>(args);
    QL_REQUIRE(a != nullptr, "wrong argument type in EquityPositionInstrumentWrapper");
    a->quantity_ = quantity_;
    a->equities_ = equities_;
    a->weights_ = weights_;
    a->fxConversion_ = fxConversion_;
    a->npvCcyConversion_ = npvCcyConversion_;
}

void EquityPositionInstrumentWrapper::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);
}

void EquityPositionInstrumentWrapperEngine::calculate() const {
    Real result = 0.0;
    for (Size i = 0; i < arguments_.equities_.size(); ++i) {
        Real tmp = arguments_.quantity_ * arguments_.equities_[i]->equitySpot()->value();
        if (!arguments_.fxConversion_[i].empty()) {
            tmp *= arguments_.fxConversion_[i]->value();
        }
        result += tmp * arguments_.weights_[i];
    }
    if (!arguments_.npvCcyConversion_.empty()) {
        result *= arguments_.npvCcyConversion_->value();
    }
    results_.value = result;
}

} // namespace data
} // namespace ore
