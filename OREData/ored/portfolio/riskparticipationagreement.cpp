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

#include <ored/portfolio/builders/riskparticipationagreement.hpp>
#include <ored/portfolio/riskparticipationagreement.hpp>
#include <ored/scripting/engines/riskparticipationagreementbaseengine.hpp>

#include <qle/instruments/riskparticipationagreement.hpp>
#include <qle/instruments/riskparticipationagreement_tlock.hpp>

#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>

namespace ore {
namespace data {

void RiskParticipationAgreement::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    LOG("RiskParticipationAgreement::build() for id \"" << id() << "\" called.");

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Interest Rate");
    additionalData_["isdaBaseProduct"] = string("Exotic");
    additionalData_["isdaSubProduct"] = string("");  
    additionalData_["isdaTransaction"] = string("");  

    // do some checks

    QL_REQUIRE(!protectionFee_.empty(), "protection fees must not be empty");

    QL_REQUIRE(underlying_.empty() || tlockData_.empty(),
               "RiskParticipationAgreement::build(): both LegData and TreasuryLockData given in Underlying node.");

    if (!underlying_.empty())
        buildWithSwapUnderlying(engineFactory);
    else if (!tlockData_.empty())
        buildWithTlockUnderlying(engineFactory);
    else {
        QL_FAIL("RiskParticipationAgreement::build(): Underlying node must not be empty, LegData or TreasuryLockData "
                "required as subnode");
    }

