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

#include <ored/portfolio/barrieroption.hpp>
#include <ored/portfolio/builders/fxbarrieroption.hpp>
#include <ored/portfolio/fxkikobarrieroption.hpp>
#include <ored/portfolio/genericbarrieroption.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
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

QuantLib::Date calculateOptionPaymentDate(const std::string tradeId, const std::string tradeType, 
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


GenericBarrierOptionData parseFxBarrierOption(const ore::data::FxOptionWithBarrier* fxBarrierOption) {
    QL_REQUIRE(fxBarrierOption != nullptr, "FxBarrierOptionScriptedEngineBuilder: internal error, could not "
                                           "cast to ore::data::FxOptionWithBarrier. Contact dev.");
    GenericBarrierOptionData data;
    std::string indexName = fxBarrierOption->fxIndex().empty()
                                ? "GENERIC-" + fxBarrierOption->boughtCurrency() + "-" + fxBarrierOption->soldCurrency()
                                : fxBarrierOption->fxIndex().substr(3);
    data.underlying = QuantLib::ext::make_shared<FXUnderlying>("FX", indexName, 1.0);
    data.optionData = fxBarrierOption->option();
    // Barrier
    const auto& barrier = fxBarrierOption->barrier();
    if (barrier.levels().size() == 1){
        data.barriers.push_back(barrier);
    } else if (barrier.levels().size() == 2){
        auto doubleBarrierType = parseDoubleBarrierType(barrier.type());
        string lowBarrierType = doubleBarrierType == DoubleBarrier::KIKO || doubleBarrierType == DoubleBarrier::KnockIn
                                    ? "DownAndIn"
                                    : "DownAndOut";
        string highBarrierType = doubleBarrierType == DoubleBarrier::KIKO || doubleBarrierType == DoubleBarrier::KnockOut
                                    ? "UpAndOut"
                                    : "UpAndIn";

        BarrierData lowBarrier(lowBarrierType, {barrier.levels().front().value()}, barrier.rebate(), {barrier.levels().front()},
                               barrier.style(), barrier.strictComparison(), barrier.overrideTriggered());
        BarrierData highBarrier(highBarrierType, {barrier.levels().back().value()}, barrier.rebate(),
                                {barrier.levels().back()}, barrier.style(), barrier.strictComparison(),
                                barrier.overrideTriggered());
        data.barriers.push_back(lowBarrier);
        data.barriers.push_back(highBarrier);
    } else {
        QL_FAIL("FxBarrierOptionScriptedEngineBuilder: only single and double barriers are supported. Please check trade xml.");
    }
    Date today = Settings::instance().evaluationDate();
    auto start = fxBarrierOption->startDate();
    std::string startDate = start == Date() ? to_string(today) : to_string(start);
    std::string exerciseDate = data.optionData.exerciseDates().front();
    ScheduleRules rule(startDate, exerciseDate, "1D", fxBarrierOption->calendarStr(), "Following", "Unadjusted",
                       "Backward");
    data.barrierMonitoringDates = ScheduleData(rule);
    //! Empty transatlantic
    data.transatlanticBarrier = BarrierData();
    data.payCurrency = fxBarrierOption->soldCurrency();
    Date expiryDate = parseDate(exerciseDate);
    Date paymentDate = calculateOptionPaymentDate(fxBarrierOption->id(), fxBarrierOption->tradeType(), expiryDate,
                                                  data.optionData.paymentData());
    data.settlementDate = to_string(paymentDate);
    data.quantity = to_string(fxBarrierOption->boughtAmount());
    data.strike = to_string(fxBarrierOption->strike());
    data.amount = "";
    data.kikoType = "KoAlways";
    return data;
}

GenericBarrierOptionData parseFxKIKOBarrierOptionData(const ore::data::FxKIKOBarrierOption* fxKiKoBarrierOption) {
    QL_REQUIRE(fxKiKoBarrierOption != nullptr, "FxKIKOBarrierOptionScriptedEngineBuilder: internal error, could not "
                                               "cast to ore::data::FxKIKOBarrierOption. Contact dev.");
    GenericBarrierOptionData data;
    std::string indexName =
        fxKiKoBarrierOption->fxIndex().empty()
            ? "GENERIC-" + fxKiKoBarrierOption->boughtCurrency() + "-" + fxKiKoBarrierOption->soldCurrency()
            : fxKiKoBarrierOption->fxIndex().substr(3);
    data.underlying = QuantLib::ext::make_shared<FXUnderlying>("FX", indexName, 1.0);
    data.optionData = fxKiKoBarrierOption->option();

    std::string startDate = fxKiKoBarrierOption->startDate();
    startDate = startDate.empty() ? to_string(Settings::instance().evaluationDate()) : startDate;
    std::string exerciseDate = data.optionData.exerciseDates().front();

    ScheduleRules rule(startDate, exerciseDate, "1D", fxKiKoBarrierOption->calendar(), "Following", "Unadjusted",
                       "Backward");
    data.barrierMonitoringDates = ScheduleData(rule);

    //! Empty transatlantic
    data.barriers = fxKiKoBarrierOption->barriers();
    data.transatlanticBarrier = BarrierData();
    data.payCurrency = fxKiKoBarrierOption->soldCurrency();
    Date expiryDate = parseDate(exerciseDate);
    Date paymentDate = calculateOptionPaymentDate(fxKiKoBarrierOption->id(), fxKiKoBarrierOption->tradeType(), expiryDate,
                                                  data.optionData.paymentData());
    data.settlementDate = to_string(paymentDate);
    data.quantity = to_string(fxKiKoBarrierOption->boughtAmount());
    data.strike = to_string(fxKiKoBarrierOption->strike());
    data.amount = "";
    data.kikoType = "KoAlways";
    return data;
}

QuantLib::ext::shared_ptr<ore::data::Trade>
FxBarrierOptionScriptedEngineBuilder::build(const Trade* trade,
                                            const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {
    GenericBarrierOptionData data;
    if (auto fxKiKoBarrierOption = dynamic_cast<const ore::data::FxKIKOBarrierOption*>(trade);
        fxKiKoBarrierOption != nullptr) {
        data = parseFxKIKOBarrierOptionData(fxKiKoBarrierOption);
    } else if (auto fxBarrierOption = dynamic_cast<const ore::data::FxOptionWithBarrier*>(trade);
               fxBarrierOption != nullptr) {
        data = parseFxBarrierOption(fxBarrierOption);
    } else {
        QL_FAIL("FxKIKOBarrierOptionScriptedEngineBuilder::build(): trade is not a FxKIKOBarrierOption");
    }
    auto barrierOption = QuantLib::ext::make_shared<GenericBarrierOption>(
        data.underlying, data.optionData, data.barriers, data.barrierMonitoringDates, data.transatlanticBarrier,
        data.payCurrency, data.settlementDate, data.quantity, data.strike, data.amount, data.kikoType);

    barrierOption->build(engineFactory);
    return barrierOption;
}
} // namespace data
} // namespace ore
