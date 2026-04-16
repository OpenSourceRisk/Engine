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

#include <ored/portfolio/equityautodeltahedgeoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>
#include <qle/instruments/equityautodeltahedgedoption.hpp>
#include <qle/pricingengines/analyticeuropeanengineautodeltahedge.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void EquityAutoDeltaHedgedOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Equity");
    additionalData_["isdaBaseProduct"] = string("Option");
    additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    additionalData_["isdaTransaction"] = string("");
    additionalData_["hedgingVolatility"] = hedgingVol_;
    additionalData_["forwardRate"] = forwardRate_;
    additionalData_["observationStartDate"] = ore::data::to_string(observationStartDate_);

    QL_REQUIRE(!underlyings_.empty(),
               "EquityAutoDeltaHedgedOption: no underlyings specified for trade " << id());

    // All underlyings share the same equity name and currency — use the first
    string assetName = underlyings_.front().equityUnderlying.name();
    Currency ccy = parseCurrency(underlyings_.front().currency);
    npvCurrency_ = notionalCurrency_ = ccy.code();

    const QuantLib::ext::shared_ptr<Market>& market = engineFactory->market();
    string config = engineFactory->configuration(MarketContext::pricing);

    // Build the instrument batches
    std::vector<QuantExt::UnderlyingOptionBatch> batches;
    notional_ = 0.0;
    maturity_ = Date::minDate();

    for (size_t i = 0; i < underlyings_.size(); ++i) {
        const auto& u = underlyings_[i];

        QL_REQUIRE(u.optionData.exerciseDates().size() == 1,
                   "EquityAutoDeltaHedgedOption: need exactly one exercise date for underlying " << i
                                                                                                << " in trade " << id());

        Date expiryDate = parseDate(u.optionData.exerciseDates().front());
        maturity_ = std::max(maturity_, expiryDate);

        QL_REQUIRE(observationStartDate_ < expiryDate,
                   "EquityAutoDeltaHedgedOption: observation start date " << observationStartDate_
                   << " must be before expiry date " << expiryDate << " for underlying " << i
                   << " in trade " << id());

        Real K = u.strike.value();
        notional_ += K * u.quantity;

        Option::Type type = parseOptionType(u.optionData.callPut());

        auto premData = u.optionData.premiumData().premiumData();
        QL_REQUIRE(premData.size() == 1, "EquityAutoDeltaHedgedOption: expected exactly one premium per underlying, got "
                                             << premData.size() << " for underlying " << i << " in trade " << id());
        auto pd = premData.front();
        QL_REQUIRE(pd.amount != Null<Real>(), "Invalid premium data for underlying " << i);
        Real premAmount = convertMinorToMajorCurrency(pd.ccy, pd.amount);
        Currency premCcy = parseCurrencyWithMinors(pd.ccy);

        QuantExt::UnderlyingOptionBatch batch;
        batch.type = type;
        batch.strike = K;
        batch.quantity = u.quantity;
        batch.premium = premAmount;
        batch.premiumCurrency = premCcy;
        batch.expiryDate = expiryDate;
        batches.push_back(batch);
    }

    // Create the QuantExt instrument
    auto instrument = QuantLib::ext::make_shared<QuantExt::EquityAutoDeltaHedgedOption>(
        batches, hedgingVol_, forwardRate_, observationStartDate_, assetName, ccy);

    // Attach the pricing engine with market data handles
    Handle<Quote> spot = market->equitySpot(assetName, config);
    Handle<YieldTermStructure> discountCurve = market->discountCurve(ccy.code(), config);
    Handle<YieldTermStructure> divCurve = market->equityDividendCurve(assetName, config);
    Handle<YieldTermStructure> fcstCurve = market->equityForecastCurve(assetName, config);
    Handle<BlackVolTermStructure> marketVol = market->equityVol(assetName, config);
    Handle<QuantExt::EquityIndex2> eqIndex = market->equityCurve(assetName, config);

    instrument->setPricingEngine(QuantLib::ext::make_shared<QuantExt::AnalyticEuropeanEngineAutoDeltaHedge>(
        spot, discountCurve, divCurve, fcstCurve, marketVol, eqIndex));

    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(new VanillaInstrument(instrument));

    setSensitivityTemplate(std::string());
}

void EquityAutoDeltaHedgedOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);

    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityAutoDeltaHedgedOptionData");
    QL_REQUIRE(eqNode, "No EquityAutoDeltaHedgedOptionData node for trade " << id());

    hedgingVol_ = XMLUtils::getChildValueAsDouble(eqNode, "Volatility", true);
    forwardRate_ = XMLUtils::getChildValueAsDouble(eqNode, "ForwardRate", true);

    string obsStartStr = XMLUtils::getChildValue(eqNode, "ObservationStartDate", true);
    observationStartDate_ = parseDate(obsStartStr);

    XMLNode* underlyingsNode = XMLUtils::getChildNode(eqNode, "Underlyings");
    QL_REQUIRE(underlyingsNode, "No Underlyings node in EquityAutoDeltaHedgedOptionData for trade " << id());

    vector<XMLNode*> underlyingNodes = XMLUtils::getChildrenNodes(underlyingsNode, "Underlying");
    QL_REQUIRE(!underlyingNodes.empty(), "No Underlying nodes in Underlyings for trade " << id());

    underlyings_.clear();
    for (auto* uNode : underlyingNodes) {
        UnderlyingOptionData u;

        XMLNode* optNode = XMLUtils::getChildNode(uNode, "OptionData");
        QL_REQUIRE(optNode, "No OptionData in Underlying element for trade " << id());
        u.optionData.fromXML(optNode);

        // Parse the equity underlying name
        string nameStr = XMLUtils::getChildValue(uNode, "Name", true);
        u.equityUnderlying = EquityUnderlying(nameStr);
        u.currency = XMLUtils::getChildValue(uNode, "Currency", true);
        u.strike.fromXML(uNode, true, false);
        u.strikeCurrency = XMLUtils::getChildValue(uNode, "StrikeCurrency", false);
        u.quantity = XMLUtils::getChildValueAsDouble(uNode, "Quantity", true);
        underlyings_.push_back(u);
    }
}

XMLNode* EquityAutoDeltaHedgedOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityAutoDeltaHedgedOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::addChild(doc, eqNode, "Volatility", hedgingVol_);
    XMLUtils::addChild(doc, eqNode, "ForwardRate", forwardRate_);

    XMLNode* underlyingsNode = doc.allocNode("Underlyings");
    XMLUtils::appendNode(eqNode, underlyingsNode);

    for (const auto& u : underlyings_) {
        XMLNode* uNode = doc.allocNode("Underlying");
        XMLUtils::appendNode(underlyingsNode, uNode);

        XMLUtils::appendNode(uNode, u.optionData.toXML(doc));
        XMLUtils::addChild(doc, uNode, "Name", u.equityUnderlying.name());
        XMLUtils::addChild(doc, uNode, "Currency", u.currency);
        XMLUtils::appendNode(uNode, u.strike.toXML(doc));
        if (!u.strikeCurrency.empty())
            XMLUtils::addChild(doc, uNode, "StrikeCurrency", u.strikeCurrency);
        XMLUtils::addChild(doc, uNode, "Quantity", u.quantity);
    }

    XMLUtils::addChild(doc, eqNode, "ObservationStartDate", ore::data::to_string(observationStartDate_));

    return node;
}

std::map<AssetClass, std::set<std::string>>
EquityAutoDeltaHedgedOption::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::set<std::string> names;
    for (const auto& u : underlyings_)
        names.insert(u.equityUnderlying.name());
    return {{AssetClass::EQ, names}};
}

} // namespace data
} // namespace ore
