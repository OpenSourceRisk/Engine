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

#include <ored/portfolio/equityoptionposition.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/portfolio/builders/vanillaoption.hpp>

#include <qle/indexes/genericindex.hpp>

namespace ore {
namespace data {
    
void EquityOptionUnderlyingData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Underlying");
    XMLNode* n = XMLUtils::getChildNode(node, "Underlying");
    QL_REQUIRE(n != nullptr, "EquityOptionUnderlyingData: expected child node Underlying");
    underlying_.fromXML(n);
    n = XMLUtils::getChildNode(node, "OptionData");
    QL_REQUIRE(n != nullptr, "EquityOptionUnderlyingData: expected child node OptionData");
    optionData_.fromXML(n);
    strike_ = XMLUtils::getChildValueAsDouble(node, "Strike");
}

XMLNode* EquityOptionUnderlyingData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("Underlying");
    XMLUtils::appendNode(n, underlying_.toXML(doc));
    XMLUtils::appendNode(n, optionData_.toXML(doc));
    XMLUtils::addChild(doc, n, "Strike", strike_);
    return n;
}

void EquityOptionPositionData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "EquityOptionPositionData");
    quantity_ = XMLUtils::getChildValueAsDouble(node, "Quantity", true);
    auto c = XMLUtils::getChildrenNodes(node, "Underlying");
    underlyings_.clear();
    for (auto const n : c) {
        underlyings_.push_back(EquityOptionUnderlyingData());
        underlyings_.back().fromXML(n);
    }
}

XMLNode* EquityOptionPositionData::toXML(XMLDocument& doc) const {
    XMLNode* n = doc.allocNode("EquityOptionPositionData");
    XMLUtils::addChild(doc, n, "Quantity", quantity_);
    for (auto& u : underlyings_) {
        XMLUtils::appendNode(n, u.toXML(doc));
    }
    return n;
}

void EquityOptionPosition::build(const QuantLib::ext::shared_ptr<ore::data::EngineFactory>& engineFactory) {

    // ISDA taxonomy: not a derivative, but define the asset class at least
    // so that we can determine a TRS asset class that has an EQ position underlying
    additionalData_["isdaAssetClass"] = string("Equity");
    additionalData_["isdaBaseProduct"] = string("");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    DLOG("EquityOptionPosition::build() called for " << id());
    QL_REQUIRE(!data_.underlyings().empty(), "EquityOptionPosition::build(): no underlyings given");
    options_.clear();
    indices_.clear();
    weights_.clear();
    positions_.clear();
    currencies_.clear();
    fxConversion_.clear();

    setSensitivityTemplate(std::string()); // default, will usually be overwritten below

    for (auto const& u : data_.underlyings()) {

        // get equity, populate weight, currency

        auto eq = *engineFactory->market()->equityCurve(u.underlying().name(),
                                                        engineFactory->configuration(MarketContext::pricing));
        weights_.push_back(u.underlying().weight());
        QL_REQUIRE(!eq->currency().empty(),
                   "did not get currency for equity name '" << u.underlying().name() << "', is this set up?");
        currencies_.push_back(eq->currency().code());
        QuantLib::Position::Type pos = parsePositionType(u.optionData().longShort());
        Real posInd = (pos == QuantLib::Position::Long ? 1.0 : -1.0);
        positions_.push_back(posInd);

        // build vanilla option and attach engine

        Option::Type optionType = parseOptionType(u.optionData().callPut());
        QuantLib::Exercise::Type exerciseType = parseExerciseType(u.optionData().style());
        QL_REQUIRE(u.optionData().exerciseDates().size() == 1, "Invalid number of exercise dates");
        Date optionExpiry = parseDate(u.optionData().exerciseDates().front());
        QuantLib::ext::shared_ptr<Exercise> exercise;
        switch (exerciseType) {
        case QuantLib::Exercise::Type::European: {
            exercise = QuantLib::ext::make_shared<EuropeanExercise>(optionExpiry);
            break;
        }
        case QuantLib::Exercise::Type::American: {
            exercise = QuantLib::ext::make_shared<AmericanExercise>(optionExpiry, u.optionData().payoffAtExpiry());
            break;
        }
        default:
            QL_FAIL("Option Style " << u.optionData().style() << " is not supported");
        }
        options_.push_back(QuantLib::ext::make_shared<VanillaOption>(
            QuantLib::ext::make_shared<PlainVanillaPayoff>(optionType, u.strike()), exercise));
        if (!options_.back()->isExpired()) {
            std::string tradeTypeBuilder =
                (exerciseType == QuantLib::Exercise::Type::European ? "EquityOption" : "EquityOptionAmerican");
            QuantLib::ext::shared_ptr<VanillaOptionEngineBuilder> builder =
                QuantLib::ext::dynamic_pointer_cast<VanillaOptionEngineBuilder>(engineFactory->builder(tradeTypeBuilder));
            QL_REQUIRE(builder, "EquityOptionPosition::build(): no engine builder for '" << tradeTypeBuilder << "'");
            options_.back()->setPricingEngine(builder->engine(u.underlying().name(), eq->currency(), optionExpiry));
            setSensitivityTemplate(*builder);
        }

        // populate index for historical prices

        // ensure the strike appears as 2400.2 (i.e. with decimal places as necessary)
        std::ostringstream strikeStr;
        strikeStr << u.strike();

        string underlyingName = u.underlying().name();
        if (engineFactory->referenceData() && engineFactory->referenceData()->hasData("Equity", underlyingName)) {
            const auto& underlyingRef = engineFactory->referenceData()->getData("Equity", underlyingName);
            if (auto equityRef = QuantLib::ext::dynamic_pointer_cast<EquityReferenceDatum>(underlyingRef))
                underlyingName = equityRef->equityData().equityId;
        }
        indices_.push_back(QuantLib::ext::make_shared<QuantExt::GenericIndex>(
            "GENERIC-MD/EQUITY_OPTION/PRICE/" + underlyingName + "/" + eq->currency().code() + "/" +
            ore::data::to_string(optionExpiry) + "/" + strikeStr.str() + "/" +
            (optionType == Option::Call ? "C" : "P"), optionExpiry));
    }

    // get fx quotes

    isSingleCurrency_ = true;
    npvCurrency_ = currencies_.front();
    for (auto const& c : currencies_) {
        // we use fxSpot() as opposed to fxRate() here to ensure consistency between NPV() and the fixing of an
        // equivalent index representing the same basket
        fxConversion_.push_back(
            engineFactory->market()->fxSpot(c + npvCurrency_, engineFactory->configuration(MarketContext::pricing)));
        if (npvCurrency_ != c)
            isSingleCurrency_ = false;
    }

    // set instrument
    auto qlInstr =
        QuantLib::ext::make_shared<EquityOptionPositionInstrumentWrapper>(data_.quantity(), options_, weights_, positions_, fxConversion_);
    qlInstr->setPricingEngine(QuantLib::ext::make_shared<EquityOptionPositionInstrumentWrapperEngine>());
    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(qlInstr);

    // no sensible way to set these members
    maturity_ = Date::maxDate();
    notional_ = Null<Real>();
    notionalCurrency_ = "";

    // leave legs empty
}

