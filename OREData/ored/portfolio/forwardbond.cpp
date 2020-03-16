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

namespace {
Leg joinLegs(const std::vector<Leg>& legs) {
    Leg masterLeg;
    for (Size i = 0; i < legs.size(); ++i) {
        // check if the periods of adjacent legs are consistent
        if (i > 0) {
            auto lcpn = boost::dynamic_pointer_cast<Coupon>(legs[i - 1].back());
            auto fcpn = boost::dynamic_pointer_cast<Coupon>(legs[i].front());
            QL_REQUIRE(lcpn, "joinLegs: expected coupon as last cashflow in leg #" << (i - 1));
            QL_REQUIRE(fcpn, "joinLegs: expected coupon as first cashflow in leg #" << i);
            QL_REQUIRE(lcpn->accrualEndDate() == fcpn->accrualStartDate(),
                       "joinLegs: accrual end date of last coupon in leg #"
                           << (i - 1) << " (" << lcpn->accrualEndDate()
                           << ") is not equal to accrual start date of first coupon in leg #" << i << " ("
                           << fcpn->accrualStartDate() << ")");
        }
        // copy legs together
        masterLeg.insert(masterLeg.end(), legs[i].begin(), legs[i].end());
    }
    return masterLeg;
}
} // namespace

void ForwardBond::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("ForwardBond::build() called for trade " << id());

    // Clear the separateLegs_ member here. Should be done in reset() but it is not virtual
    separateLegs_.clear();

    const boost::shared_ptr<Market> market = engineFactory->market();

    boost::shared_ptr<EngineBuilder> builder_fwd = engineFactory->builder("ForwardBond");
    boost::shared_ptr<EngineBuilder> builder_bd = engineFactory->builder("Bond");

    Date issueDate = parseDate(issueDate_);
    Date fwdMaturityDate = parseDate(fwdMaturityDate_);
    Real payOff = parseReal(payOff_);
    bool longInBond = parseBool(longInBond_);
    Calendar calendar = parseCalendar(calendar_);

    bool settlementDirty = !settlementDirty_.empty() ? parseBool(settlementDirty_) : true;
    Real compensationPayment = parseReal(compensationPayment_);
    Date compensationPaymentDate = parseDate(compensationPaymentDate_);

    QL_REQUIRE(compensationPaymentDate <= fwdMaturityDate, "Premium cannot be paid after forward contract maturity");

    Natural settlementDays = boost::lexical_cast<Natural>(settlementDays_);
    boost::shared_ptr<QuantLib::Bond> bond; // pointer to QuantLib Bond
    boost::shared_ptr<QuantExt::ForwardBond>
        fwdBond; // pointer to fwdBond; both are needed. But only the fwdBond equipped with pricer
    std::vector<boost::shared_ptr<IborIndex>> indexes;
    std::vector<boost::shared_ptr<OptionletVolatilityStructure>> ovses;

    // FIXME: zero bonds are always long (firstLegIsPayer = false, mult = 1.0)
    bool firstLegIsPayer = (coupons_.size() == 0) ? false : coupons_[0].isPayer();
    QL_REQUIRE(firstLegIsPayer == false,
               "The zero bond position must be Long. Specify long/short position of the forward using 'longInBond'");
    QL_REQUIRE(compensationPayment >= 0,
               "Negative compensation payments are not supported");

    boost::shared_ptr<Payoff> payoff =
        longInBond ? boost::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Long, payOff)
                   : boost::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Short, payOff);
    compensationPayment = longInBond ? compensationPayment : compensationPayment * (-1.0);

    if (zeroBond_) { // Zero coupon bond
        bond.reset(new QuantLib::ZeroCouponBond(settlementDays, calendar, faceAmount_, parseDate(maturityDate_)));
    } else { // Coupon bond
        for (Size i = 0; i < coupons_.size(); ++i) {
            bool legIsPayer = coupons_[i].isPayer();
            QL_REQUIRE(legIsPayer == firstLegIsPayer, "Bond legs must all have same pay/receive flag");
            if (i == 0)
                currency_ = coupons_[i].currency();
            else {
                QL_REQUIRE(currency_ == coupons_[i].currency(), "leg #" << i << " currency (" << coupons_[i].currency()
                                                                        << ") not equal to leg #0 currency ("
                                                                        << coupons_[0].currency());
            }
            Leg leg;
            auto configuration = builder_bd->configuration(MarketContext::pricing);
            auto legBuilder = engineFactory->legBuilder(coupons_[i].legType());
            leg = legBuilder->buildLeg(coupons_[i], engineFactory, configuration);
            separateLegs_.push_back(leg);

            // Initialise the set of [index name, leg] index pairs
            for (const auto& index : coupons_[i].indices()) {
                nameIndexPairs_.insert(make_pair(index, separateLegs_.size() - 1));
            }

        } // for coupons_
        Leg leg = joinLegs(separateLegs_);
        bond.reset(new QuantLib::Bond(settlementDays, calendar, issueDate, leg));
        // workaround, QL doesn't register a bond with its leg's cashflows
        for (auto const& c : leg)
            bond->registerWith(c);
    }

    npvCurrency_ = currency_;
    maturity_ = bond->cashflows().back()->date();
    notional_ = currentNotional(bond->cashflows());
    notionalCurrency_ = currency_;

    // Add legs (only 1)
    legs_ = {bond->cashflows()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {firstLegIsPayer};

    Currency currency = parseCurrency(currency_);

    fwdBond.reset(new QuantExt::ForwardBond(bond, payoff, fwdMaturityDate, settlementDirty, compensationPayment,
                                            compensationPaymentDate)); // nur bond payoff maturityDate lassen

    boost::shared_ptr<fwdBondEngineBuilder> fwdBondBuilder =
        boost::dynamic_pointer_cast<fwdBondEngineBuilder>(builder_fwd);
    QL_REQUIRE(fwdBondBuilder, "No Builder found for fwdBond: " << id());

    LOG("Calling engine for forward bond " << id() << " with credit curve: " << creditCurveId_
                                           << " with reference curve:" << referenceCurveId_
                                           << " with income curve: " << incomeCurveId_);
    fwdBond->setPricingEngine(fwdBondBuilder->engine(id(), currency, creditCurveId_, securityId_, referenceCurveId_,
                                                     incomeCurveId_, adjustmentSpread_));
    instrument_.reset(new VanillaInstrument(fwdBond, 1.0)); // long or short is regulated via the payoff class
}

