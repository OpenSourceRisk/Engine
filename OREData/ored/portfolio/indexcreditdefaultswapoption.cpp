/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ored/portfolio/indexcreditdefaultswapoption.hpp>

#include <ored/portfolio/builders/indexcreditdefaultswap.hpp>
#include <ored/portfolio/builders/indexcreditdefaultswapoption.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/instruments/indexcdsoption.hpp>

#include <boost/lexical_cast.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

void IndexCreditDefaultSwapOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    DLOG("IndexCreditDefaultSwapOption::build() called for trade " << id());

    // build underlying

    const boost::shared_ptr<Market> market = engineFactory->market();
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder("IndexCreditDefaultSwap");
    boost::shared_ptr<IndexCreditDefaultSwapEngineBuilder> cdsBuilder =
        boost::dynamic_pointer_cast<IndexCreditDefaultSwapEngineBuilder>(builder);
    QL_REQUIRE(cdsBuilder != nullptr,
               "IndexCreditDefaultSwapOption expected IndexCreditDefaultSwapEngineBuilder for underlying");

    QL_REQUIRE(swap_.leg().legType() == "Fixed", "IndexCreditDefaultSwapOption requires Fixed leg");
    Schedule schedule = makeSchedule(swap_.leg().schedule());
    Calendar calendar = parseCalendar(swap_.leg().schedule().rules().front().calendar());
    BusinessDayConvention payConvention = parseBusinessDayConvention(swap_.leg().paymentConvention());
    Protection::Side prot = swap_.leg().isPayer() ? Protection::Side::Buyer : Protection::Side::Seller;
    QL_REQUIRE(swap_.leg().notionals().size() == 1, "IndexCreditDefaultSwapOption requires single notional");
    notional_ = swap_.leg().notionals().front();
    DayCounter dc = parseDayCounter(swap_.leg().dayCounter());
    QL_REQUIRE(swap_.leg().fixedLegData().rates().size() == 1, "IndexCreditDefaultSwapOption requires single rate");

    boost::shared_ptr<QuantExt::IndexCreditDefaultSwap> cds;

    if (swap_.upfrontDate() == Null<Date>())
        cds = boost::make_shared<QuantExt::IndexCreditDefaultSwap>(
            prot, swap_.leg().notionals().front(), swap_.basket().notionals(),
            swap_.leg().fixedLegData().rates().front(), schedule, payConvention, dc, swap_.settlesAccrual(),
            swap_.paysAtDefaultTime(), swap_.protectionStart());
    else {
        QL_REQUIRE(swap_.upfrontFee() != Null<Real>(),
                   "IndexCreditDefaultSwap: upfront date given, but no upfront fee");
        cds = boost::make_shared<QuantExt::IndexCreditDefaultSwap>(
            prot, notional_, swap_.basket().notionals(), swap_.upfrontFee(), swap_.leg().fixedLegData().rates().front(),
            schedule, payConvention, dc, swap_.settlesAccrual(), swap_.paysAtDefaultTime(), swap_.protectionStart(),
            swap_.upfrontDate());
    }

    npvCurrency_ = swap_.leg().currency();

    QL_REQUIRE(cdsBuilder, "No Builder found for IndexCreditDefaultSwapOption: " << id());
    cds->setPricingEngine(
        cdsBuilder->engine(parseCurrency(npvCurrency_), swap_.creditCurveId(), swap_.basket().creditCurves()));

    // build option

    boost::shared_ptr<EngineBuilder> builder2 = engineFactory->builder("IndexCreditDefaultSwapOption");
    boost::shared_ptr<IndexCreditDefaultSwapOptionEngineBuilder> optionBuilder =
        boost::dynamic_pointer_cast<IndexCreditDefaultSwapOptionEngineBuilder>(builder2);
    QL_REQUIRE(optionBuilder != nullptr,
               "IndexCreditDefaultSwapOption: expected IndexCreditDefaultSwapOptionEngineBuilder for option");

    QL_REQUIRE(option_.style() == "European",
               "IndexCreditDefaultSwapOption: option style must be European (got " << option_.style() << ")");
    QL_REQUIRE(!option_.payoffAtExpiry(), "IndexCreditDefaultSwapOption: payoff must be at exercise");
    QL_REQUIRE(option_.exerciseDates().size() == 1,
               "IndexCreditDefaultSwapOption: got " << option_.exerciseDates().size()
                                                    << " exercise dates, expected one");
    QL_REQUIRE(option_.exerciseFees().size() == 0, "IndexCreditDefaultSwapOption: can not handle exercise fees");

    Date exDate = parseDate(option_.exerciseDates().front());

    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(exDate);

    boost::shared_ptr<QuantExt::IndexCdsOption> option =
        boost::make_shared<QuantExt::IndexCdsOption>(cds, exercise, knockOut_);

    option->setPricingEngine(
        optionBuilder->engine(parseCurrency(npvCurrency_), swap_.creditCurveId(), swap_.basket().creditCurves()));

    Settlement::Type settleType = parseSettlementType(option_.settlement());
    Position::Type positionType = parsePositionType(option_.longShort());

    if (settleType == Settlement::Cash) {
        instrument_.reset(new VanillaInstrument(option));
        maturity_ = exDate;
    } else {
        instrument_ = boost::make_shared<EuropeanOptionWrapper>(option, positionType == Position::Long, exDate,
                                                                settleType == Settlement::Physical, cds);
        maturity_ = cds->coupons().back()->date();
    }

    legs_ = {cds->coupons()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {swap_.leg().isPayer()};
}

void IndexCreditDefaultSwapOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* node2 = XMLUtils::getChildNode(node, "IndexCreditDefaultSwapOptionData");
    QL_REQUIRE(node2, "No IndexCreditDefaultSwapOptionData Node");
    knockOut_ = XMLUtils::getChildValueAsBool(node2, "KnockOut");
    XMLNode* node3 = XMLUtils::getChildNode(node, "IndexCreditDefaultSwapData");
    QL_REQUIRE(node3, "No IndexCreditDefaultSwapData Node");
    swap_.fromXML(node3);
    XMLNode* node4 = XMLUtils::getChildNode(node, "OptionData");
    QL_REQUIRE(node4, "No IndexCreditDefaultSwapData Node");
    option_.fromXML(node4);
}

XMLNode* IndexCreditDefaultSwapOption::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* node2 = doc.allocNode("IndexCreditDefaultSwapOptionData");
    XMLUtils::addChild(doc, node2, "KnockOut", knockOut_);
    XMLUtils::appendNode(node2, swap_.toXML(doc));
    XMLUtils::appendNode(node2, option_.toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
