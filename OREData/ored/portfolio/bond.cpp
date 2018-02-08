/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2017 Aareal Bank AG

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
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/instruments/bonds/zerocouponbond.hpp>

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

void Bond::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("Bond::build() called for trade " << id());

    const boost::shared_ptr<Market> market = engineFactory->market();

    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("Bond");

    Date issueDate = parseDate(issueDate_);
    Calendar calendar = parseCalendar(calendar_);
    Natural settlementDays = boost::lexical_cast<Natural>(settlementDays_);
    boost::shared_ptr<QuantLib::Bond> bond;
    std::vector<boost::shared_ptr<IborIndex>> indexes;
    std::vector<boost::shared_ptr<OptionletVolatilityStructure>> ovses;

    if (zeroBond_) { // Zero coupon bond
        bond.reset(new QuantLib::ZeroCouponBond(settlementDays, calendar, faceAmount_, parseDate(maturityDate_)));
    } else { // Coupon bond

        std::vector<Leg> legs;
        for (Size i = 0; i < coupons_.size(); ++i) {
            if (i == 0)
                currency_ = coupons_[i].currency();
            else {
                QL_REQUIRE(currency_ == coupons_[i].currency(), "leg #" << i << " currency (" << coupons_[i].currency()
                                                                        << ") not equal to leg #0 currency ("
                                                                        << coupons_[0].currency());
            }
            Leg leg;
            Handle<IborIndex> hIndex;
            Handle<OptionletVolatilityStructure> ovs;

            if (coupons_[i].legType() == "Fixed")
                leg = makeFixedLeg(coupons_[i]);
            else if (coupons_[i].legType() == "Floating") {
                boost::shared_ptr<FloatingLegData> floatData =
                    boost::dynamic_pointer_cast<FloatingLegData>(coupons_[i].concreteLegData());
                QL_REQUIRE(floatData, "Wrong LegType, expected Floating, got " << coupons_[i].legType());

                string indexName = floatData->index();
                hIndex = engineFactory->market()->iborIndex(indexName, builder->configuration(MarketContext::pricing));
                indexes.push_back(*hIndex); // for registration below
                QL_REQUIRE(!hIndex.empty(), "Could not find ibor index " << indexName << " in market.");
                boost::shared_ptr<IborIndex> index = hIndex.currentLink();
                leg = makeIborLeg(coupons_[i], index, engineFactory);
                if (!floatData->floors().empty() || !floatData->caps().empty()) {
                    ovs =
                        engineFactory->market()->capFloorVol(currency_, builder->configuration(MarketContext::pricing));
                    ovses.push_back(*ovs); // for registration below
                }

            } else {
                QL_FAIL("Unknown leg type " << coupons_[i].legType());
            }
            legs.push_back(leg);
        } // for coupons_
        Leg leg = joinLegs(legs);
        bond.reset(new QuantLib::Bond(settlementDays, calendar, issueDate, leg));
        // workaround, QL doesn't register a bond with its leg's cashflows
        for (auto const& i : indexes)
            bond->registerWith(i);
        for (auto const& o : ovses)
            bond->registerWith(o);
    }

    Currency currency = parseCurrency(currency_);
    boost::shared_ptr<BondEngineBuilder> bondBuilder = boost::dynamic_pointer_cast<BondEngineBuilder>(builder);
    QL_REQUIRE(bondBuilder, "No Builder found for Bond: " << id());
    bond->setPricingEngine(bondBuilder->engine(currency, creditCurveId_, securityId_, referenceCurveId_));
    instrument_.reset(new VanillaInstrument(bond));

    npvCurrency_ = currency_;
    maturity_ = bond->cashflows().back()->date();
    notional_ = currentNotional(bond->cashflows());

    // Add legs (only 1)
    legs_ = {bond->cashflows()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {false}; // We own the bond => we receive the flows
}

void Bond::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* bondNode = XMLUtils::getChildNode(node, "BondData");
    QL_REQUIRE(bondNode, "No BondData Node");
    issuerId_ = XMLUtils::getChildValue(bondNode, "IssuerId", true);
    creditCurveId_ = XMLUtils::getChildValue(bondNode, "CreditCurveId", false); // issuer credit term structure not mandatory
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
}

XMLNode* Bond::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* bondNode = doc.allocNode("BondData");
    XMLUtils::appendNode(node, bondNode);
    XMLUtils::addChild(doc, bondNode, "IssuerId", issuerId_);
    XMLUtils::addChild(doc, bondNode, "CreditCurveId", creditCurveId_);
    XMLUtils::addChild(doc, bondNode, "SecurityId", securityId_);
    XMLUtils::addChild(doc, bondNode, "ReferenceCurveId", referenceCurveId_);
    XMLUtils::addChild(doc, bondNode, "SettlementDays", settlementDays_);
    XMLUtils::addChild(doc, bondNode, "Calendar", calendar_);
    XMLUtils::addChild(doc, bondNode, "IssueDate", issueDate_);
    for (auto& c : coupons_)
        XMLUtils::appendNode(bondNode, c.toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
