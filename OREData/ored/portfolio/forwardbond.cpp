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

#include <boost/lexical_cast.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/forwardbond.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/forwardbond.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/instruments/bonds/zerocouponbond.hpp>
#include <ql/quotes/simplequote.hpp>
#include <qle/instruments/forwardbond.hpp>

#include <boost/make_shared.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void ForwardBond::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("ForwardBond::build() called for trade " << id());

    const boost::shared_ptr<Market> market = engineFactory->market();

    boost::shared_ptr<EngineBuilder> builder_fwd = engineFactory->builder("ForwardBond");
    boost::shared_ptr<EngineBuilder> builder_bd = engineFactory->builder("Bond");

    bondData_.populateFromBondReferenceData(engineFactory->referenceData());

    // check data requirements
    QL_REQUIRE(!bondData_.referenceCurveId().empty(), "reference curve id required");
    QL_REQUIRE(!bondData_.settlementDays().empty(), "settlement days required");

    Date issueDate = parseDate(bondData_.issueDate());
    Calendar calendar = parseCalendar(bondData_.calendar());
    Natural settlementDays = boost::lexical_cast<Natural>(bondData_.settlementDays());

    Date fwdMaturityDate = parseDate(fwdMaturityDate_);
    Real payOff = parseReal(payOff_);
    bool longInBond = parseBool(longInBond_);

    bool settlementDirty = !settlementDirty_.empty() ? parseBool(settlementDirty_) : true;
    Real compensationPayment = parseReal(compensationPayment_);
    Date compensationPaymentDate = parseDate(compensationPaymentDate_);

    QL_REQUIRE(compensationPaymentDate <= fwdMaturityDate, "Premium cannot be paid after forward contract maturity");

    boost::shared_ptr<QuantLib::Bond> bond; // pointer to QuantLib Bond
    // pointer to fwdBond; both are needed. But only the fwdBond equipped with pricer
    boost::shared_ptr<QuantExt::ForwardBond> fwdBond;

    // FIXME: zero bonds are always long (firstLegIsPayer = false, mult = 1.0)
    bool firstLegIsPayer = (bondData_.coupons().size() == 0) ? false : bondData_.coupons()[0].isPayer();
    QL_REQUIRE(firstLegIsPayer == false,
               "The zero bond position must be Long. Specify long/short position of the forward using 'longInBond'");
    QL_REQUIRE(compensationPayment >= 0, "Negative compensation payments are not supported");

    boost::shared_ptr<Payoff> payoff =
        longInBond ? boost::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Long, payOff)
                   : boost::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Short, payOff);
    compensationPayment = longInBond ? compensationPayment : compensationPayment * (-1.0);

    std::vector<Leg> separateLegs;
    if (bondData_.zeroBond()) { // Zero coupon bond
        bond.reset(new QuantLib::ZeroCouponBond(settlementDays, calendar, bondData_.faceAmount(),
                                                parseDate(bondData_.maturityDate())));
    } else { // Coupon bond
        for (Size i = 0; i < bondData_.coupons().size(); ++i) {
            bool legIsPayer = bondData_.coupons()[i].isPayer();
            QL_REQUIRE(legIsPayer == firstLegIsPayer, "Bond legs must all have same pay/receive flag");
            if (i == 0)
                currency_ = bondData_.coupons()[i].currency();
            else {
                QL_REQUIRE(currency_ == bondData_.coupons()[i].currency(),
                           "leg #" << i << " currency (" << bondData_.coupons()[i].currency()
                                   << ") not equal to leg #0 currency (" << bondData_.coupons()[0].currency());
            }
            Leg leg;
            auto configuration = builder_bd->configuration(MarketContext::pricing);
            auto legBuilder = engineFactory->legBuilder(bondData_.coupons()[i].legType());
            leg = legBuilder->buildLeg(bondData_.coupons()[i], engineFactory, requiredFixings_, configuration);
            separateLegs.push_back(leg);
        } // for coupons_
        Leg leg = joinLegs(separateLegs);
        bond.reset(new QuantLib::Bond(settlementDays, calendar, issueDate, leg));
        // workaround, QL doesn't register a bond with its leg's cashflows
        for (auto const& c : leg)
            bond->registerWith(c);
    }

    npvCurrency_ = currency_;
    maturity_ = bond->cashflows().back()->date();
    notional_ = currentNotional(bond->cashflows()) * bondData_.bondNotional();
    notionalCurrency_ = currency_;

    // cashflows will be generated as additional results in the pricing engine
    legs_ = {};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {firstLegIsPayer};

    Currency currency = parseCurrency(currency_);

    fwdBond.reset(new QuantExt::ForwardBond(bond, payoff, fwdMaturityDate, settlementDirty, compensationPayment,
                                            compensationPaymentDate,
                                            bondData_.bondNotional())); // nur bond payoff maturityDate lassen

    boost::shared_ptr<fwdBondEngineBuilder> fwdBondBuilder =
        boost::dynamic_pointer_cast<fwdBondEngineBuilder>(builder_fwd);
    QL_REQUIRE(fwdBondBuilder, "No Builder found for fwdBond: " << id());

    LOG("Calling engine for forward bond " << id() << " with credit curve: " << bondData_.creditCurveId()
                                           << " with reference curve:" << bondData_.referenceCurveId()
                                           << " with income curve: " << bondData_.incomeCurveId());
    fwdBond->setPricingEngine(fwdBondBuilder->engine(id(), currency, bondData_.creditCurveId(), bondData_.securityId(),
                                                     bondData_.referenceCurveId(), bondData_.incomeCurveId()));
    instrument_.reset(new VanillaInstrument(fwdBond, 1.0)); // long or short is regulated via the payoff class
}