void EquityOptionPosition::setNpvCurrencyConversion(const std::string& ccy, const Handle<Quote>& conversion) {
    npvCurrency_ = ccy;
    QuantLib::ext::static_pointer_cast<EquityOptionPositionInstrumentWrapper>(instrument_->qlInstrument())
        ->setNpvCurrencyConversion(conversion);
}

void EquityOptionPosition::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    data_.fromXML(XMLUtils::getChildNode(node, "EquityOptionPositionData"));
}

XMLNode* EquityOptionPosition::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLUtils::appendNode(node, data_.toXML(doc));
    return node;
}

std::map<AssetClass, std::set<std::string>>
EquityOptionPosition::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    for (auto const& u : data_.underlyings()) {
        result[AssetClass::EQ].insert(u.underlying().name());
    }
    return result;
}

EquityOptionPositionInstrumentWrapper::EquityOptionPositionInstrumentWrapper(
    const Real quantity, const std::vector<QuantLib::ext::shared_ptr<QuantLib::VanillaOption>>& options,
    const std::vector<Real>& weights, const std::vector<Real>& positions, const std::vector<Handle<Quote>>& fxConversion)
    : quantity_(quantity), options_(options), weights_(weights), positions_(positions), fxConversion_(fxConversion) {
    QL_REQUIRE(options_.size() == weights_.size(), "EquityOptionPositionInstrumentWrapper: options size ("
                                                       << options_.size() << ") must match weights size ("
                                                       << weights_.size() << ")");
    QL_REQUIRE(fxConversion_.empty() || fxConversion_.size() == options_.size(),
               "EquityPositionInstrumentWrapper: fxConversion size ("
                   << fxConversion_.size() << ") must match options size (" << options_.size() << ")");
    for (auto const& o : options)
        registerWith(o);
    for (auto const& fx : fxConversion)
        registerWith(fx);
}

void EquityOptionPositionInstrumentWrapper::setNpvCurrencyConversion(const Handle<Quote>& npvCcyConversion) {
    unregisterWith(npvCcyConversion_);
    npvCcyConversion_ = npvCcyConversion;
    registerWith(npvCcyConversion_);
    update();
}

bool EquityOptionPositionInstrumentWrapper::isExpired() const {
    for (auto const& o : options_)
        if (!o->isExpired())
            return false;
    return true;
}

void EquityOptionPositionInstrumentWrapper::setupArguments(QuantLib::PricingEngine::arguments* args) const {
    EquityOptionPositionInstrumentWrapper::arguments* a =
        dynamic_cast<EquityOptionPositionInstrumentWrapper::arguments*>(args);
    QL_REQUIRE(a != nullptr, "wrong argument type in EquityOptionPositionInstrumentWrapper");
    a->quantity_ = quantity_;
    a->options_ = options_;
    a->weights_ = weights_;
    a->positions_ = positions_;
    a->fxConversion_ = fxConversion_;
    a->npvCcyConversion_ = npvCcyConversion_;
}

void EquityOptionPositionInstrumentWrapper::fetchResults(const QuantLib::PricingEngine::results* r) const {
    Instrument::fetchResults(r);
}

void EquityOptionPositionInstrumentWrapperEngine::calculate() const {
    Real result = 0.0;
    for (Size i = 0; i < arguments_.options_.size(); ++i) {
        Real tmp = arguments_.quantity_ * arguments_.options_[i]->NPV();
        if (!arguments_.fxConversion_[i].empty()) {
            tmp *= arguments_.fxConversion_[i]->value();
        }
        result += tmp * arguments_.weights_[i] * arguments_.positions_[i];
    }
    if (!arguments_.npvCcyConversion_.empty()) {
        result *= arguments_.npvCcyConversion_->value();
    }
    results_.value = result;
}

} // namespace data
} // namespace ore
