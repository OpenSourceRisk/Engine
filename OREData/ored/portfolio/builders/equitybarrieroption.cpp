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

#include <ored/portfolio/barrieroption.hpp>
#include <ored/portfolio/builders/equitybarrieroption.hpp>
#include <ored/portfolio/genericbarrieroption.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

struct GenericBarrierOptionData {
    QuantLib::ext::shared_ptr<Underlying> underlying;
    OptionData optionData;
    std::vector<BarrierData> barriers;
    ScheduleData barrierMonitoringDates;
    BarrierData transatlanticBarrier;
    std::string payCurrency;
    std::string settlementDate;
    std::string quantity;
    std::string strike;
    std::string amount;
    std::string kikoType;
};

QuantLib::Date calculateOptionPaymentDate(const std::string& tradeId, const std::string& tradeType,
                                          const QuantLib::Date& expiryDate,
                                          const QuantLib::ext::optional<OptionPaymentData>& opd) {
    Date paymentDate = expiryDate;
    if (opd) {
        if (opd->rulesBased()) {
            const Calendar& cal = opd->calendar();
            QL_REQUIRE(cal != Calendar(), "Need a non-empty calendar for rules based payment date.");
            paymentDate = cal.advance(expiryDate, opd->lag(), Days, opd->convention());
        } else {
            if (opd->dates().size() > 1)
                ore::data::StructuredTradeWarningMessage(tradeId, tradeType, "Trade build",
                                                         "Found more than 1 payment date. The first one will be used.")
                    .log();
            paymentDate = opd->dates().front();
        }
        QL_REQUIRE(paymentDate >= expiryDate, "Payment date must be greater than or equal to expiry date.");
    }
    return paymentDate;
}

GenericBarrierOptionData parseEquityBarrierOption(const ore::data::EquityOptionWithBarrier* equityBarrierOption) {
    QL_REQUIRE(equityBarrierOption != nullptr, "EquityBarrierOptionScriptedEngineBuilder: internal error, could not "
                                               "cast to ore::data::EquityOptionWithBarrier. Contact dev.");
    GenericBarrierOptionData data;
    std::string indexName = equityBarrierOption->getIndex()->name();
    data.underlying = QuantLib::ext::make_shared<EquityUnderlying>(indexName);
    data.optionData = equityBarrierOption->option();
    // Barrier
    const auto& barrier = equityBarrierOption->barrier();
    if (barrier.levels().size() == 1) {
        data.barriers.push_back(barrier);
    } else if (barrier.levels().size() == 2) {
        auto doubleBarrierType = parseDoubleBarrierType(barrier.type());
        string lowBarrierType = doubleBarrierType == DoubleBarrier::KIKO || doubleBarrierType == DoubleBarrier::KnockIn
                                    ? "DownAndIn"
                                    : "DownAndOut";
        string highBarrierType =
            doubleBarrierType == DoubleBarrier::KIKO || doubleBarrierType == DoubleBarrier::KnockOut ? "UpAndOut"
                                                                                                     : "UpAndIn";

        BarrierData lowBarrier(lowBarrierType, {barrier.levels().front().value()}, barrier.rebate(),
                               {barrier.levels().front()}, barrier.style(), barrier.strictComparison(),
                               barrier.overrideTriggered());
        BarrierData highBarrier(highBarrierType, {barrier.levels().back().value()}, barrier.rebate(),
                                {barrier.levels().back()}, barrier.style(), barrier.strictComparison(),
                                barrier.overrideTriggered());
        data.barriers.push_back(lowBarrier);
        data.barriers.push_back(highBarrier);
    } else {
        QL_FAIL("EquityBarrierOptionScriptedEngineBuilder: only single and double barriers are supported. Please check "
                "trade xml.");
    }
    Date today = Settings::instance().evaluationDate();
    auto start = equityBarrierOption->startDate();
    std::string startDate = start == Date() ? to_string(today) : to_string(start);
    std::string exerciseDate = data.optionData.exerciseDates().front();
    ScheduleRules rule(startDate, exerciseDate, "1D", equityBarrierOption->calendarStr(), "Following", "Unadjusted",
                       "Backward");
    data.barrierMonitoringDates = ScheduleData(rule);
    //! Empty transatlantic
    data.transatlanticBarrier = BarrierData();
    data.payCurrency = equityBarrierOption->npvCurrency();
    Date expiryDate = parseDate(exerciseDate);
    Date paymentDate = calculateOptionPaymentDate(equityBarrierOption->id(), equityBarrierOption->tradeType(),
                                                  expiryDate, data.optionData.paymentData());
    data.settlementDate = to_string(paymentDate);
    data.quantity = to_string(equityBarrierOption->quantity());
    data.strike = to_string(equityBarrierOption->strike());
    data.amount = "";
    data.kikoType = "KoAlways";
    return data;
}

QuantLib::ext::shared_ptr<ore::data::Trade>
EquityBarrierOptionScriptedEngineBuilder::build(const Trade* trade,
                                                const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    GenericBarrierOptionData data;
    if (auto equityBarrierOption = dynamic_cast<const ore::data::EquityOptionWithBarrier*>(trade);
        equityBarrierOption != nullptr) {
        data = parseEquityBarrierOption(equityBarrierOption);
    } else {
        QL_FAIL("EquityBarrierOptionScriptedEngineBuilder::build(): trade is not an EquityOptionWithBarrier");
    }
    auto barrierOption = QuantLib::ext::make_shared<GenericBarrierOption>(
        data.underlying, data.optionData, data.barriers, data.barrierMonitoringDates, data.transatlanticBarrier,
        data.payCurrency, data.settlementDate, data.quantity, data.strike, data.amount, data.kikoType);

    barrierOption->build(engineFactory);
    return barrierOption;
}
} // namespace data
} // namespace ore
