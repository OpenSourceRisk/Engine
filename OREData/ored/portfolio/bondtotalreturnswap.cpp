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

#include <ored/portfolio/bondtotalreturnswap.hpp>
#include <ored/portfolio/bondutils.hpp>
#include <ored/portfolio/builders/bond.hpp>
#include <ored/portfolio/builders/bondtotalreturnswap.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/bondindexbuilder.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <qle/cashflows/bondtrscashflow.hpp>
#include <qle/indexes/bondindex.hpp>
#include <qle/instruments/bondtotalreturnswap.hpp>
#include <qle/utilities/inflation.hpp>

#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/instruments/bonds/zerocouponbond.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void BondTRS::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("BondTRS::build() called for trade " << id());

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("Total Return Swap");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();
    QuantLib::ext::shared_ptr<EngineBuilder> builder_trs = engineFactory->builder("BondTRS");
    bondData_ = originalBondData_;
    bondData_.populateFromBondReferenceData(engineFactory->referenceData());

    Schedule schedule = makeSchedule(scheduleData_);
    Calendar calendar = parseCalendar(bondData_.calendar());

    auto configuration = builder_trs->configuration(MarketContext::pricing);
    auto legBuilder = engineFactory->legBuilder(fundingLegData_.legType());

    // check currency restrictions

    QL_REQUIRE(fundingLegData_.currency() != bondData_.currency() || fxIndex_.empty(),
               "if funding leg ccy (" << fundingLegData_.currency() << ") = bond ccy (" << bondData_.currency()
                                      << "), no fx index must be given");
    QL_REQUIRE(fundingLegData_.currency() == bondData_.currency() || !fxIndex_.empty(),
               "if funding leg ccy (" << fundingLegData_.currency() << ") != bond ccy (" << bondData_.currency()
                                      << "), a fx index must be given");

    npvCurrency_ = fundingLegData_.currency();
    notionalCurrency_ = bondData_.currency();

    // build return leg valuation and payment schedule
    DLOG("build valuation and payment dates vectors");

    Period observationLag = observationLag_.empty() ? 0 * Days : parsePeriod(observationLag_);
    Calendar observationCalendar = parseCalendar(observationCalendar_);
    BusinessDayConvention observationConvention =
        observationConvention_.empty() ? Unadjusted : parseBusinessDayConvention(observationConvention_);

    PaymentLag paymentLag = parsePaymentLag(paymentLag_);
    Period plPeriod = boost::apply_visitor(PaymentLagPeriod(), paymentLag);
    Calendar paymentCalendar = parseCalendar(paymentCalendar_);
    BusinessDayConvention paymentConvention =
        paymentConvention_.empty() ? Unadjusted : parseBusinessDayConvention(paymentConvention_);

    std::vector<Date> valuationDates, paymentDates;

    for (auto const& d : schedule.dates()) {
        valuationDates.push_back(observationCalendar.advance(d, -observationLag, observationConvention));
        if (d != schedule.dates().front())
            paymentDates.push_back(paymentCalendar.advance(d, plPeriod, paymentConvention));
    }

    if (!paymentDates_.empty()) {
        paymentDates.clear();
        QL_REQUIRE(paymentDates_.size() + 1 == valuationDates.size(),
                   "paymentDates size (" << paymentDates_.size() << ") does no match valuatioDates size ("
                                         << valuationDates.size() << ") minus 1");
        for (auto const& s : paymentDates_)
            paymentDates.push_back(parseDate(s));
    }

    DLOG("valuation schedule:");
    for (auto const& d : valuationDates)
        DLOG(ore::data::to_string(d));

    DLOG("payment schedule:");
    for (auto const& d : paymentDates)
        DLOG(ore::data::to_string(d));

    // build fx index for composite bond trs

    QuantLib::ext::shared_ptr<FxIndex> fxIndex;
    if (!fxIndex_.empty()) {
        fxIndex =
            buildFxIndex(fxIndex_, fundingLegData_.currency(), bondData_.currency(), engineFactory->market(),
                         engineFactory->configuration(MarketContext::pricing));

    }

    // build bond index (absolute prices, conditional on survival set to false)
    BondIndexBuilder bondIndexBuilder(bondData_, useDirtyPrices_, false, NullCalendar(), false, engineFactory);
    auto bondIndex = bondIndexBuilder.bondIndex();

    // compute initial price taking into account the possible scaling with priceQuoteBaseValue and 100.0
    Real effectiveInitialPrice = bondIndexBuilder.priceAdjustment(initialPrice_);
    if (effectiveInitialPrice != Null<Real>())
        effectiveInitialPrice = effectiveInitialPrice / 100.0;

    // add indexing data from the bond trs leg, if this is desired

    if (fundingLegData_.indexingFromAssetLeg()) {
        DLOG("adding indexing information from trs leg to funding leg");

        std::vector<string> stringValuationDates;
        for (auto const& d : valuationDates)
            stringValuationDates.push_back(ore::data::to_string(d));
        ScheduleData valuationSchedule(ScheduleDates("", "", "", stringValuationDates, ""));

        // add bond indexing
        Indexing bondIndexing("BOND-" + bondIndex->securityName(), "", bondIndex->dirty(), bondIndex->relative(),
                              bondIndex->conditionalOnSurvival(), bondData_.bondNotional(), effectiveInitialPrice,
                              Null<Real>(), valuationSchedule, 0, "", "U", false);
        fundingLegData_.indexing().push_back(bondIndexing);

        // add fx indexing, if applicable
        if (!fxIndex_.empty()) {
            Indexing fxIndexing(fxIndex_, "", false, false, false, 1.0, Null<Real>(), Null<Real>(), valuationSchedule,
                                0, "", "U", false);
            fundingLegData_.indexing().push_back(fxIndexing);
        }

        // set notional node to 1.0
        fundingLegData_.notionals() = std::vector<Real>(1, 1.0);
        fundingLegData_.notionalDates() = std::vector<std::string>();

        // reset flag that told us to pull the indexing information from the equity leg
        fundingLegData_.indexingFromAssetLeg() = false;
    }

    // build funding leg (consisting of a coupon leg and (possibly) a notional leg

    Leg fundingLeg = legBuilder->buildLeg(fundingLegData_, engineFactory, requiredFixings_, configuration);
    Leg fundingNotionalLeg;
    if (fundingLegData_.notionalInitialExchange() || fundingLegData_.notionalFinalExchange() ||
        fundingLegData_.notionalAmortizingExchange()) {
        Natural fundingLegPayLag = 0;
        fundingNotionalLeg =
            makeNotionalLeg(fundingLeg, fundingLegData_.notionalInitialExchange(),
                            fundingLegData_.notionalFinalExchange(), fundingLegData_.notionalAmortizingExchange(),
                            fundingLegPayLag, parseBusinessDayConvention(fundingLegData_.paymentConvention()),
                            parseCalendar(fundingLegData_.paymentCalendar()));
    }

    QL_REQUIRE(fundingLegData_.isPayer() != payTotalReturnLeg_,
               "funding leg and total return lag are both rec or both pay");
    DLOG("Before bondTRS");
    auto bondTRS = QuantLib::ext::make_shared<QuantExt::BondTRS>(
        bondIndex, bondData_.bondNotional(), effectiveInitialPrice,
        std::vector<QuantLib::Leg>{fundingLeg, fundingNotionalLeg}, payTotalReturnLeg_, valuationDates, paymentDates,
        fxIndex, payBondCashFlowsImmediately_, parseCurrency(fundingLegData_.currency()),
        parseCurrency(bondData_.currency()));
    DLOG("After bondTRS");
    QuantLib::ext::shared_ptr<BondTRSEngineBuilder> trsBondBuilder =
        QuantLib::ext::dynamic_pointer_cast<BondTRSEngineBuilder>(builder_trs);
    QL_REQUIRE(trsBondBuilder, "No Builder found for BondTRS: " << id());
    bondTRS->setPricingEngine(trsBondBuilder->engine(fundingLegData_.currency()));
    setSensitivityTemplate(*trsBondBuilder);
    instrument_.reset(new VanillaInstrument(bondTRS));
    // maturity_ = std::max(valuationDates.back(), paymentDates.back());
    maturity_ = bondIndex->bond()->maturityDate();
    notional_ = bondIndex->bond()->notional() * bondData_.bondNotional();

    // cashflows will be generated as additional results in the pricing engine

    legs_ = {};
    legCurrencies_ = {};
    legPayers_ = {};

    // add required bond and fx fixings for return calculation
    addToRequiredFixings(bondTRS->returnLeg(), QuantLib::ext::make_shared<FixingDateGetter>(requiredFixings_));
    bondIndexBuilder.addRequiredFixings(requiredFixings_, bondTRS->returnLeg());

    // add required fx fixings for bond cashflow conversion (see the engine for details)

    if (!fxIndex_.empty()) {
        for (auto const& c : bondIndex->bond()->cashflows()) {
            requiredFixings_.addFixingDate(fxIndex->fixingCalendar().adjust(c->date(), Preceding), fxIndex_, c->date());
        }
    }

    if (bondData_.isInflationLinked() && useDirtyPrices()) {
        auto inflationIndices = extractAllInflationUnderlyingFromBond(bondIndex->bond());
        for (const auto& cf : bondTRS->returnLeg()) {
            auto tcf = QuantLib::ext::dynamic_pointer_cast<TRSCashFlow>(cf);
            if (tcf != nullptr) {
                for (const auto& [key, index] : inflationIndices) {
                    auto [name, interpolation, couponFrequency, observationLag] = key;
                    std::string oreName = IndexNameTranslator::instance().oreName(name);
                    requiredFixings_.addZeroInflationFixingDate(
                        tcf->fixingStartDate() - observationLag, oreName, false, index->frequency(),
                        index->availabilityLag(), interpolation, couponFrequency, Date::maxDate(), false, false);
                }
            }
        }
    }
}

