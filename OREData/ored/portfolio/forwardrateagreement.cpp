/*
 Copyright (C) 2017, 2023 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/forwardrateagreement.hpp>
#include <ored/utilities/parsers.hpp>
#include <qle/cashflows/iborfracoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

void ForwardRateAgreement::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("FRA");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();

    Date startDate = parseDate(startDate_);
    Date endDate = parseDate(endDate_);
    Position::Type positionType = parsePositionType(longShort_);
    auto index = market->iborIndex(index_);
    
    QuantLib::ext::shared_ptr<FloatingRateCoupon> cpn;
    if (auto overnightIndex = QuantLib::ext::dynamic_pointer_cast<QuantLib::OvernightIndex>(*index)) {
        cpn = QuantLib::ext::make_shared<QuantExt::OvernightIndexedCoupon>(endDate, amount_, startDate, endDate, overnightIndex,
                                                                   1.0, -strike_);
        cpn->setPricer(QuantLib::ext::make_shared<QuantExt::OvernightIndexedCouponPricer>());
    } else {
        bool useIndexedCoupon = true;
        cpn = QuantLib::ext::make_shared<QuantExt::IborFraCoupon>(startDate, endDate, amount_, *index, strike_);
        cpn->setPricer(QuantLib::ext::make_shared<BlackIborCouponPricer>(
            Handle<OptionletVolatilityStructure>(), BlackIborCouponPricer::TimingAdjustment::Black76,
            Handle<Quote>(ext::shared_ptr<Quote>(new SimpleQuote(1.0))), useIndexedCoupon));
    }
    legs_.push_back({cpn});

    Currency npvCcy = parseCurrency(currency_);
    legCurrencies_.push_back(npvCcy.code());
    legPayers_ = vector<bool>(1, positionType == Position::Type::Short);
    isXCCY_ = false;
    notional_ = amount_;
    npvCurrency_ = npvCcy.code();
    notionalCurrency_ = npvCcy.code();

    QuantLib::ext::shared_ptr<QuantLib::Swap> swap(new QuantLib::Swap(legs_, legPayers_));
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("Swap");
    QuantLib::ext::shared_ptr<SwapEngineBuilderBase> swapBuilder = QuantLib::ext::dynamic_pointer_cast<SwapEngineBuilderBase>(builder);
    QL_REQUIRE(swapBuilder, "No Builder found for Swap " << id());
    swap->setPricingEngine(swapBuilder->engine(npvCcy, std::string(), std::string()));
    setSensitivityTemplate(*swapBuilder);
    instrument_.reset(new VanillaInstrument(swap));
    maturity_ = endDate;
}

void ForwardRateAgreement::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fNode = XMLUtils::getChildNode(node, "ForwardRateAgreementData");
    startDate_ = XMLUtils::getChildValue(fNode, "StartDate", true);
    endDate_ = XMLUtils::getChildValue(fNode, "EndDate", true);
    currency_ = XMLUtils::getChildValue(fNode, "Currency", true);
    index_ = XMLUtils::getChildValue(fNode, "Index", true);
    longShort_ = XMLUtils::getChildValue(fNode, "LongShort", true);
    strike_ = XMLUtils::getChildValueAsDouble(fNode, "Strike", true);
    amount_ = XMLUtils::getChildValueAsDouble(fNode, "Notional", true);
}

XMLNode* ForwardRateAgreement::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fNode = doc.allocNode("ForwardRateAgreementData");
    XMLUtils::appendNode(node, fNode);
    XMLUtils::addChild(doc, fNode, "StartDate", startDate_);
    XMLUtils::addChild(doc, fNode, "EndDate", endDate_);
    XMLUtils::addChild(doc, fNode, "Currency", currency_);
    XMLUtils::addChild(doc, fNode, "Index", index_);
    XMLUtils::addChild(doc, fNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, fNode, "Strike", strike_);
    XMLUtils::addChild(doc, fNode, "Notional", amount_);
    return node;
}
} // namespace data
} // namespace ore
