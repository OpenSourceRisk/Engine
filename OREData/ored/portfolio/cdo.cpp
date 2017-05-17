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

#include <ored/portfolio/cdo.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/builders/cdo.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/experimental/credit/syntheticcdo.hpp>
#include <ql/experimental/credit/inhomogeneouspooldef.hpp>
//#include <ql/experimental/credit/midpointcdoengine.hpp>
#include <qle/pricingengines/midpointcdoengine.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/log.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ored/utilities/indexparser.hpp>
#include <boost/lexical_cast.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void SyntheticCDO::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("SyntheticCDO::build() called for trade " << id());

    const boost::shared_ptr<Market> market = engineFactory->market();

    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("SyntheticCDO");
    boost::shared_ptr<CdoEngineBuilder> cdoEngineBuilder = boost::dynamic_pointer_cast<CdoEngineBuilder>(builder);

    // not used
    Date protectionStartDate = parseDate(protectionStart_);
    // Date upfrontDateQL = parseDate(upfrontDate_);

    Leg leg = makeFixedLeg(legData_);

    Protection::Side side = legData_.isPayer() ? Protection::Buyer : Protection::Seller;
    Schedule schedule = makeSchedule(legData_.schedule());
    Real upfrontRate = upfrontFee_;
    // FIXME: QL CDO supports a single rate only
    Real runningRate = legData_.fixedLegData().rates().front();
    DayCounter dayCounter = parseDayCounter(legData_.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(legData_.paymentConvention());

    Currency ccy = parseCurrency(legData_.currency());

    boost::shared_ptr<Pool> pool(new Pool);
    vector<Real> recoveryRates;
    for (Size i = 0; i < basketData_.issuers().size(); ++i) {
        string issuerId = basketData_.issuers()[i];
        string creditCurveId = basketData_.creditCurves()[i];
        QL_REQUIRE(basketData_.currencies()[i] == legData_.currency(), "currency not unique across basket");
        // FIXME: dummy default key
        DefaultProbKey key = NorthAmericaCorpDefaultKey(ccy, SeniorSec, Period(), 1.0);
        Handle<DefaultProbabilityTermStructure> defaultCurve =
            market->defaultCurve(creditCurveId, builder->configuration(MarketContext::pricing));
        recoveryRates.push_back(
            market->recoveryRate(creditCurveId, builder->configuration(MarketContext::pricing))->value());
        pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>> p(key, defaultCurve);
        vector<pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>>> probabilities(1, p);
        // FIXME: empty default event set, assuming no default until schedule start date
        DefaultEventSet eventSet;
        Issuer issuer(probabilities, eventSet);
        pool->add(issuerId, issuer, key);
        DLOG("Issuer " << issuerId << " added to the pool");
    }

    // FIXME: use schedule[0] or protectionStart ?
    boost::shared_ptr<Basket> basketD(new Basket(schedule[0], basketData_.issuers(), basketData_.notionals(), pool, 0.0,
                                                 detachmentPoint_)); // assume face value claim

    basketD->setLossModel(cdoEngineBuilder->lossModel(qualifier_, recoveryRates, detachmentPoint_));

    boost::shared_ptr<QuantLib::SyntheticCDO> cdoD(
        new QuantLib::SyntheticCDO(basketD, side, schedule, upfrontRate, runningRate, dayCounter, bdc));

    cdoD->setPricingEngine(cdoEngineBuilder->engine(ccy));
    DLOG("Equity tranche to detachment point: CDO NPV " << cdoD->NPV());
    
    if (attachmentPoint_ < QL_EPSILON)
        instrument_.reset(new VanillaInstrument(cdoD));
    else { // price the tranche as spread of two equity tranches with different base correlations

        // FIXME: use schedule[0] or protectionStart ?
        // note attachment point here
        boost::shared_ptr<Basket> basketA(new Basket(schedule[0], basketData_.issuers(), basketData_.notionals(), pool,
                                                     0.0,
                                                     attachmentPoint_)); // assume face value claim

        // note attachment point here
        basketA->setLossModel(cdoEngineBuilder->lossModel(qualifier_, recoveryRates, attachmentPoint_));

        // note upfront rate set to zero here
        boost::shared_ptr<QuantLib::SyntheticCDO> cdoA(
            new QuantLib::SyntheticCDO(basketA, side, schedule, 0.0, runningRate, dayCounter, bdc));

        cdoA->setPricingEngine(cdoEngineBuilder->engine(ccy));
	DLOG("Equity tranche to attachment point: CDO NPV " << cdoA->NPV());

        boost::shared_ptr<CompositeInstrument> composite(new CompositeInstrument());
        composite->add(cdoD);
        composite->subtract(cdoA);
        instrument_.reset(new VanillaInstrument(composite));

	DLOG("Composite CDO NPV " << composite->NPV());
    }

    DLOG("CDO instrument built");

    npvCurrency_ = legData_.currency();
    maturity_ = leg.back()->date();
    notional_ = legData_.notionals().front();
    legs_ = {leg};
    legPayers_ = {legData_.isPayer()};
    legCurrencies_ = {legData_.currency()};
}

void SyntheticCDO::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* cdoNode = XMLUtils::getChildNode(node, "CdoData");
    QL_REQUIRE(cdoNode, "No CdoData Node");
    qualifier_ = XMLUtils::getChildValue(cdoNode, "Qualifier", true);
    protectionStart_ = XMLUtils::getChildValue(cdoNode, "ProtectionStart", true);
    upfrontDate_ = XMLUtils::getChildValue(cdoNode, "UpfrontDate", true);
    upfrontFee_ = XMLUtils::getChildValueAsDouble(cdoNode, "UpfrontFee", true);
    attachmentPoint_ = XMLUtils::getChildValueAsDouble(cdoNode, "AttachmentPoint", true);
    detachmentPoint_ = XMLUtils::getChildValueAsDouble(cdoNode, "DetachmentPoint", true);
    XMLNode* legNode = XMLUtils::getChildNode(cdoNode, "LegData");
    legData_.fromXML(legNode);
    XMLNode* basketNode = XMLUtils::getChildNode(cdoNode, "BasketData");
    basketData_.fromXML(basketNode);
}

XMLNode* SyntheticCDO::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* cdoNode = doc.allocNode("CdoData");
    XMLUtils::appendNode(node, cdoNode);
    XMLUtils::addChild(doc, cdoNode, "Qualifier", qualifier_);
    XMLUtils::addChild(doc, cdoNode, "ProtectionStart", protectionStart_);
    XMLUtils::addChild(doc, cdoNode, "UpfrontDate", upfrontDate_);
    XMLUtils::addChild(doc, cdoNode, "UpfrontFee", upfrontFee_);
    XMLUtils::addChild(doc, cdoNode, "AttachementPoint", attachmentPoint_);
    XMLUtils::addChild(doc, cdoNode, "DetachmentPoint", detachmentPoint_);
    XMLUtils::appendNode(cdoNode, legData_.toXML(doc));
    XMLUtils::appendNode(cdoNode, basketData_.toXML(doc));
    return node;
}
}
}
