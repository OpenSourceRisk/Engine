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
#include <ored/portfolio/bondtotalreturnswap.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/bondtotalreturnswap.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/instruments/bonds/zerocouponbond.hpp>
#include <ql/quotes/simplequote.hpp>
#include <qle/instruments/bondtotalreturnswap.hpp>

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

void BondTRS::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("BondTRS::build() called for trade " << id());

    // Clear the separateLegs_ member here. Should be done in reset() but it is not virtual
    separateLegs_.clear();

    const boost::shared_ptr<Market> market = engineFactory->market();

    boost::shared_ptr<EngineBuilder> builder_trs = engineFactory->builder("BondTRS");
    boost::shared_ptr<EngineBuilder> builder_bd = engineFactory->builder("Bond");

    Schedule schedule_;
    schedule_ = makeSchedule(scheduleData_);
    Date issueDate = parseDate(issueDate_);
    Calendar calendar = parseCalendar(calendar_);
    bool longInBond = parseBool(longInBond_);
    Natural settlementDays = boost::lexical_cast<Natural>(settlementDays_);

    Leg fundingLeg;
    auto configuration = builder_bd->configuration(MarketContext::pricing);
    auto legBuilder = engineFactory->legBuilder(fundingLegData_.legType());
    fundingLeg = legBuilder->buildLeg(fundingLegData_, engineFactory, configuration);

    boost::shared_ptr<QuantLib::Bond> bond; // pointer to QuantLib Bond
    boost::shared_ptr<QuantExt::BondTRS>
        bondTRS; // pointer to bondTRS; both are needed. But only the bondTRS equipped with pricer
    std::vector<boost::shared_ptr<IborIndex>> indexes;
    std::vector<boost::shared_ptr<OptionletVolatilityStructure>> ovses;

    // FIXME: zero bonds are always long (firstLegIsPayer = false, mult = 1.0)
    bool firstLegIsPayer = (coupons_.size() == 0) ? false : coupons_[0].isPayer();
    QL_REQUIRE(firstLegIsPayer == false,
               "Require long position in bond. Specify long/short position of forward in longInBond");

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
    maturity_ = schedule_.endDate();
    Date start_ = schedule_.startDate();
    notional_ = currentNotional(bond->cashflows());

    // Add legs (only 1)
    legs_ = {bond->cashflows()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {firstLegIsPayer};

    Currency currency = parseCurrency(currency_);

    bondTRS.reset(new QuantExt::BondTRS(bond, fundingLeg, schedule_, start_, maturity_, longInBond));

    boost::shared_ptr<trsBondEngineBuilder> trsBondBuilder =
        boost::dynamic_pointer_cast<trsBondEngineBuilder>(builder_trs);
    QL_REQUIRE(trsBondBuilder, "No Builder found for BondTRS: " << id());

    LOG("Calling engine for bond trs " << id() << " with credit curve: " << creditCurveId_
                                       << " with reference curve:" << referenceCurveId_);

    bondTRS->setPricingEngine(
        trsBondBuilder->engine(currency, creditCurveId_, securityId_, referenceCurveId_, adjustmentSpread_));
    instrument_.reset(new VanillaInstrument(bondTRS, 1.0)); // long or short is regulated inside pricing engine
}

map<string, set<Date>> BondTRS::fixings(const Date& settlementDate) const {

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

void BondTRS::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* bondTRSNode = XMLUtils::getChildNode(node, "BondTRSData");
    QL_REQUIRE(bondTRSNode, "No BondTRSData Node");
    XMLNode* bondNode = XMLUtils::getChildNode(bondTRSNode, "BondData");
    QL_REQUIRE(bondNode, "No bondData Node");
    creditCurveId_ =
        XMLUtils::getChildValue(bondNode, "CreditCurveId", false); // issuer credit term structure not mandatory
    securityId_ = XMLUtils::getChildValue(bondNode, "SecurityId", true);
    referenceCurveId_ = XMLUtils::getChildValue(bondNode, "ReferenceCurveId", true);
    settlementDays_ = XMLUtils::getChildValue(bondNode, "SettlementDays", true);
    calendar_ = XMLUtils::getChildValue(bondNode, "Calendar", true);
    issueDate_ = XMLUtils::getChildValue(bondNode, "IssueDate", true);
    XMLNode* legNode = XMLUtils::getChildNode(bondNode, "LegData");
    while (legNode != nullptr) {
        coupons_.push_back(LegData());
        coupons_.back().fromXML(legNode);
        legNode = XMLUtils::getNextSibling(legNode, "LegData");
    }

    XMLNode* bondTSRDataNode = XMLUtils::getChildNode(bondTRSNode, "TotalReturnData");
    QL_REQUIRE(bondTSRDataNode, "No bondTSRDataNode Node");

    longInBond_ = XMLUtils::getChildValue(bondTSRDataNode, "Payer", true);
    scheduleData_.fromXML(XMLUtils::getChildNode(bondTSRDataNode, "ScheduleData"));

    XMLNode* bondTSRFundingNode = XMLUtils::getChildNode(bondTRSNode, "FundingData");
    XMLNode* fLegNode = XMLUtils::getChildNode(bondTSRFundingNode, "LegData");
    fundingLegData_ = LegData();
    fundingLegData_.fromXML(fLegNode);
}

XMLNode* BondTRS::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* bondTRSNode = doc.allocNode("BondTRSData");
    XMLUtils::appendNode(node, bondTRSNode);
    XMLNode* bondNode = doc.allocNode("BondData");
    XMLUtils::appendNode(bondTRSNode, bondNode);
    XMLUtils::addChild(doc, bondNode, "CreditCurveId", creditCurveId_);
    XMLUtils::addChild(doc, bondNode, "ReferenceCurveId", referenceCurveId_);
    XMLUtils::addChild(doc, bondNode, "SecurityId", securityId_);
    XMLUtils::addChild(doc, bondNode, "SettlementDays", settlementDays_);
    XMLUtils::addChild(doc, bondNode, "Calendar", calendar_);
    XMLUtils::addChild(doc, bondNode, "IssueDate", issueDate_);

    for (auto& c : coupons_)
        XMLUtils::appendNode(bondNode, c.toXML(doc));

    XMLNode* trsDataNode = doc.allocNode("TotalReturnData");
    XMLUtils::appendNode(bondTRSNode, trsDataNode);
    XMLUtils::addChild(doc, trsDataNode, "Payer", longInBond_);
    XMLUtils::appendNode(trsDataNode, scheduleData_.toXML(doc));

    XMLNode* fundingDataNode = doc.allocNode("FundingData");
    XMLUtils::appendNode(bondTRSNode, fundingDataNode);
    XMLUtils::appendNode(fundingDataNode, fundingLegData_.toXML(doc));
    return node;
}

} // namespace data
} // namespace ore