void BondTRS::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* bondTRSNode = XMLUtils::getChildNode(node, "BondTRSData");
    QL_REQUIRE(bondTRSNode, "No BondTRSData Node");
    originalBondData_.fromXML(XMLUtils::getChildNode(bondTRSNode, "BondData"));
    bondData_ = originalBondData_;

    XMLNode* bondTRSDataNode = XMLUtils::getChildNode(bondTRSNode, "TotalReturnData");
    QL_REQUIRE(bondTRSDataNode, "No bondTRSDataNode Node");

    payTotalReturnLeg_ = parseBool(XMLUtils::getChildValue(bondTRSDataNode, "Payer", true));

    scheduleData_.fromXML(XMLUtils::getChildNode(bondTRSDataNode, "ScheduleData"));

    observationLag_ = XMLUtils::getChildValue(bondTRSDataNode, "ObservationLag");
    observationConvention_ = XMLUtils::getChildValue(bondTRSDataNode, "ObservationConvention");
    observationCalendar_ = XMLUtils::getChildValue(bondTRSDataNode, "ObservationCalendar");

    paymentLag_ = XMLUtils::getChildValue(bondTRSDataNode, "PaymentLag");
    paymentConvention_ = XMLUtils::getChildValue(bondTRSDataNode, "PaymentConvention");
    paymentCalendar_ = XMLUtils::getChildValue(bondTRSDataNode, "PaymentCalendar");
    paymentDates_ = XMLUtils::getChildrenValues(bondTRSDataNode, "PaymentDates", "PaymentDate");

    initialPrice_ = Null<Real>();
    if (auto n = XMLUtils::getChildNode(bondTRSDataNode, "InitialPrice"))
        initialPrice_ = parseReal(XMLUtils::getNodeValue(n));
    std::string priceType = XMLUtils::getChildValue(bondTRSDataNode, "PriceType", true);
    if (priceType == "Dirty")
        useDirtyPrices_ = true;
    else if (priceType == "Clean")
        useDirtyPrices_ = false;
    else {
        QL_FAIL("PriceType (" << priceType << ") must be Clean or Dirty");
    }

    XMLNode* fxt = XMLUtils::getChildNode(bondTRSDataNode, "FXTerms");
    if (fxt)
        fxIndex_ = XMLUtils::getChildValue(fxt, "FXIndex", true);

    payBondCashFlowsImmediately_ =
        XMLUtils::getChildValueAsBool(bondTRSDataNode, "PayBondCashFlowsImmediately", false, false);

    XMLNode* bondTRSFundingNode = XMLUtils::getChildNode(bondTRSNode, "FundingData");
    XMLNode* fLegNode = XMLUtils::getChildNode(bondTRSFundingNode, "LegData");
    fundingLegData_ = LegData();
    fundingLegData_.fromXML(fLegNode);
}

