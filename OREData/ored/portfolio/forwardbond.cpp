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

#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/forwardbond.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/forwardbond.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/instruments/forwardbond.hpp>

#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/instruments/bonds/zerocouponbond.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual360.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void ForwardBond::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("ForwardBond::build() called for trade " << id());

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("Forward");
    additionalData_["isdaSubProduct"] = string("Debt");
    additionalData_["isdaTransaction"] = string("");

    additionalData_["currency"] = currency_;

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();

    QuantLib::ext::shared_ptr<EngineBuilder> builder_fwd = engineFactory->builder("ForwardBond");
    QuantLib::ext::shared_ptr<EngineBuilder> builder_bd = engineFactory->builder("Bond");

    bondData_ = originalBondData_;
    bondData_.populateFromBondReferenceData(engineFactory->referenceData());

    npvCurrency_ = currency_ = bondData_.coupons().front().currency();
    notionalCurrency_ = currency_;

    QL_REQUIRE(!bondData_.referenceCurveId().empty(), "reference curve id required");
    QL_REQUIRE(!bondData_.settlementDays().empty(), "settlement days required");

    Date issueDate = parseDate(bondData_.issueDate());
    Calendar calendar = parseCalendar(bondData_.calendar());
    Natural settlementDays = boost::lexical_cast<Natural>(bondData_.settlementDays());

    Date fwdMaturityDate = parseDate(fwdMaturityDate_);
    Date fwdSettlementDate = fwdSettlementDate_.empty() ? fwdMaturityDate : parseDate(fwdSettlementDate_);
    bool isPhysicallySettled;
    if (settlement_ == "Physical" || settlement_.empty())
        isPhysicallySettled = true;
    else if (settlement_ == "Cash")
        isPhysicallySettled = false;
    else {
        QL_FAIL("ForwardBond: invalid settlement '" << settlement_ << "', expected Cash or Physical");
    }
    Real amount = amount_.empty() ? Null<Real>() : parseReal(amount_);
    Real lockRate = lockRate_.empty() ? Null<Real>() : parseReal(lockRate_);
    Real dv01 = dv01_.empty() ? Null<Real>() : parseReal(dv01_);
    DayCounter lockRateDayCounter = lockRateDayCounter_.empty() ? Actual360() : parseDayCounter(lockRateDayCounter_);
    bool settlementDirty = settlementDirty_.empty() ? true : parseBool(settlementDirty_);
    Real compensationPayment = parseReal(compensationPayment_);
    Date compensationPaymentDate = parseDate(compensationPaymentDate_);
    bool longInForward = parseBool(longInForward_);

    QL_REQUIRE((amount == Null<Real>() && lockRate != Null<Real>()) ||
                   (amount != Null<Real>() && lockRate == Null<Real>()),
               "ForwardBond: exactly one of Amount of LockRate must be given");
    QL_REQUIRE(dv01 >= 0.0, "negative DV01 given");
    QL_REQUIRE(compensationPaymentDate <= fwdMaturityDate, "Premium cannot be paid after forward contract maturity");

    if (lockRate != Null<Real>())
        isPhysicallySettled = false;

    QL_REQUIRE(!bondData_.coupons().empty(), "ForwardBond: No LegData given. If you want to represent a zero bond, "
                                             "set it up as a coupon bond with zero fixed rate");

    bool firstLegIsPayer = bondData_.coupons()[0].isPayer();
    QL_REQUIRE(firstLegIsPayer == false, "ForwardBond: The underlying bond must be entered with a receiver leg. Use "
                                         "LongInBond to specify pay direction of forward payoff");
    QL_REQUIRE(compensationPayment > 0.0 || close_enough(compensationPayment, 0.0),
               "ForwardBond: Negative compensation payments ("
                   << compensationPayment
                   << ") are not allowed. Notice that we will ensure that a positive compensation amount will be paid "
                      "by the party being long in the forward contract.");

    QuantLib::ext::shared_ptr<Payoff> payoff;
    if (amount != Null<Real>()) {
        payoff = longInForward ? QuantLib::ext::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Long, amount)
                               : QuantLib::ext::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Short, amount);
    }
    compensationPayment = longInForward ? compensationPayment : -compensationPayment;

    std::vector<Leg> separateLegs;
    for (Size i = 0; i < bondData_.coupons().size(); ++i) {
        Leg leg;
        auto configuration = builder_bd->configuration(MarketContext::pricing);
        auto legBuilder = engineFactory->legBuilder(bondData_.coupons()[i].legType());
        leg = legBuilder->buildLeg(bondData_.coupons()[i], engineFactory, requiredFixings_, configuration);
        separateLegs.push_back(leg);
    }
    Leg leg = joinLegs(separateLegs);
    auto bond = QuantLib::ext::make_shared<QuantLib::Bond>(settlementDays, calendar, issueDate, leg);

    // cashflows will be generated as additional results in the pricing engine
    legs_ = {};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {firstLegIsPayer};
    Currency currency = parseCurrency(currency_);
    maturity_ = bond->cashflows().back()->date();
    notional_ = currentNotional(bond->cashflows()) * bondData_.bondNotional();

    // first ctor is for vanilla fwd bonds, second for tlocks with a lock rate specifying the payoff
    QuantLib::ext::shared_ptr<QuantLib::Instrument> fwdBond =
        payoff ? QuantLib::ext::make_shared<QuantExt::ForwardBond>(bond, payoff, fwdMaturityDate, fwdSettlementDate,
                                                           isPhysicallySettled, settlementDirty, compensationPayment,
                                                           compensationPaymentDate, bondData_.bondNotional())
               : QuantLib::ext::make_shared<QuantExt::ForwardBond>(bond, lockRate, lockRateDayCounter, longInForward,
                                                           fwdMaturityDate, fwdSettlementDate, isPhysicallySettled,
                                                           settlementDirty, compensationPayment,
                                                           compensationPaymentDate, bondData_.bondNotional(), dv01);

    QuantLib::ext::shared_ptr<fwdBondEngineBuilder> fwdBondBuilder =
        QuantLib::ext::dynamic_pointer_cast<fwdBondEngineBuilder>(builder_fwd);
    QL_REQUIRE(fwdBondBuilder, "ForwardBond::build(): could not cast builder: " << id());

    fwdBond->setPricingEngine(fwdBondBuilder->engine(id(), currency, bondData_.creditCurveId(),
                                                     bondData_.hasCreditRisk(), bondData_.securityId(),
                                                     bondData_.referenceCurveId(), bondData_.incomeCurveId()));
    setSensitivityTemplate(*fwdBondBuilder);
    instrument_.reset(new VanillaInstrument(fwdBond, 1.0));

    additionalData_["currentNotional"] = currentNotional(bond->cashflows()) * bondData_.bondNotional();
    additionalData_["originalNotional"] = originalNotional(bond->cashflows()) * bondData_.bondNotional();
}

