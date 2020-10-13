/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/creditdefaultswapoption.hpp>
#include <ored/portfolio/creditdefaultswapoption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void CreditDefaultSwapOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("CreditDefaultSwapOption::build() called for trade " << id());

    const boost::shared_ptr<Market> market = engineFactory->market();
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("CreditDefaultSwap");

    QL_REQUIRE(swap_.leg().legType() == "Fixed", "CreditDefaultSwap requires Fixed leg");
    Schedule schedule = makeSchedule(swap_.leg().schedule());

    BusinessDayConvention payConvention = Following;
    if (!swap_.leg().paymentConvention().empty()) {
        payConvention = parseBusinessDayConvention(swap_.leg().paymentConvention());
    }

    Protection::Side prot = swap_.leg().isPayer() ? Protection::Side::Buyer : Protection::Side::Seller;
    QL_REQUIRE(swap_.leg().notionals().size() == 1, "CreditDefaultSwap requires single notional");
    notional_ = swap_.leg().notionals().front();

    DayCounter dc = Actual360(true);
    if (!swap_.leg().dayCounter().empty()) {
        dc = parseDayCounter(swap_.leg().dayCounter());
    }

    // In general for CDS and CDS index trades, the standard day counter is Actual/360 and the final
    // period coupon accrual includes the maturity date.
    Actual360 standardDayCounter;
    DayCounter lastPeriodDayCounter = dc == standardDayCounter ? Actual360(true) : dc;

    boost::shared_ptr<FixedLegData> fixedData =
        boost::dynamic_pointer_cast<FixedLegData>(swap_.leg().concreteLegData());
    QL_REQUIRE(fixedData, "Wrong LegType, expected Fixed, got " << swap_.leg().legType());

    QL_REQUIRE(fixedData->rates().size() == 1, "CreditDefaultSwap requires single rate");

    boost::shared_ptr<QuantExt::CreditDefaultSwap> cds;

    if (swap_.upfrontDate() == Null<Date>())
        cds = boost::make_shared<QuantExt::CreditDefaultSwap>(
            prot, swap_.leg().notionals().front(), fixedData->rates().front(), schedule, payConvention, dc,
            swap_.settlesAccrual(), swap_.protectionPaymentTime(), swap_.protectionStart(), boost::shared_ptr<Claim>(),
            lastPeriodDayCounter);
    else {
        QL_REQUIRE(swap_.upfrontFee() != Null<Real>(), "CreditDefaultSwap: upfront date given, but no upfront fee");
        cds = boost::make_shared<QuantExt::CreditDefaultSwap>(
            prot, notional_, swap_.upfrontFee(), fixedData->rates().front(), schedule, payConvention, dc,
            swap_.settlesAccrual(), swap_.protectionPaymentTime(), swap_.protectionStart(), swap_.upfrontDate(),
            boost::shared_ptr<Claim>(), lastPeriodDayCounter);
    }

    boost::shared_ptr<CreditDefaultSwapEngineBuilder> cdsBuilder =
        boost::dynamic_pointer_cast<CreditDefaultSwapEngineBuilder>(builder);

    npvCurrency_ = swap_.leg().currency();

    QL_REQUIRE(cdsBuilder, "No Builder found for CreditDefaultSwap: " << id());
    cds->setPricingEngine(cdsBuilder->engine(parseCurrency(npvCurrency_), swap_.creditCurveId(), swap_.recoveryRate()));

    // Payoff
    Option::Type type = parseOptionType(option_.callPut());
    boost::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type, Null<Real>()));

    QuantLib::Exercise::Type exerciseType = parseExerciseType(option_.style());
    QL_REQUIRE(exerciseType == QuantLib::Exercise::Type::European, "Only European Style exercise is supported");
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of excercise dates");
    maturity_ = parseDate(option_.exerciseDates().front());

    // Exercise
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(maturity_);

    boost::shared_ptr<QuantExt::CdsOption> cdsOption;
    cdsOption = boost::make_shared<QuantExt::CdsOption>(cds, exercise);

    builder = engineFactory->builder("CreditDefaultSwapOption");
    boost::shared_ptr<CreditDefaultSwapOptionEngineBuilder> cdsOptionBuilder =
        boost::dynamic_pointer_cast<CreditDefaultSwapOptionEngineBuilder>(builder);
    QL_REQUIRE(cdsOptionBuilder, "No Builder found for CreditDefaultSwapOption: " << id());
    cdsOption->setPricingEngine(cdsOptionBuilder->engine(parseCurrency(npvCurrency_), swap_.creditCurveId(), term_));

    instrument_.reset(new VanillaInstrument(cdsOption));

    legs_ = {cds->coupons()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {swap_.leg().isPayer()};
    notionalCurrency_ = swap_.leg().currency();
}

void CreditDefaultSwapOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* node2 = XMLUtils::getChildNode(node, "CreditDefaultSwapOptionData");
    QL_REQUIRE(node2, "No CreditDefaultSwapOptionData Node");
    term_ = XMLUtils::getChildValue(node2, "Term", false);
    XMLNode* node3 = XMLUtils::getChildNode(node2, "CreditDefaultSwapData");
    QL_REQUIRE(node3, "No CreditDefaultSwapData Node");
    swap_.fromXML(node3);
    XMLNode* node4 = XMLUtils::getChildNode(node2, "OptionData");
    QL_REQUIRE(node4, "No OptionData Node");
    option_.fromXML(node4);
}

XMLNode* CreditDefaultSwapOption::toXML(XMLDocument& doc) {
    // Trade node
    XMLNode* node = Trade::toXML(doc);

    // CreditDefaultSwapOptionData node
    XMLNode* cdsOptionDataNode = doc.allocNode("CreditDefaultSwapOptionData");

    if (!term_.empty())
        XMLUtils::addChild(doc, cdsOptionDataNode, "Term", term_);
    XMLUtils::appendNode(cdsOptionDataNode, swap_.toXML(doc));
    XMLUtils::appendNode(cdsOptionDataNode, option_.toXML(doc));

    // Add CreditDefaultSwapOptionData node to Trade node
    XMLUtils::appendNode(node, cdsOptionDataNode);

    return node;
}
} // namespace data
} // namespace ore