map<string, set<Date>> ForwardBond::fixings(const Date& settlementDate) const {

    map<string, set<Date>> result;

    for (const auto& nameIndexPair : nameIndexPairs_) {
        // For clarity
        string indexName = nameIndexPair.first;
        Size legNumber = nameIndexPair.second;

        // Get the set of fixing dates for the  [index name, leg index] pair
        set<Date> dates = fixingDates(separateLegs_[legNumber], settlementDate);

        // Update the results with the fixing dates.
        if (!dates.empty())
            result[indexName].insert(dates.begin(), dates.end());
    }

    return result;
}

void ForwardBond::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fwdBondNode = XMLUtils::getChildNode(node, "ForwardBondData");
    QL_REQUIRE(fwdBondNode, "No ForwardBondData Node");
    XMLNode* bondNode = XMLUtils::getChildNode(fwdBondNode, "BondData");
    QL_REQUIRE(bondNode, "No bondData Node");
    issuerId_ = XMLUtils::getChildValue(bondNode, "IssuerId", false);
    creditCurveId_ =
        XMLUtils::getChildValue(bondNode, "CreditCurveId", false); // issuer credit term structure not mandatory
    securityId_ = XMLUtils::getChildValue(bondNode, "SecurityId", true);
    referenceCurveId_ = XMLUtils::getChildValue(bondNode, "ReferenceCurveId", false);
    incomeCurveId_ = XMLUtils::getChildValue(bondNode, "IncomeCurveId", false);
    settlementDays_ = XMLUtils::getChildValue(bondNode, "SettlementDays", false);
    calendar_ = XMLUtils::getChildValue(bondNode, "Calendar", false);
    issueDate_ = XMLUtils::getChildValue(bondNode, "IssueDate", false);
    XMLNode* legNode = XMLUtils::getChildNode(bondNode, "LegData");
    while (legNode != nullptr) {
        coupons_.push_back(LegData());
        coupons_.back().fromXML(legNode);
        legNode = XMLUtils::getNextSibling(legNode, "LegData");
    }

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
    XMLNode* bondNode = doc.allocNode("BondData");
    XMLUtils::appendNode(fwdBondNode, bondNode);
    XMLUtils::addChild(doc, bondNode, "IssuerId", issuerId_);
    XMLUtils::addChild(doc, bondNode, "CreditCurveId", creditCurveId_);
    XMLUtils::addChild(doc, bondNode, "SecurityId", securityId_);
    XMLUtils::addChild(doc, bondNode, "ReferenceCurveId", referenceCurveId_);
    XMLUtils::addChild(doc, bondNode, "IncomeCurveId", incomeCurveId_);
    XMLUtils::addChild(doc, bondNode, "SettlementDays", settlementDays_);
    XMLUtils::addChild(doc, bondNode, "Calendar", calendar_);
    XMLUtils::addChild(doc, bondNode, "IssueDate", issueDate_);

    for (auto& c : coupons_)
        XMLUtils::appendNode(bondNode, c.toXML(doc));

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
} // namespace data
} // namespace ore