    // set start date
    additionalData_["startDate"] = to_string(protectionStart_);
}

void RiskParticipationAgreement::buildWithSwapUnderlying(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    npvCurrency_ = notionalCurrency_ = underlying_.front().currency();

    bool isXccy = false;
    for (auto const& u : underlying_) {
        isXccy = isXccy || u.currency() != npvCurrency_;
    }

    for (auto const& p : protectionFee_)
        QL_REQUIRE(p.isPayer() == protectionFee_.front().isPayer(),
                   "the protection fee legs must all be pay or all receive");

    /* determine product variant used to retrieve engine builder

       RiskParticipationAgreement_Vanilla:
       - exactly one fixed and one floating leg with opposite payer flags
       - only fixed, ibor, ois (comp, avg) coupons allowed, no cap / floors, no in arrears fixings for ibor

       RiskParticipationAgreement_Structured:
       - arbitrary number of fixed, floating, cashflow legs
       - only fixed, ibor coupons, ois (comp, avg), simple cashflows allowed, but possibly capped / floored,
         as naked option, with in arrears fixing for ibor
       - with optionData (i.e. callable underlying), as naked option (i.e. swaption)

       RiskParticipationAgreement_Vanilla_XCcy:
       - two legs in different currencies with arbitrary coupons allowed, no optionData though
    */

    std::set<std::string> legTypes;
    std::set<bool> legPayers;
    bool hasCapFloors = false, hasIborInArrears = false;
    for (auto const& l : underlying_) {
        QL_REQUIRE(l.legType() == "Fixed" || l.legType() == "Floating" || l.legType() == "Cashflow",
                   "RiskParticipationAgreement: leg type " << l.legType()
                                                           << " not supported, expected Fixed, Floating, Cashflow");
        legTypes.insert(l.legType());
        legPayers.insert(l.isPayer());
        if (auto c = QuantLib::ext::dynamic_pointer_cast<FloatingLegData>(l.concreteLegData())) {
            hasCapFloors = hasCapFloors || !c->caps().empty();
            hasCapFloors = hasCapFloors || !c->floors().empty();
            hasIborInArrears = hasIborInArrears || (c->isInArrears() && !isOvernightIndex(c->index()));
        }
    }

    std::string productVariant;
    if (isXccy) {
        productVariant = "RiskParticipationAgreement_Vanilla_XCcy";
        QL_REQUIRE(!optionData_, "XCcy Risk Participation Agreement does not allow for OptionData");
    } else if (underlying_.size() == 2 &&
               (legTypes == std::set<std::string>{"Fixed", "Floating"} ||
                legTypes == std::set<std::string>{"Cashflow", "Floating"}) &&
               legPayers == std::set<bool>{false, true} && !hasCapFloors && !hasIborInArrears && !optionData_) {
        productVariant = "RiskParticipationAgreement_Vanilla";
    } else {
        productVariant = "RiskParticipationAgreement_Structured";
    }

    // get engine builder

    DLOG("get engine builder for product variant " << productVariant);
    auto builder = QuantLib::ext::dynamic_pointer_cast<RiskParticipationAgreementEngineBuilderBase>(
        engineFactory->builder(productVariant));
    QL_REQUIRE(builder, "wrong builder, expected RiskParticipationAgreementEngineBuilderBase");
    auto configuration = builder->configuration(MarketContext::pricing);

    // build underlying legs and protection fee legs

    std::vector<Leg> underlyingLegs, protectionFeeLegs;
    std::vector<bool> underlyingPayer, protectionPayer;
    std::vector<string> underlyingCcys, protectionCcys;
    for (auto const& l : underlying_) {
        auto legBuilder = engineFactory->legBuilder(l.legType());
        underlyingLegs.push_back(legBuilder->buildLeg(l, engineFactory, requiredFixings_, configuration));
        underlyingPayer.push_back(l.isPayer());
        underlyingCcys.push_back(l.currency());
        auto leg = buildNotionalLeg(l, underlyingLegs.back(), requiredFixings_, engineFactory->market(), configuration);
        if (!leg.empty()) {
            underlyingLegs.push_back(leg);
            underlyingPayer.push_back(l.isPayer());
            underlyingCcys.push_back(l.currency());
        }
    }
    for (auto const& l : protectionFee_) {
        auto legBuilder = engineFactory->legBuilder(l.legType());
        protectionFeeLegs.push_back(legBuilder->buildLeg(l, engineFactory, requiredFixings_, configuration));
        protectionPayer.push_back(l.isPayer());
        protectionCcys.push_back(protectionFeeLegs.back().empty() ? npvCurrency_ : l.currency());
    }

    // build exercise, if option data is present

    QuantLib::ext::shared_ptr<QuantLib::Exercise> exercise;
    bool exerciseIsLong = true;
    std::vector<QuantLib::ext::shared_ptr<CashFlow>> vectorPremium;
    if (optionData_) {
        ExerciseBuilder eb(*optionData_, underlyingLegs);
        exercise = eb.exercise();
        exerciseIsLong = parsePositionType((*optionData_).longShort()) == QuantLib::Position::Long;      
        for (const auto& premium : (*optionData_).premiumData().premiumData()) {
            QL_REQUIRE((premium.ccy == underlyingCcys[0]) && (premium.ccy == underlyingCcys[1]),
                           "premium currency must be the same than the swaption legs");
            vectorPremium.push_back(QuantLib::ext::make_shared<SimpleCashFlow>(premium.amount, premium.payDate));
        }
    }

    // build ql instrument

    auto qleInstr = QuantLib::ext::make_shared<QuantExt::RiskParticipationAgreement>(
        underlyingLegs, underlyingPayer, underlyingCcys, protectionFeeLegs, protectionPayer.front(), protectionCcys,
        participationRate_, protectionStart_, protectionEnd_, settlesAccrual_, fixedRecoveryRate_, exercise,
        exerciseIsLong, vectorPremium, nakedOption_);

    // wrap instrument

    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(qleInstr);

    // set trade members

    notional_ = 0.0;
    for (Size i = 0; i < underlyingLegs.size(); ++i) {
        notional_ = std::max(notional_, currentNotional(underlyingLegs[i]) *
                                            engineFactory->market()
                                                ->fxRate(underlyingCcys[i] + notionalCurrency_,
                                                         engineFactory->configuration(MarketContext::pricing))
                                                ->value());
    }
    legs_ = underlyingLegs;
    legCurrencies_ = underlyingCcys;
    legPayers_ = underlyingPayer;
    legs_.insert(legs_.end(), protectionFeeLegs.begin(), protectionFeeLegs.end());
    legCurrencies_.insert(legCurrencies_.end(), protectionCcys.begin(), protectionCcys.end());
    legPayers_.insert(legPayers_.end(), protectionPayer.begin(), protectionPayer.end());
    maturity_ = qleInstr->maturity();

    // set pricing engine
    qleInstr->setPricingEngine(builder->engine(id(), this));
    setSensitivityTemplate(*builder);
}

namespace {
DayCounter getDayCounter(const Leg& l) {
    for (auto const& c : l) {
        if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c))
            return cpn->dayCounter();
    }
    QL_FAIL("RiskParticipationAgreement: could not deduce DayCounter from underlying bond, no coupons found in "
            "bond "
            "cashflows ("
            << l.size() << ")");
}

} // namespace

