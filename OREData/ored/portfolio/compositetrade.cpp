/*
  Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/portfolio/compositetrade.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/tradefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/instruments/multiccycompositeinstrument.hpp>

#include <boost/algorithm/string/case_conv.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {
using QuantExt::MultiCcyCompositeInstrument;

void CompositeTrade::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("Building Composite Trade: " << id());
    npvCurrency_ = currency_;
    QuantLib::ext::shared_ptr<MultiCcyCompositeInstrument> compositeInstrument =
	QuantLib::ext::make_shared<MultiCcyCompositeInstrument>();
    fxRates_.clear();
    fxRatesNotional_.clear();
    legs_.clear();

    populateFromReferenceData(engineFactory->referenceData());

    
    for (const QuantLib::ext::shared_ptr<Trade>& trade : trades_) {

	    trade->reset();
	    trade->build(engineFactory);
	    trade->validate();

        if (sensitivityTemplate_.empty())
            setSensitivityTemplate(trade->sensitivityTemplate());

        Handle<Quote> fx = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
	    if (trade->npvCurrency() != npvCurrency_)
	        fx = engineFactory->market()->fxRate(trade->npvCurrency() + npvCurrency_);
	    fxRates_.push_back(fx);

        Handle<Quote> fxNotional = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
        if (trade->notionalCurrency().empty()) {
            // trade is not guaranteed to provide a non-null notional, but if it does we require a notional currency
            if (trade->notional() != Null<Real>()) {
                StructuredTradeErrorMessage(
                    trade, "Error building composite trade '" + id() + "'",
                    "Component trade '" + trade->id() + "' does not provide notional currency for notional " +
                                                std::to_string(trade->notional()) + ". Assuming " + npvCurrency_ + ".")
                    .log();
            }
        } else if (trade->notionalCurrency() != npvCurrency_)
            fxNotional = engineFactory->market()->fxRate(trade->notionalCurrency() + npvCurrency_);
        fxRatesNotional_.push_back(fxNotional);

        QuantLib::ext::shared_ptr<InstrumentWrapper> instrumentWrapper = trade->instrument();
        Real effectiveMultiplier = instrumentWrapper->multiplier();
        if (auto optionWrapper = QuantLib::ext::dynamic_pointer_cast<ore::data::OptionWrapper>(instrumentWrapper)) {
	        effectiveMultiplier *= optionWrapper->isLong() ? 1.0 : -1.0;
	    }

	    compositeInstrument->add(instrumentWrapper->qlInstrument(), effectiveMultiplier, fx);
	    for (Size i = 0; i < instrumentWrapper->additionalInstruments().size(); ++i) {
	        compositeInstrument->add(instrumentWrapper->additionalInstruments()[i],
				         instrumentWrapper->additionalMultipliers()[i]);
	    }

        bool isDuplicate = false;
        try {
            if (instrumentWrapper->additionalResults().find("cashFlowResults") != trade->instrument()->additionalResults().end())
                isDuplicate = true;
        } catch (...) {}
        if (!isDuplicate) {
            // For cashflows
            legs_.insert(legs_.end(), trade->legs().begin(), trade->legs().end());
            legPayers_.insert(legPayers_.end(), trade->legPayers().begin(), trade->legPayers().end());
            legCurrencies_.insert(legCurrencies_.end(), trade->legCurrencies().begin(), trade->legCurrencies().end());
        }

	    maturity_ = std::max(maturity_, trade->maturity());
    }
    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(new VanillaInstrument(compositeInstrument));

    notionalCurrency_ = npvCurrency_;

    // set required fixings
    for (auto const& t : trades_)
        requiredFixings_.addData(t->requiredFixings());
}

QuantLib::Real CompositeTrade::notional() const {
    vector<Real> notionals;
    vector<Handle<Quote>> fxRates;
    // trade is not guaranteed to provide a non-null notional
    for (const QuantLib::ext::shared_ptr<Trade>& trade : trades_)
        notionals.push_back(trade->notional() != Null<Real>() ? trade->notional() : 0.0);

    // need to convert the component notionals to the composite currency.
    auto notionalConverter = [](const Real ntnl, const Handle<Quote>& fx) { return (ntnl * fx->value()); };
    std::transform(begin(notionals), end(notionals), begin(fxRates_), begin(notionals), notionalConverter);
    return calculateNotional(notionals);
}

void CompositeTrade::fromXML(XMLNode* node) {
    QL_REQUIRE("CompositeTrade" == XMLUtils::getChildValue(node, "TradeType", true),
               "Wrong trade type in composite trade builder.");
    Trade::fromXML(node);
    this->id() = XMLUtils::getAttribute(node, "id");
    // We read the data particular to composite trades
    XMLNode* compNode = XMLUtils::getChildNode(node, "CompositeTradeData");
    QL_REQUIRE(compNode, "Could not find CompositeTradeData node.");
    currency_ = XMLUtils::getChildValue(compNode, "Currency", true);
    // The notional logic is as follows:
    // If the notional override is specified then it is used, regardless of the "NotionalCalculation" field
    // Otherwise we calculate the notional as per the calculation specified
    if (XMLUtils::getChildNode(compNode, "NotionalOverride") != nullptr) {
        notionalOverride_ = XMLUtils::getChildValueAsDouble(compNode, "NotionalOverride");
        QL_REQUIRE(notionalOverride_ >= 0, "Non-negative notional expected.");
        DLOG("Using override notional of " << notionalOverride_);
        notionalCalculation_ = "Override";
    } else {
        // Convert everything to proper case to match xml schema
        notionalCalculation_ = boost::to_lower_copy(XMLUtils::getChildValue(compNode, "NotionalCalculation"));
        notionalCalculation_[0] = toupper(notionalCalculation_[0]);
        QL_REQUIRE(notionalCalculation_ != "Override", "Notional override value has not been provided.");
    }

    if (XMLUtils::getChildNode(compNode, "PortfolioBasket")) {
        portfolioBasket_ = XMLUtils::getChildValueAsBool(node, "PortfolioBasket", false);
    } else {
        portfolioBasket_ = false;
    }

    portfolioId_ = XMLUtils::getChildValue(compNode, "BasketName", false);

    XMLNode* tradesNode = XMLUtils::getChildNode(compNode, "Components");
    if (portfolioBasket_ && portfolioId_.empty()) {
        QL_REQUIRE(tradesNode, "Required a Portfolio Id or a Components Node.");
    }
    if ((portfolioBasket_ && portfolioId_.empty()) || (!portfolioBasket_)) {
    
        vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(tradesNode, "Trade");
        for (Size i = 0; i < nodes.size(); i++) {
            string tradeType = XMLUtils::getChildValue(nodes[i], "TradeType", true);
            string id = XMLUtils::getAttribute(nodes[i], "id");
            if (id == "") {
                WLOG("Empty component trade id being overwritten in composite trade " << this->id() << ".");
            }
            id = this->id() + "_" + std::to_string(i);
            DLOG("Parsing composite trade " << this->id() << " node " << i << " with id: " << id);

            QuantLib::ext::shared_ptr<Trade> trade;
            try {
                trade = TradeFactory::instance().build(tradeType);
                trade->id() = id;
                Envelope componentEnvelope;
                if (XMLNode* envNode = XMLUtils::getChildNode(nodes[i], "Envelope")) {
                    componentEnvelope.fromXML(envNode);
                }
                Envelope env = this->envelope();
                // the component trade's envelope is the main trade's envelope with possibly overwritten add fields
                for (auto const& [k, v] : componentEnvelope.fullAdditionalFields()) {
                    env.setAdditionalField(k,v);
                }
                    
                trade->setEnvelope(env);
                trade->fromXML(nodes[i]);
                trades_.push_back(trade);
                DLOG("Added Trade " << id << " (" << trade->id() << ")"
                                    << " type:" << tradeType << " to composite trade " << this->id() << ".");
            } catch (const std::exception& e) {
                StructuredTradeErrorMessage(
                    id, this->tradeType(),
                    "Failed to build subtrade with id '" + id + "' inside composite trade: ", e.what())
                    .log();
            }
        }
        LOG("Finished Parsing XML doc");
    }
}

XMLNode* CompositeTrade::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* compNode = doc.allocNode("CompositeTradeData");
    XMLUtils::appendNode(node, compNode);

    XMLUtils::addChild(doc, compNode, "Currency", currency_);
    if (notionalCalculation_ == "Override")
        XMLUtils::addChild(doc, compNode, "NotionalOverride", notionalOverride_);
    XMLUtils::addChild(doc, compNode, "NotionalCalculation", notionalCalculation_);

    XMLNode* tradesNode = doc.allocNode("Components");
    XMLUtils::appendNode(compNode, tradesNode);
    for (auto trade : trades_) {
        XMLNode* subTradeNode = trade->toXML(doc);
        XMLUtils::appendNode(tradesNode, subTradeNode);
    }
    return node;
}

Real CompositeTrade::calculateNotional(const vector<Real>& notionals) const {
    if (notionalCalculation_ == "Sum" || notionalCalculation_ == "")
        return std::accumulate(notionals.begin(), notionals.end(), 0.0);
    else if (notionalCalculation_ == "Mean" || notionalCalculation_ == "Average")
        return std::accumulate(notionals.begin(), notionals.end(), 0.0) / notionals.size();
    else if (notionalCalculation_ == "First")
        return notionals[0];
    else if (notionalCalculation_ == "Last")
        return notionals.back();
    else if (notionalCalculation_ == "Min")
        return *std::min_element(notionals.begin(), notionals.end());
    else if (notionalCalculation_ == "Max")
        return *std::min_element(notionals.begin(), notionals.end());
    else if (notionalCalculation_ == "Override")
        return notionalOverride_;
    else
        QL_FAIL("Unsupported notional calculation type.");
}

map<string, RequiredFixings::FixingDates> CompositeTrade::fixings(const Date& settlementDate) const {

    map<string, RequiredFixings::FixingDates>  result;
    for (const auto& t : trades_) {
        auto fixings = t->fixings(settlementDate);
        for (const auto& [indexName, fixingDates] : fixings) {
            result[indexName].addDates(fixingDates);
        }
    }
    return result;
}

std::map<AssetClass, std::set<std::string>>
CompositeTrade::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    
    map<AssetClass, std::set<std::string>> result;
    for (const auto& t : trades_) {
        auto underlyings = t->underlyingIndices(referenceDataManager);
        for (const auto& kv : underlyings) {
            result[kv.first].insert(kv.second.begin(), kv.second.end());
        }
    }
    return result;
}

const std::map<std::string, boost::any>& CompositeTrade::additionalData() const {
    additionalData_.clear();
    Size counter = 0;
    for (auto const& t : trades_) {
        for (auto const& d : t->additionalData()) {
            additionalData_[d.first + "_" + std::to_string(counter)] = d.second;
        }
        ++counter;
    }
    return additionalData_;
}

void CompositeTrade::populateFromReferenceData(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData) {

    if (!portfolioId_.empty() && referenceData != nullptr &&
        (referenceData->hasData(PortfolioBasketReferenceDatum::TYPE, portfolioId_))) {
        auto ptfRefData = QuantLib::ext::dynamic_pointer_cast<PortfolioBasketReferenceDatum>(
                referenceData->getData(PortfolioBasketReferenceDatum::TYPE, portfolioId_));
            QL_REQUIRE(ptfRefData, "could not cast to PortfolioBasketReferenceDatum, this is unexpected");
            getTradesFromReferenceData(ptfRefData);
    } else {
        DLOG("Could not get PortfolioBasketReferenceDatum for Id " << portfolioId_ << " leave data in trade unchanged");
    }
   
}

void CompositeTrade::getTradesFromReferenceData(
    const QuantLib::ext::shared_ptr<PortfolioBasketReferenceDatum>& ptfReferenceDatum) {

    DLOG("populating portfolio basket data from reference data");
    QL_REQUIRE(ptfReferenceDatum, "populateFromReferenceData(): empty cbo reference datum given");

    auto refData = ptfReferenceDatum->getTrades();
    trades_.clear();
    for (Size i = 0; i < refData.size(); i++) {
        trades_.push_back(refData[i]);
    }
    LOG("Finished Parsing XML doc");

}

} // namespace data
} // namespace oreplus
