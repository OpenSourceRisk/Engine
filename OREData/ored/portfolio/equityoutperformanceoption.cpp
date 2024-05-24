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

#include <ored/utilities/parsers.hpp>

#include <ored/portfolio/barrieroptionwrapper.hpp>
#include <ored/portfolio/builders/equityoutperformanceoption.hpp>
#include <ored/portfolio/equityoutperformanceoption.hpp>
#include <ored/portfolio/legdata.hpp>

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/tradefactory.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>

#include <qle/indexes/equityindex.hpp>

#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/barrieroption.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

EquityOutperformanceOption::EquityOutperformanceOption(Envelope& env, OptionData option, 
            const string& currency, Real notional, const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying1, 
            const QuantLib::ext::shared_ptr<ore::data::Underlying>& underlying2, 
            Real initialPrice1, Real initialPrice2, Real strike, const string& initialPriceCurrency1, const string& initialPriceCurrency2,
            Real knockInPrice, Real knockOutPrice, string fxIndex1, string fxIndex2)
    : Trade("EquityOutperformanceOption", env), option_(option), currency_(currency), amount_(notional), 
        underlying1_(underlying1), underlying2_(underlying2), initialPrice1_(initialPrice1), initialPrice2_(initialPrice2), 
        strikeReturn_(strike), knockInPrice_(knockInPrice), knockOutPrice_(knockOutPrice), initialPriceCurrency1_(initialPriceCurrency1),
        initialPriceCurrency2_(initialPriceCurrency2), fxIndex1_(fxIndex1), fxIndex2_(fxIndex2) {
    }

void EquityOutperformanceOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();

    // Only European exercise supported for now
    QL_REQUIRE(option_.style() == "European", "Option Style unknown: " << option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of excercise dates");
    Currency ccy = parseCurrency(currency_);

    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex1 = nullptr;
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex2 = nullptr;
   
    Real initialPrice1 = initialPrice1_;
    if (initialPriceCurrency1_ != "") {
        Currency ccy1 = parseCurrencyWithMinors(initialPriceCurrency1_);
        initialPrice1 = convertMinorToMajorCurrency(initialPriceCurrency1_, initialPrice1_);
        
        auto eqCurve1 = *engineFactory->market()->equityCurve(name1());
        Currency eqCurrency1 = eqCurve1->currency();
        
        // if the initialPrice differs from the equity price
        if (ccy1 != eqCurrency1) {
            QL_REQUIRE(!fxIndex1_.empty(), "FX settlement index must be specified: " << ccy1 << " " << eqCurrency1 << " " << fxIndex1_);
            fxIndex1 = buildFxIndex(fxIndex1_, eqCurrency1.code(), ccy1.code(), engineFactory->market(),
                                    engineFactory->configuration(MarketContext::pricing));
        }
    }

    Real initialPrice2 = initialPrice2_;
    if (initialPriceCurrency2_ != "") {
        Currency ccy2 = parseCurrencyWithMinors(initialPriceCurrency2_);
        initialPrice2 = convertMinorToMajorCurrency(initialPriceCurrency2_, initialPrice2_);

        auto eqCurve2 = *engineFactory->market()->equityCurve(name2());
        Currency eqCurrency2 = eqCurve2->currency();
        
        // if the initialPrice differs from the equity price
        if (ccy2 != eqCurrency2) {
            QL_REQUIRE(!fxIndex2_.empty(), "FX settlement index must be specified");
            fxIndex2 = buildFxIndex(fxIndex2_, eqCurrency2.code(), ccy2.code(), engineFactory->market(),
                                    engineFactory->configuration(MarketContext::pricing));
        }
    }

    Date valuationDate = parseDate(option_.exerciseDates().front());
    QuantLib::ext::shared_ptr<Exercise> exercise;
    exercise = QuantLib::ext::make_shared<EuropeanExercise>(valuationDate);

    Option::Type optionType = parseOptionType(option_.callPut());
    QuantLib::ext::shared_ptr<Instrument> inst = QuantLib::ext::make_shared<QuantExt::OutperformanceOption>(exercise, optionType, strikeReturn_, initialPrice1, initialPrice2, amount_, knockInPrice_, knockOutPrice_, fxIndex1, fxIndex2);

    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<EquityOutperformanceOptionEngineBuilder> eqOptBuilder = QuantLib::ext::dynamic_pointer_cast<EquityOutperformanceOptionEngineBuilder>(builder);

    inst->setPricingEngine(eqOptBuilder->engine(name1(), name2(), ccy));
    setSensitivityTemplate(*eqOptBuilder);
    
    // Add additional premium payments
    Position::Type positionType = parsePositionType(option_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    Real mult = bsInd;

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    Date lastPremiumDate = addPremiums(additionalInstruments, additionalMultipliers, mult, option_.premiumData(),
                                       -bsInd, ccy, engineFactory, eqOptBuilder->configuration(MarketContext::pricing));

    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(new VanillaInstrument(inst, mult, additionalInstruments, additionalMultipliers));
    npvCurrency_ = currency_;
    maturity_ = std::max(lastPremiumDate, std::max(maturity_, valuationDate));
    notional_ = amount_;
    notionalCurrency_= currency_;
}

void EquityOutperformanceOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityOutperformanceOptionData");
    QL_REQUIRE(eqNode, "No EquityOutperformanceOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(eqNode, "OptionData"));
    knockInPrice_ = Null<Real>();
    if (XMLUtils::getChildNode(eqNode, "KnockInPrice")) {
       knockInPrice_ = XMLUtils::getChildValueAsDouble(eqNode, "KnockInPrice", true);
    }
    knockOutPrice_ = Null<Real>();
    if (XMLUtils::getChildNode(eqNode, "KnockOutPrice")) {
       knockOutPrice_ = XMLUtils::getChildValueAsDouble(eqNode, "KnockOutPrice", true);
    }

    currency_ = XMLUtils::getChildValue(eqNode, "Currency", true);
    amount_ = XMLUtils::getChildValueAsDouble(eqNode, "Notional", true);

    XMLNode* tmp1 = XMLUtils::getChildNode(eqNode, "Underlying1");
    if (!tmp1)
        tmp1 = XMLUtils::getChildNode(eqNode, "Name1");
    UnderlyingBuilder underlyingBuilder1("Underlying1", "Name1");
    underlyingBuilder1.fromXML(tmp1);
    underlying1_ = underlyingBuilder1.underlying();

    XMLNode* fxt1 = XMLUtils::getChildNode(eqNode, "InitialPriceFXTerms1");
    if (fxt1)
        fxIndex1_ = XMLUtils::getChildValue(fxt1, "FXIndex", true);

    XMLNode* tmp2 = XMLUtils::getChildNode(eqNode, "Underlying2");
    if (!tmp2)
        tmp2 = XMLUtils::getChildNode(eqNode, "Name2");
    UnderlyingBuilder underlyingBuilder2("Underlying2", "Name2");
    underlyingBuilder2.fromXML(tmp2);
    underlying2_ = underlyingBuilder2.underlying();

    XMLNode* fxt2 = XMLUtils::getChildNode(eqNode, "InitialPriceFXTerms2");
    if (fxt2)
        fxIndex2_ = XMLUtils::getChildValue(fxt2, "FXIndex", true);

    initialPrice1_ = XMLUtils::getChildValueAsDouble(eqNode, "InitialPrice1", true);
    initialPrice2_ = XMLUtils::getChildValueAsDouble(eqNode, "InitialPrice2", true);
    
    tmp1 = XMLUtils::getChildNode(eqNode, "InitialPriceCurrency1");
    if (tmp1)
        initialPriceCurrency1_ = XMLUtils::getChildValue(eqNode, "InitialPriceCurrency1", true);
    tmp2 = XMLUtils::getChildNode(eqNode, "InitialPriceCurrency2");
    if (tmp2)
        initialPriceCurrency2_ = XMLUtils::getChildValue(eqNode, "InitialPriceCurrency2", true);
    strikeReturn_ = XMLUtils::getChildValueAsDouble(eqNode, "StrikeReturn", true);

}

XMLNode* EquityOutperformanceOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityOutperformanceOptionData");
    XMLUtils::appendNode(node, eqNode);
    XMLUtils::appendNode(eqNode, option_.toXML(doc));
    
    XMLUtils::addChild(doc, eqNode, "Currency", currency_);
    XMLUtils::addChild(doc, eqNode, "Notional", amount_);
    
    XMLUtils::appendNode(eqNode, underlying1_->toXML(doc));
    XMLUtils::appendNode(eqNode, underlying2_->toXML(doc));
    
    XMLUtils::addChild(doc, eqNode, "InitialPrice1", initialPrice1_);
    XMLUtils::addChild(doc, eqNode, "InitialPrice2", initialPrice2_);
    if (initialPriceCurrency1_ != "")
        XMLUtils::addChild(doc, eqNode, "InitialPriceCurrency1", initialPriceCurrency1_);
    if (initialPriceCurrency2_ != "")
        XMLUtils::addChild(doc, eqNode, "InitialPriceCurrency2", initialPriceCurrency2_);
    XMLUtils::addChild(doc, eqNode, "StrikeReturn", strikeReturn_);
    if (knockInPrice_ !=  Null<Real>())
        XMLUtils::addChild(doc, eqNode, "KnockInPrice", knockInPrice_);
    if (knockOutPrice_ !=  Null<Real>())
        XMLUtils::addChild(doc, eqNode, "KnockOutPrice", knockOutPrice_);

    if (fxIndex1_ != "") {
        XMLNode* fxNode = doc.allocNode("InitialPriceFXTerms1");
        XMLUtils::addChild(doc, fxNode, "FXIndex", fxIndex1_);
        XMLUtils::appendNode(eqNode, fxNode);
    }

    if (fxIndex2_ != "") {
        XMLNode* fxNode = doc.allocNode("InitialPriceFXTerms2");
        XMLUtils::addChild(doc, fxNode, "FXIndex", fxIndex2_);
        XMLUtils::appendNode(eqNode, fxNode);
    }
    
    return node;
}

} // namespace data
} // namespace ore
