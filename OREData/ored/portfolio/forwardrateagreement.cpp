/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ored/portfolio/forwardrateagreement.hpp>

#include <qle/indexes/fallbackiborindex.hpp>

#include <ql/instruments/forwardrateagreement.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

void ForwardRateAgreement::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    const boost::shared_ptr<Market> market = engineFactory->market();

    Date startDate = parseDate(startDate_);
    Date endDate = parseDate(endDate_);
    Position::Type positionType = parsePositionType(longShort_);

    Handle<YieldTermStructure> discountTS = market->discountCurve(currency_);
    Handle<QuantLib::IborIndex> index = market->iborIndex(index_);

    boost::shared_ptr<QuantLib::ForwardRateAgreement> fra(
        new QuantLib::ForwardRateAgreement(startDate, endDate, positionType, strike_, amount_, *index, discountTS));
    instrument_.reset(new VanillaInstrument(fra));
    npvCurrency_ = currency_;
    maturity_ = endDate;
    instrument_->qlInstrument()->update();
    notional_ = amount_;
    notionalCurrency_ = currency_;
    // the QL instrument reads the fixing in setupExpired() (bug?), so we don't add a payment date here to be safe
    requiredFixings_.addFixingDate(fra->fixingDate(), index_);
    // add required fixings for an Ibor fallback index
    if (auto fallback = boost::dynamic_pointer_cast<QuantExt::FallbackIborIndex>(*index)) {
        requiredFixings_.addFixingDates(fallback->onCoupon(fra->fixingDate())->fixingDates(),
                                        engineFactory->iborFallbackConfig().fallbackData(index_).rfrIndex);
    }

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("FRA");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");
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

XMLNode* ForwardRateAgreement::toXML(XMLDocument& doc) {
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