void RiskParticipationAgreement::buildWithTlockUnderlying(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    std::string productVariant = "RiskParticipationAgreement_TLock";

    // get bond reference data and build bond

    tlockData_.bondData() = tlockData_.originalBondData();
    tlockData_.bondData().populateFromBondReferenceData(engineFactory->referenceData());
    ore::data::Bond tmp(Envelope(), tlockData_.bondData());
    tmp.build(engineFactory);
    auto bond = QuantLib::ext::dynamic_pointer_cast<QuantLib::Bond>(tmp.instrument()->qlInstrument());
    QL_REQUIRE(bond != nullptr, "RiskParticipationAgreement: could not build tlock underlying, cast failed (internal "
                                "error that dev needs to look at)");

    // set currency and notional

    npvCurrency_ = notionalCurrency_ = tlockData_.bondData().currency();
    notional_ = tlockData_.bondData().bondNotional();

    // checks

    for (auto const& p : protectionFee_)
        QL_REQUIRE(p.isPayer() == protectionFee_.front().isPayer(),
                   "the protection fee legs must all be pay or all receive");

    // get engine builder

    DLOG("get engine builder for product variant " << productVariant);
    auto builder = QuantLib::ext::dynamic_pointer_cast<RiskParticipationAgreementEngineBuilderBase>(
        engineFactory->builder(productVariant));
    QL_REQUIRE(builder, "wrong builder, expected RiskParticipationAgreementEngineBuilderBase");
    auto configuration = builder->configuration(MarketContext::pricing);

    // build ql instrument and set the pricing engine

    bool payer = tlockData_.payer();
    Real referenceRate = tlockData_.referenceRate();
    DayCounter dayCounter =
        tlockData_.dayCounter().empty() ? getDayCounter(bond->cashflows()) : parseDayCounter(tlockData_.dayCounter());
    Date terminationDate = parseDate(tlockData_.terminationDate());
    Size paymentGap = tlockData_.paymentGap();
    Calendar paymentCalendar = parseCalendar(tlockData_.paymentCalendar());

    std::vector<Leg> protectionFeeLegs;
    std::vector<bool> protectionPayer;
    std::vector<string> protectionCcys;
    for (auto const& l : protectionFee_) {
        auto legBuilder = engineFactory->legBuilder(l.legType());
        protectionFeeLegs.push_back(legBuilder->buildLeg(l, engineFactory, requiredFixings_, configuration));
        protectionPayer.push_back(l.isPayer());
        protectionCcys.push_back(protectionFeeLegs.back().empty() ? npvCurrency_ : l.currency());
    }

    Date paymentDate = paymentCalendar.advance(terminationDate, paymentGap * Days);
    auto qleInstr = QuantLib::ext::make_shared<QuantExt::RiskParticipationAgreementTLock>(
        bond, notional_, payer, referenceRate, dayCounter, terminationDate, paymentDate, protectionFeeLegs,
        protectionPayer.front(), protectionCcys, participationRate_, protectionStart_, protectionEnd_, settlesAccrual_,
        fixedRecoveryRate_);

    // wrap instrument

    instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(qleInstr);

    // set trade members

    legs_ = {bond->cashflows()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {payer};
    legs_.insert(legs_.end(), protectionFeeLegs.begin(), protectionFeeLegs.end());
    legCurrencies_.insert(legCurrencies_.end(), protectionCcys.begin(), protectionCcys.end());
    legPayers_.insert(legPayers_.end(), protectionPayer.begin(), protectionPayer.end());
    maturity_ = qleInstr->maturity();

    // set pricing engine

    qleInstr->setPricingEngine(builder->engine(id(), this));
    setSensitivityTemplate(*builder);
}

void RiskParticipationAgreement::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* n = XMLUtils::getChildNode(node, "RiskParticipationAgreementData");
    QL_REQUIRE(n, "RiskParticipationAgreement::fromXML(): RiskParticipationAgreementData not found");
    participationRate_ = XMLUtils::getChildValueAsDouble(n, "ParticipationRate", true);
    protectionStart_ = parseDate(XMLUtils::getChildValue(n, "ProtectionStart", true));
    protectionEnd_ = parseDate(XMLUtils::getChildValue(n, "ProtectionEnd", true));
    creditCurveId_ = XMLUtils::getChildValue(n, "CreditCurveId", true);
    issuerId_ = XMLUtils::getChildValue(n, "IssuerId", false);                         // defaults to empty string
    settlesAccrual_ = XMLUtils::getChildValueAsBool(n, "SettlesAccrual", false);       // defaults to true
    tryParseReal(XMLUtils::getChildValue(n, "FixedRecoveryRate"), fixedRecoveryRate_); // defaults to null

    underlying_.clear();
    XMLNode* u = XMLUtils::getChildNode(n, "Underlying");
    QL_REQUIRE(u, "RiskParticipationAgreement::fromXML(): Underlying not found");

    vector<XMLNode*> nodes = XMLUtils::getChildrenNodes(u, "LegData");
    for (auto const n : nodes) {
        LegData ld;
        ld.fromXML(n);
        underlying_.push_back(ld);
    }

    if (auto tmp = XMLUtils::getChildNode(u, "OptionData")) {
        optionData_ = OptionData();
        (*optionData_).fromXML(tmp);
    }

    nakedOption_ = XMLUtils::getChildValueAsBool(u, "NakedOption", false, false);

    if (auto tmp = XMLUtils::getChildNode(u, "TreasuryLockData")) {
        tlockData_.fromXML(tmp);
    }

    protectionFee_.clear();
    XMLNode* p = XMLUtils::getChildNode(n, "ProtectionFee");
    QL_REQUIRE(p, "RiskParticipationAgreement::fromXML(): ProtectionFee not found");
    vector<XMLNode*> nodes2 = XMLUtils::getChildrenNodes(p, "LegData");
    for (auto const n : nodes2) {
        LegData ld; // we do not allow ORE+ leg types anyway
        ld.fromXML(n);
        protectionFee_.push_back(ld);
    }
}

