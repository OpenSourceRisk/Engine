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

#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxswap.hpp>
#include <ored/utilities/log.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <qle/instruments/fxforward.hpp>

using ore::data::XMLUtils;
using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void FxSwap::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    Currency nearBoughtCcy = data::parseCurrency(nearBoughtCurrency_);
    Currency nearSoldCcy = data::parseCurrency(nearSoldCurrency_);
    Date nearDate = data::parseDate(nearDate_);
    Date farDate = data::parseDate(farDate_);

    try {
        DLOG("FxSwap::build() called for trade " << id());
        boost::shared_ptr<QuantLib::Instrument> instNear;
        // builds two fxforwards and sums the npvs
        // so that both npvs are in the same currency, the value of the first forward is taken to be the negative of the
        // counterparty's npv
        // npv_total= -npv1+npv2
        instNear.reset(
            new QuantExt::FxForward(nearSoldAmount_, nearSoldCcy, nearBoughtAmount_, nearBoughtCcy, nearDate, false));
        boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("FxForward");
        QL_REQUIRE(builder, "No builder found for " << tradeType_);
        boost::shared_ptr<FxForwardEngineBuilder> fxBuilder =
            boost::dynamic_pointer_cast<FxForwardEngineBuilder>(builder);
        instNear->setPricingEngine(fxBuilder->engine(nearSoldCcy, nearBoughtCcy));
        boost::shared_ptr<QuantLib::Instrument> instFar;
        instFar.reset(
            new QuantExt::FxForward(farBoughtAmount_, nearSoldCcy, farSoldAmount_, nearBoughtCcy, farDate, false));
        instFar->setPricingEngine(fxBuilder->engine(nearSoldCcy, nearBoughtCcy));

        DLOG("FxSwap::build(): Near NPV = " << instNear->NPV());
        DLOG("FxSwap::build(): Far NPV = " << instFar->NPV());
        // TODO: cannot use a CompositeInstrument
        boost::shared_ptr<CompositeInstrument> composite(new CompositeInstrument());
        composite->add(instNear, -1.0);
        composite->add(instFar, 1.0);
        instrument_.reset(new VanillaInstrument(composite));
        npvCurrency_ = nearBoughtCurrency_;
        notional_ = nearBoughtAmount_;
        maturity_ = farDate;

    } catch (std::exception& e) {
        instrument_.reset();
        throw e;
    }
    // Set up Legs
    legs_.resize(4);
    legCurrencies_.resize(4);
    legPayers_.resize(4);
    legs_[0].push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(nearBoughtAmount_, nearDate)));
    legs_[1].push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(nearSoldAmount_, nearDate)));
    legs_[2].push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(farBoughtAmount_, farDate)));
    legs_[3].push_back(boost::shared_ptr<CashFlow>(new SimpleCashFlow(farSoldAmount_, farDate)));

    legCurrencies_[0] = nearBoughtCurrency_;
    legCurrencies_[1] = nearSoldCurrency_;
    legCurrencies_[2] = nearSoldCurrency_;
    legCurrencies_[3] = nearBoughtCurrency_;

    legPayers_[0] = false;
    legPayers_[1] = true;
    legPayers_[2] = false;
    legPayers_[3] = true;

    DLOG("FxSwap leg 0: " << nearDate_ << " " << legs_[0][0]->amount());
    DLOG("FxSwap leg 1: " << nearDate_ << " " << legs_[1][0]->amount());
    DLOG("FxSwap leg 2: " << farDate_ << " " << legs_[2][0]->amount());
    DLOG("FxSwap leg 3: " << farDate_ << " " << legs_[3][0]->amount());
}

void FxSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxSwapData");
    nearDate_ = XMLUtils::getChildValue(fxNode, "NearDate", true);
    farDate_ = XMLUtils::getChildValue(fxNode, "FarDate", true);
    nearBoughtCurrency_ = XMLUtils::getChildValue(fxNode, "NearBoughtCurrency", true);
    nearSoldCurrency_ = XMLUtils::getChildValue(fxNode, "NearSoldCurrency", true);
    nearBoughtAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "NearBoughtAmount", true);
    nearSoldAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "NearSoldAmount", true);
    farBoughtAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "FarBoughtAmount", true);
    farSoldAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "FarSoldAmount", true);
}

XMLNode* FxSwap::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fxNode = doc.allocNode("FxSwapData");
    XMLUtils::appendNode(node, fxNode);
    XMLUtils::addChild(doc, fxNode, "NearDate", nearDate_);
    XMLUtils::addChild(doc, fxNode, "FarDate", farDate_);
    XMLUtils::addChild(doc, fxNode, "NearBoughtCurrency", nearBoughtCurrency_);
    XMLUtils::addChild(doc, fxNode, "NearBoughtAmount", nearBoughtAmount_);
    XMLUtils::addChild(doc, fxNode, "NearSoldCurrency", nearSoldCurrency_);
    XMLUtils::addChild(doc, fxNode, "NearSoldAmount", nearSoldAmount_);
    XMLUtils::addChild(doc, fxNode, "FarBoughtAmount", farBoughtAmount_);
    XMLUtils::addChild(doc, fxNode, "FarSoldAmount", farSoldAmount_);

    return node;
}
}
}