void ForwardBond::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fwdBondNode = XMLUtils::getChildNode(node, "ForwardBondData");
    QL_REQUIRE(fwdBondNode, "No ForwardBondData Node");
    originalBondData_.fromXML(XMLUtils::getChildNode(fwdBondNode, "BondData"));
    bondData_ = originalBondData_;

    XMLNode* fwdSettlementNode = XMLUtils::getChildNode(fwdBondNode, "SettlementData");
    QL_REQUIRE(fwdSettlementNode, "No fwdSettlementNode Node");

    fwdMaturityDate_ = XMLUtils::getChildValue(fwdSettlementNode, "ForwardMaturityDate", true);
    fwdSettlementDate_ = XMLUtils::getChildValue(fwdSettlementNode, "ForwardSettlementDate", false);
    settlement_ = XMLUtils::getChildValue(fwdSettlementNode, "Settlement", false);
    amount_ = XMLUtils::getChildValue(fwdSettlementNode, "Amount", false);
    lockRate_ = XMLUtils::getChildValue(fwdSettlementNode, "LockRate", false);
    lockRateDayCounter_ = XMLUtils::getChildValue(fwdSettlementNode, "LockRateDayCounter", false);
    settlementDirty_ = XMLUtils::getChildValue(fwdSettlementNode, "SettlementDirty", false);
    dv01_ = XMLUtils::getChildValue(fwdSettlementNode, "dv01", false);

    XMLNode* fwdPremiumNode = XMLUtils::getChildNode(fwdBondNode, "PremiumData");
    if (fwdPremiumNode) {
        compensationPayment_ = XMLUtils::getChildValue(fwdPremiumNode, "Amount", true);
        compensationPaymentDate_ = XMLUtils::getChildValue(fwdPremiumNode, "Date", true);
    } else {
        compensationPayment_ = "0.0";
        compensationPaymentDate_ = fwdMaturityDate_;
    }

    longInForward_ = XMLUtils::getChildValue(fwdBondNode, "LongInForward", true);
}

XMLNode* ForwardBond::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fwdBondNode = doc.allocNode("ForwardBondData");
    XMLUtils::appendNode(node, fwdBondNode);
    XMLUtils::appendNode(fwdBondNode, originalBondData_.toXML(doc));

    XMLNode* fwdSettlementNode = doc.allocNode("SettlementData");
    XMLUtils::appendNode(fwdBondNode, fwdSettlementNode);
    XMLUtils::addChild(doc, fwdSettlementNode, "ForwardMaturityDate", fwdMaturityDate_);
    if (!fwdSettlementDate_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "ForwardSettlementDate", fwdSettlementDate_);
    if (!settlement_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "Settlement", settlement_);
    if (!amount_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "Amount", amount_);
    if (!lockRate_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "LockRate", lockRate_);
    if (!dv01_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "dv01", dv01_);
    if (!lockRateDayCounter_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "LockRateDayCounter", lockRateDayCounter_);
    if (!settlementDirty_.empty())
        XMLUtils::addChild(doc, fwdSettlementNode, "SettlementDirty", settlementDirty_);

    XMLNode* fwdPremiumNode = doc.allocNode("PremiumData");
    XMLUtils::appendNode(fwdBondNode, fwdPremiumNode);
    XMLUtils::addChild(doc, fwdPremiumNode, "Amount", compensationPayment_);
    XMLUtils::addChild(doc, fwdPremiumNode, "Date", compensationPaymentDate_);

    XMLUtils::addChild(doc, fwdBondNode, "LongInForward", longInForward_);

    return node;
}

std::map<AssetClass, std::set<std::string>>
ForwardBond::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    result[AssetClass::BOND] = {bondData_.securityId()};
    return result;
}

} // namespace data
} // namespace ore