XMLNode* RiskParticipationAgreement::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* n = doc.allocNode("RiskParticipationAgreementData");
    XMLUtils::appendNode(node, n);
    XMLUtils::addChild(doc, n, "ParticipationRate", participationRate_);
    XMLUtils::addChild(doc, n, "ProtectionStart", ore::data::to_string(protectionStart_));
    XMLUtils::addChild(doc, n, "ProtectionEnd", ore::data::to_string(protectionEnd_));
    XMLUtils::addChild(doc, n, "CreditCurveId", creditCurveId_);
    XMLUtils::addChild(doc, n, "IssuerId", issuerId_);
    XMLUtils::addChild(doc, n, "SettlesAccrual", settlesAccrual_);
    if (fixedRecoveryRate_ != Null<Real>())
        XMLUtils::addChild(doc, n, "FixedRecoveryRate", fixedRecoveryRate_);
    XMLNode* p = doc.allocNode("ProtectionFee");
    XMLNode* u = doc.allocNode("Underlying");
    XMLUtils::appendNode(n, p);
    XMLUtils::appendNode(n, u);
    if (optionData_)
        XMLUtils::appendNode(u, (*optionData_).toXML(doc));
    if (nakedOption_)
        XMLUtils::addChild(doc, n, "NakedOption", nakedOption_);
    for (auto& l : protectionFee_)
        XMLUtils::appendNode(p, l.toXML(doc));
    for (auto& l : underlying_)
        XMLUtils::appendNode(u, l.toXML(doc));
    if (!tlockData_.empty()) {
        XMLUtils::appendNode(u, tlockData_.toXML(doc));
    }
    return node;
}

} // namespace data
} // namespace ore
