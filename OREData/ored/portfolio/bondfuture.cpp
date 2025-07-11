/*
 Copyright (C) 2025 Quaternion Risk Management Ltd

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

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/bondfuture.hpp>
#include <ored/portfolio/builders/forwardbond.hpp>
#include <ored/portfolio/legdata.hpp>

#include <qle/instruments/forwardbond.hpp>

#include <ql/currencies/america.hpp>
#include <ql/currencies/asia.hpp>
#include <ql/time/calendars/china.hpp>
#include <ql/time/calendars/unitedstates.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void BondFuture::build(const ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("BondFuture::build() called for trade " << id());

    // ISDA taxonomy https://www.isda.org/a/20EDE/q4-2011-credit-standardisation-legend.pdf
    // TODO: clarify ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("Other");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("ForwardBond");
    QL_REQUIRE(builder, "BondFuture::build(): internal error, forward bond engine builder is null");

    // if data not given explicitly, exploit bond future reference data
    populateFromBondFutureReferenceData(engineFactory->referenceData());
    checkData();

    // if dates are not given explicitly or by referenceData, deduce dates from contractMonth/deliverableGrade
    Date expiry = parseDate(lastTrading_);
    Date settlementDate = parseDate(lastDelivery_);
    if (expiry == Date() || settlementDate == Date())
        deduceDates(expiry, settlementDate);
    checkDates(expiry, settlementDate);

    // identify CTD bond
    QL_REQUIRE(secList_.size() > 0, "BondFuture::build no DeliveryBasket given");
    auto res = identifyCtdBond(engineFactory, expiry);
    ctdId_ = res.first;
    double ctdCf = res.second;
    ctdUnderlying_ = BondFactory::instance().build(engineFactory, engineFactory->referenceData(), ctdId_);

    // handle strike treatment
    fairPriceBool_ = true; // fallback: strike = settlement price --> MtM = 0.0
    if (!fairPrice_.empty())
        fairPriceBool_ = parseBool(fairPrice_);
    double amount = 0.0;
    if (fairPriceBool_)
        amount = getSettlementPriceFuture(engineFactory) * contractNotional_;

    // more flags with fallbacks
    // supposed to be physical
    bool isPhysicallySettled;
    if (settlement_ == "Physical" || settlement_.empty())
        isPhysicallySettled = true;
    else if (settlement_ == "Cash")
        isPhysicallySettled = false;
    else {
        QL_FAIL("ForwardBond: invalid settlement '" << settlement_ << "', expected Cash or Physical");
    }

    // supposed to be clean
    bool settlementDirty = settlementDirty_.empty() ? false : parseBool(settlementDirty_);

    // hardcoded values for bondfuture vs forward bond
    double compensationPayment = 0.0;      // no compensation payments for bondfutures
    Date compensationPaymentDate = Date(); // no compensation payments for bondfutures

    // define bondspread id, differentiate for purpose spreadimply (ctd id) or not (future id)
    string bondSpreadId = contractName_;
    auto it = engineFactory->engineData()->globalParameters().find("RunType");
    if (it != engineFactory->engineData()->globalParameters().end() && it->second == "BondSpreadImply")
        bondSpreadId = ctdId_;
    DLOG("BondFuture::build -- bondSpreadId " << bondSpreadId);

    // create quantext forward bond instrument
    ext::shared_ptr<Payoff> payoff = parsePositionType(longShort_) == Position::Long
                                         ? ext::make_shared<ForwardBondTypePayoff>(Position::Long, amount)
                                         : ext::make_shared<ForwardBondTypePayoff>(Position::Short, amount);

    ext::shared_ptr<Instrument> fwdBond =
        ext::make_shared<ForwardBond>(ctdUnderlying_.bond, payoff, expiry, settlementDate, isPhysicallySettled,
                                      settlementDirty, compensationPayment, compensationPaymentDate, contractNotional_);

    ext::shared_ptr<FwdBondEngineBuilder> fwdBondBuilder = ext::dynamic_pointer_cast<FwdBondEngineBuilder>(builder);
    QL_REQUIRE(fwdBondBuilder, "BondFuture::build(): could not cast FwdBondEngineBuilder: " << id());

    fwdBond->setPricingEngine(fwdBondBuilder->engine(
        id(), parseCurrency(currency_), bondSpreadId, envelope().additionalField("discount_curve", false, string()),
        ctdUnderlying_.creditCurveId, ctdId_, ctdUnderlying_.bondData.referenceCurveId(),
        ctdUnderlying_.bondData.incomeCurveId(), settlementDirty, ctdCf));

    setSensitivityTemplate(*fwdBondBuilder);
    addProductModelEngine(*fwdBondBuilder);
    instrument_.reset(new VanillaInstrument(fwdBond, 1.0));

    maturity_ = settlementDate;
    maturityType_ = "Contract settled";
    npvCurrency_ = currency_;
    notional_ = contractNotional_;
    legs_ = vector<Leg>(1, ctdUnderlying_.bond->cashflows()); // presenting the cashflows of the underlying
    legCurrencies_ = vector<string>(1, currency_);
    legPayers_ = vector<bool>(1, false);
}

void BondFuture::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* bondFutureNode = XMLUtils::getChildNode(node, "BondFutureData");
    QL_REQUIRE(bondFutureNode, "No BondFutureData Node");

    contractName_ = XMLUtils::getChildValue(bondFutureNode, "ContractName", true);
    contractNotional_ = XMLUtils::getChildValueAsDouble(bondFutureNode, "ContractNotional", true);
    longShort_ = XMLUtils::getChildValue(bondFutureNode, "LongShort", true);

    secList_.clear();
    // only via reference data allowed
    // XMLNode* basket = XMLUtils::getChildNode(bondFutureNode, "DeliveryBasket");
    // if (basket) {
    //     for (XMLNode* child = XMLUtils::getChildNode(basket, "SecurityId"); child;
    //          child = XMLUtils::getNextSibling(child))
    //         secList_.push_back(XMLUtils::getNodeValue(child));
    // }
    // currency_ = XMLUtils::getChildValue(bondFutureNode, "Currency", false);
    // contractMonth_ = XMLUtils::getChildValue(bondFutureNode, "ContractMonth", false);
    // deliverableGrade_ = XMLUtils::getChildValue(bondFutureNode, "DeliverableGrade", false);
    // fairPrice_ = XMLUtils::getChildValue(bondFutureNode, "FairPrice", false);
    // settlement_ = XMLUtils::getChildValue(bondFutureNode, "Settlement", false);
    // settlementDirty_ = XMLUtils::getChildValue(bondFutureNode, "SettlementDirty", false);
    // rootDate_ = XMLUtils::getChildValue(bondFutureNode, "RootDate", false);
    // expiryBasis_ = XMLUtils::getChildValue(bondFutureNode, "ExpiryBasis", false);
    // settlementBasis_ = XMLUtils::getChildValue(bondFutureNode, "SettlementBasis", false);
    // expiryLag_ = XMLUtils::getChildValue(bondFutureNode, "ExpiryLag", false);
    // settlementLag_ = XMLUtils::getChildValue(bondFutureNode, "SettlementLag", false);

    // lastTrading_ = XMLUtils::getChildValue(bondFutureNode, "LastTradingDate", false);
    // lastDelivery_ = XMLUtils::getChildValue(bondFutureNode, "LastDeliveryDate", false);
}

XMLNode* BondFuture::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("BondFutureData");

    XMLUtils::addChild(doc, node, "ContractName", contractName_);
    XMLUtils::addChild(doc, node, "ContractNotional", contractNotional_);
    XMLUtils::addChild(doc, node, "LongShort", longShort_);

    // only via reference data allowed
    // XMLUtils::addChild(doc, node, "Currency", currency_);
    // XMLUtils::addChild(doc, node, "ContractMonth", contractMonth_);
    // XMLUtils::addChild(doc, node, "DeliverableGrade", deliverableGrade_);
    // XMLUtils::addChild(doc, node, "FairPrice", fairPrice_);
    // XMLUtils::addChild(doc, node, "Settlement", settlement_);
    // XMLUtils::addChild(doc, node, "SettlementDirty", settlementDirty_);
    // XMLUtils::addChild(doc, node, "RootDate", rootDate_);
    // XMLUtils::addChild(doc, node, "ExpiryBasis", expiryBasis_);
    // XMLUtils::addChild(doc, node, "SettlementBasis", settlementBasis_);
    // XMLUtils::addChild(doc, node, "ExpiryLag", expiryLag_);
    // XMLUtils::addChild(doc, node, "SettlementLag", settlementLag_);

    // XMLUtils::addChild(doc, node, "LastTradingDate", lastTrading_);
    // XMLUtils::addChild(doc, node, "LastDeliveryDate", lastDelivery_);

    // XMLNode* dbNode = XMLUtils::addChild(doc, node, "DeliveryBasket");
    // for (auto& sec : secList_)
    //     XMLUtils::addChild(doc, dbNode, "SecurityId", sec);

    return node;
}

void BondFuture::checkData() {

    QL_REQUIRE(!contractName_.empty(), "BondFuture invalid: no contractName given");
    // contractNotional_, longShort_ already checked in fromXML

    vector<string> missingElements;
    if (currency_.empty())
        missingElements.push_back("Currency");
    if (deliverableGrade_.empty())
        missingElements.push_back("DeliverableGrade");

    // we can deduce lastTrading_/lastDelivery_ given DeliverableGrade/ContractMonth
    // expiryLag_, settlementLag_ covered in method where it is used
    if (lastTrading_.empty() && lastDelivery_.empty()) {
        if (contractMonth_.empty())
            missingElements.push_back("ContractMonth");
        if (rootDate_.empty())
            missingElements.push_back("RootDate");
        if (expiryBasis_.empty())
            missingElements.push_back("ExpiryBasis");
        if (settlementBasis_.empty())
            missingElements.push_back("SettlementBasis");
        if (expiryLag_.empty())
            missingElements.push_back("ExpiryLag");
        if (settlementLag_.empty())
            missingElements.push_back("SettlementLag");
    }

    if (secList_.size() < 1)
        missingElements.push_back("DeliveryBasket");

    QL_REQUIRE(missingElements.empty(), "BondFuture invalid: missing " + boost::algorithm::join(missingElements, ", ") +
                                                " - check if reference data is set up for '"
                                            << contractName_ << "'");
}

pair<string, double> BondFuture::identifyCtdBond(const ext::shared_ptr<EngineFactory>& engineFactory,
                                                 const Date& expiry) {

} // BondFuture::ctdBond

void BondFuture::populateFromBondFutureReferenceData(const ext::shared_ptr<ReferenceDataManager>& referenceData) {
    QL_REQUIRE(!contractName_.empty(), "BondFuture::populateFromBondReferenceData(): no contract name given");

    if (!referenceData || !referenceData->hasData(BondFutureReferenceDatum::TYPE, contractName_)) {
        // DLOG("could not get BondFutureReferenceDatum for name " << contractName_ << " leave data in trade
        // unchanged");
        QL_FAIL("could not get BondFutureReferenceDatum for name " << contractName_ << " trade build failed");
    } else {
        auto bondFutureRefData = ext::dynamic_pointer_cast<BondFutureReferenceDatum>(
            referenceData->getData(BondFutureReferenceDatum::TYPE, contractName_));
        QL_REQUIRE(bondFutureRefData, "could not cast to BondReferenceDatum, this is unexpected");
        DLOG("Got BondFutureReferenceDatum for name " << contractName_ << " overwrite empty elements in trade");
        ore::data::populateFromBondFutureReferenceData(currency_, contractMonth_, deliverableGrade_, fairPrice_,
                                                       settlement_, settlementDirty_, rootDate_, expiryBasis_,
                                                       settlementBasis_, expiryLag_, settlementLag_, lastTrading_,
                                                       lastDelivery_, secList_, bondFutureRefData);
    }
}

} // namespace data
} // namespace ore