XMLNode* BondTRS::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* bondTRSNode = doc.allocNode("BondTRSData");
    XMLUtils::appendNode(node, bondTRSNode);
    XMLUtils::appendNode(bondTRSNode, originalBondData_.toXML(doc));

    XMLNode* trsDataNode = doc.allocNode("TotalReturnData");
    XMLUtils::appendNode(bondTRSNode, trsDataNode);
    XMLUtils::addChild(doc, trsDataNode, "Payer", payTotalReturnLeg_);

    if (initialPrice_ != Null<Real>())
        XMLUtils::addChild(doc, trsDataNode, "InitialPrice", initialPrice_);
    XMLUtils::addChild(doc, trsDataNode, "PriceType", useDirtyPrices_ ? "Dirty" : "Clean");

    if (!observationLag_.empty())
        XMLUtils::addChild(doc, trsDataNode, "ObservationLag", observationLag_);
    if (!observationConvention_.empty())
        XMLUtils::addChild(doc, trsDataNode, "ObservationConvention", observationConvention_);
    if (!observationCalendar_.empty())
        XMLUtils::addChild(doc, trsDataNode, "ObservationCalendar", observationCalendar_);

    if (!paymentLag_.empty())
        XMLUtils::addChild(doc, trsDataNode, "PaymentLag", paymentLag_);
    if (!paymentConvention_.empty())
        XMLUtils::addChild(doc, trsDataNode, "PaymentConvention", paymentConvention_);
    if (!paymentCalendar_.empty())
        XMLUtils::addChild(doc, trsDataNode, "PaymentCalendar", paymentCalendar_);
    if (!paymentDates_.empty())
        XMLUtils::addChildren(doc, trsDataNode, "PaymentDates", "PaymentDate", paymentDates_);

    if (!fxIndex_.empty()) {
        XMLNode* fxNode = doc.allocNode("FXTerms");
        XMLUtils::addChild(doc, fxNode, "FXIndex", fxIndex_);
        XMLUtils::appendNode(trsDataNode, fxNode);
    }

    XMLUtils::appendNode(trsDataNode, scheduleData_.toXML(doc));

    XMLUtils::addChild(doc, trsDataNode, "PayBondCashFlowsImmediately", payBondCashFlowsImmediately_);

    XMLNode* fundingDataNode = doc.allocNode("FundingData");
    XMLUtils::appendNode(bondTRSNode, fundingDataNode);
    XMLUtils::appendNode(fundingDataNode, fundingLegData_.toXML(doc));
    return node;
}

std::map<AssetClass, std::set<std::string>>
BondTRS::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    std::map<AssetClass, std::set<std::string>> result;
    result[AssetClass::BOND] = {bondData_.securityId()};
    return result;
}

} // namespace data
} // namespace ore
