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

#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/instruments/bonds/zerocouponbond.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/log.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ored/utilities/indexparser.hpp>
#include <boost/lexical_cast.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void Bond::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("Bond::build() called for trade " << id());

    const boost::shared_ptr<Market> market = engineFactory->market();

    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("Bond");

    Date issueDate = parseDate(issueDate_);
    Calendar calendar = parseCalendar(calendar_);
    Natural settlementDays = boost::lexical_cast<Natural>(settlementDays_);
    boost::shared_ptr<QuantLib::Bond> bond;

    if (zeroBond_) { // Zero coupon bond
        bond.reset(new QuantLib::ZeroCouponBond(settlementDays, calendar, faceAmount_, parseDate(maturityDate_)));
    } else { // Coupon bond

        currency_ = coupons_.currency();        
        Leg leg;
        if (coupons_.legType() == "Fixed")
            leg = makeFixedLeg(coupons_);
        else if (coupons_.legType() == "Floating") {
            string indexName = coupons_.floatingLegData().index();
            Handle<IborIndex> hIndex =
                engineFactory->market()->iborIndex(indexName, builder->configuration(MarketContext::pricing));
            QL_REQUIRE(!hIndex.empty(), "Could not find ibor index " << indexName << " in market.");
            boost::shared_ptr<IborIndex> index = hIndex.currentLink();
            leg = makeIborLeg(coupons_, index, engineFactory);
        } else {
            QL_FAIL("Unknown leg type " << coupons_.legType());
        }

        bond.reset(new QuantLib::Bond(settlementDays, calendar, issueDate, leg));
    }

    Currency currency = parseCurrency(currency_);
    boost::shared_ptr<BondEngineBuilder> bondBuilder = boost::dynamic_pointer_cast<BondEngineBuilder>(builder);
    QL_REQUIRE(bondBuilder, "No Builder found for Bond" << id());
    bond->setPricingEngine(bondBuilder->engine(currency, issuerId_, securityId_, referenceCurveId_));
    DLOG("Bond::build(): Bond NPV = " << bond->NPV());
    instrument_.reset(new VanillaInstrument(bond));

    npvCurrency_ = currency_;
    maturity_ = bond->cashflows().back()->date();
}

void Bond::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* bondNode = XMLUtils::getChildNode(node, "BondData");
    QL_REQUIRE(bondNode, "No BondData Node");
    issuerId_ = XMLUtils::getChildValue(bondNode, "IssuerId", true);
    securityId_ = XMLUtils::getChildValue(bondNode, "SecurityId", true);
    referenceCurveId_ = XMLUtils::getChildValue(bondNode, "ReferenceCurveId", true);
    settlementDays_ = XMLUtils::getChildValue(bondNode, "SettlementDays", true);
    calendar_ = XMLUtils::getChildValue(bondNode, "Calendar", true);
    issueDate_ = XMLUtils::getChildValue(bondNode, "IssueDate", true);
    XMLNode* legNode = XMLUtils::getChildNode(bondNode, "LegData");
    coupons_.fromXML(legNode);
}

XMLNode* Bond::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* bondNode = doc.allocNode("BondData");
    XMLUtils::appendNode(node, bondNode);
    XMLUtils::addChild(doc, bondNode, "IssuerId", issuerId_);
    XMLUtils::addChild(doc, bondNode, "SecurityId", securityId_);
    XMLUtils::addChild(doc, bondNode, "ReferenceCurveId", referenceCurveId_);
    XMLUtils::addChild(doc, bondNode, "SettlementDays", settlementDays_);
    XMLUtils::addChild(doc, bondNode, "Calendar", calendar_);
    XMLUtils::addChild(doc, bondNode, "IssueDate", issueDate_);
    XMLUtils::appendNode(bondNode, coupons_.toXML(doc));
    return node;
}
}
}