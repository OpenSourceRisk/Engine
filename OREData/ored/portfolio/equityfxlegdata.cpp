/*
  Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/portfolio/equityfxlegdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <qle/cashflows/equitymargincoupon.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/make_shared.hpp>

using namespace ore::data;
using namespace QuantExt;
using namespace QuantLib;

using boost::iequals;
using ore::data::parseBool;
using ore::data::parseReal;
using ore::data::to_string;
using ore::data::XMLDocument;
using ore::data::XMLNode;
using ore::data::XMLUtils;
using QuantLib::Natural;
using QuantLib::Real;
using std::ostream;
using std::string;
using std::vector;
using QuantExt::Leg;

namespace ore {
namespace data {

void EquityMarginLegData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, legNodeName());
    rates_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "Rates", "Rate", "startDate", rateDates_, parseReal,
                                                             true);
    initialMarginFactor_ = XMLUtils::getChildValueAsDouble(node, "InitialMarginFactor", true);
    multiplier_ = 1;
    if (XMLUtils::getChildNode(node, "Multiplier"))
        multiplier_ = XMLUtils::getChildValueAsDouble(node, "Multiplier");
    XMLNode* equityNode = XMLUtils::getChildNode(node, "EquityLegData");
    QL_REQUIRE(equityNode, "no equityLegData provided");

    QuantLib::ext::shared_ptr<ore::data::EquityLegData> ld = QuantLib::ext::make_shared<EquityLegData>();
    ld->fromXML(equityNode);
    equityLegData_ = ld;

}

XMLNode* EquityMarginLegData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(legNodeName());
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "Rates", "Rate", rates_, "startDate", rateDates_);
    XMLUtils::addChild(doc, node, "InitialMarginFactor", initialMarginFactor_);
    XMLUtils::addChild(doc, node, "Multiplier", multiplier_);
    
    XMLUtils::appendNode(node, equityLegData_->toXML(doc));
    return node;
}

QuantExt::Leg makeEquityMarginLeg(const LegData& data, const QuantLib::ext::shared_ptr<EquityIndex2>& equityCurve,
                                  const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex,
                                  const QuantLib::Date& openEndDateReplacement) {
    QuantLib::ext::shared_ptr<EquityMarginLegData> eqMarginLegData = QuantLib::ext::dynamic_pointer_cast<EquityMarginLegData>(data.concreteLegData());
    QL_REQUIRE(eqMarginLegData, "Wrong LegType, expected EquityMargin, got " << data.legType());
    QuantLib::ext::shared_ptr<ore::data::EquityLegData> eqLegData = eqMarginLegData->equityLegData();
    QL_REQUIRE(eqLegData, "expected equityLegData");
    QuantExt::Schedule schedule = makeSchedule(data.schedule(), openEndDateReplacement);
    DayCounter dc = parseDayCounter(data.dayCounter());
    BusinessDayConvention bdc = ore::data::parseBusinessDayConvention(data.paymentConvention());
    bool isTotalReturn = eqLegData->returnType() == EquityReturnType::Total;
    Real dividendFactor = eqLegData->dividendFactor();
    Real initialPrice = eqLegData->initialPrice();
    bool initialPriceIsInTargetCcy = false;

    if (!eqLegData->initialPriceCurrency().empty()) {
        // parse currencies to handle minor currencies
        Currency initialPriceCurrency = parseCurrencyWithMinors(eqLegData->initialPriceCurrency());
        Currency dataCurrency = parseCurrencyWithMinors(data.currency());
        Currency eqCurrency;
        // set equity currency
        if (!equityCurve->currency().empty())
            eqCurrency = equityCurve->currency();
        else
            TLOG("Cannot find currency for equity " << equityCurve->name());

        // initial price currency must match leg or equity currency
        QL_REQUIRE(initialPriceCurrency == dataCurrency ||
            initialPriceCurrency == eqCurrency || eqCurrency.empty(),
                   "initial price ccy (" << initialPriceCurrency << ") must match either leg ccy ("
                                         << dataCurrency << ") or equity ccy (if given, got '"
                                         << eqCurrency << "')");
        initialPriceIsInTargetCcy = initialPriceCurrency == dataCurrency;
        // adjust for minor currency
        initialPrice = convertMinorToMajorCurrency(eqLegData->initialPriceCurrency(), initialPrice);
    }
    bool notionalReset = eqLegData->notionalReset();
    Natural fixingDays = eqLegData->fixingDays();
    PaymentLag paymentLag = parsePaymentLag(data.paymentLag());
    ScheduleData valuationData = eqLegData->valuationSchedule();
    Schedule valuationSchedule;
    if (valuationData.hasData())
        valuationSchedule = makeSchedule(valuationData, openEndDateReplacement);
    vector<double> notionals = buildScheduledVector(data.notionals(), data.notionalDates(), schedule);
    vector<double> rates = buildScheduledVector(eqMarginLegData->rates(), eqMarginLegData->rateDates(), schedule);

    applyAmortization(notionals, data, schedule, false);
    Leg leg = EquityMarginLeg(schedule, equityCurve, fxIndex)
                  .withCouponRates(rates, dc)
                  .withInitialMarginFactor(eqMarginLegData->initialMarginFactor())
                  .withNotionals(notionals)
                  .withQuantity(eqLegData->quantity())
                  .withPaymentDayCounter(dc)
                  .withPaymentAdjustment(bdc)
                  .withPaymentLag(boost::apply_visitor(PaymentLagInteger(), paymentLag))
                  .withTotalReturn(isTotalReturn)
                  .withDividendFactor(dividendFactor)
                  .withInitialPrice(initialPrice)
                  .withInitialPriceIsInTargetCcy(initialPriceIsInTargetCcy)
                  .withNotionalReset(notionalReset)
                  .withFixingDays(fixingDays)
                  .withValuationSchedule(valuationSchedule)
                  .withMultiplier(eqMarginLegData->multiplier());

    QL_REQUIRE(leg.size() > 0, "Empty Equity Margin Leg");

    return leg;
}

} // namespace data
} // namespace ore