void ForwardBond::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fwdBondNode = XMLUtils::getChildNode(node, "ForwardBondData");
    QL_REQUIRE(fwdBondNode, "No ForwardBondData Node");
    bondData_.fromXML(XMLUtils::getChildNode(fwdBondNode, "BondData"));

    XMLNode* fwdSettlementNode = XMLUtils::getChildNode(fwdBondNode, "SettlementData");
    QL_REQUIRE(fwdSettlementNode, "No fwdSettlementNode Node");

    payOff_ = XMLUtils::getChildValue(fwdSettlementNode, "Amount", true);
    fwdMaturityDate_ = XMLUtils::getChildValue(fwdSettlementNode, "ForwardMaturityDate", true);
    settlementDirty_ = XMLUtils::getChildValue(fwdSettlementNode, "SettlementDirty", true);
    XMLNode* fwdPremiumNode = XMLUtils::getChildNode(fwdBondNode, "PremiumData");
    if (fwdPremiumNode) {
        compensationPayment_ = XMLUtils::getChildValue(fwdPremiumNode, "Amount", true);
        compensationPaymentDate_ = XMLUtils::getChildValue(fwdPremiumNode, "Date", true);
    } else {
        compensationPayment_ = "0.0";
        compensationPaymentDate_ = fwdMaturityDate_;
    }

    longInBond_ = XMLUtils::getChildValue(fwdBondNode, "LongInForward", true);
}

XMLNode* ForwardBond::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fwdBondNode = doc.allocNode("ForwardBondData");
    XMLUtils::appendNode(node, fwdBondNode);
    XMLUtils::appendNode(fwdBondNode, bondData_.toXML(doc));

    XMLNode* fwdSettlmentNode = doc.allocNode("SettlementData");
    XMLUtils::appendNode(fwdBondNode, fwdSettlmentNode);
    XMLUtils::addChild(doc, fwdSettlmentNode, "ForwardMaturityDate", fwdMaturityDate_);
    XMLUtils::addChild(doc, fwdSettlmentNode, "Amount", payOff_);
    XMLUtils::addChild(doc, fwdSettlmentNode, "SettlementDirty", settlementDirty_);

    XMLNode* fwdPremiumNode = doc.allocNode("PremiumData");
    XMLUtils::appendNode(fwdBondNode, fwdPremiumNode);
    XMLUtils::addChild(doc, fwdPremiumNode, "Amount", compensationPayment_);
    XMLUtils::addChild(doc, fwdPremiumNode, "Date", compensationPaymentDate_);

    XMLUtils::addChild(doc, fwdBondNode, "LongInForward", longInBond_);

    return node;
}

std::map<AssetClass, std::set<std::string>> ForwardBond::underlyingIndices(const boost::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    result[AssetClass::BOND] = { bondData_.securityId() };
    return result;
}

} // namespace data
} // namespace ore
