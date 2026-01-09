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

#include <ored/portfolio/builders/fxbarrieroption.hpp>
#include <ored/portfolio/fxkikobarrieroption.hpp>
#include <ored/portfolio/genericbarrieroption.hpp>
namespace ore {
namespace data {


QuantLib::ext::shared_ptr<ore::data::Trade> FxBarrierOptionScriptedEngineBuilder::build(const Trade* trade, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory){
  auto fxKiKoBarrierOption = dynamic_cast<const ore::data::FxKIKOBarrierOption*>(trade);

    QL_REQUIRE(fxKiKoBarrierOption != nullptr,
               "FxKIKOBarrierOptionScriptedEngineBuilder: internal error, could not "
               "cast to ore::data::FxKIKOBarrierOption. Contact dev.");
    std::string indexName =
        fxKiKoBarrierOption->fxIndex().empty()
            ? "GENERIC-" + fxKiKoBarrierOption->boughtCurrency() + "-" + fxKiKoBarrierOption->soldCurrency()
            : fxKiKoBarrierOption->fxIndex().substr(3);
    QuantLib::ext::shared_ptr<Underlying> underlying = QuantLib::ext::make_shared<FXUnderlying>("FX", indexName, 1.0);

    const auto& optionData = fxKiKoBarrierOption->option();

    std::string startDate = fxKiKoBarrierOption->startDate();
    std::string exerciseDate = optionData.exerciseDates().front();

    ScheduleRules rule(startDate, exerciseDate, "1D", fxKiKoBarrierOption->calendar(), "Following", "Following", "Backward");
    ScheduleData barrierMonitoringDates(rule);

    //! Empty transatlantic barrier
    auto transatlanticBarrier = BarrierData();
    //! Observation date for schedule monitoring dates only at the end

    std::string domesticCurrency = fxKiKoBarrierOption->soldCurrency();

    Date expiryDate = parseDate(exerciseDate);
    Date paymentDate = expiryDate;
    const QuantLib::ext::optional<OptionPaymentData>& opd = optionData.paymentData();
    if (opd) {
        if (opd->rulesBased()) {
            const Calendar& cal = opd->calendar();
            QL_REQUIRE(cal != Calendar(), "Need a non-empty calendar for rules based payment date.");
            paymentDate = cal.advance(expiryDate, opd->lag(), Days, opd->convention());
        } else {
            const vector<Date>& dates = opd->dates();
            QL_REQUIRE(dates.size() == 1, "Need exactly one payment date for cash settled European option.");
            paymentDate = dates[0];
        }
        QL_REQUIRE(paymentDate >= expiryDate, "Payment date must be greater than or equal to expiry date.");
    }
    auto qty = fxKiKoBarrierOption->boughtAmount();
    auto strike = fxKiKoBarrierOption->strike();
    std::vector<BarrierData> barriers = fxKiKoBarrierOption->barriers();
    auto barrierOption = QuantLib::ext::make_shared<GenericBarrierOption>(
        underlying, optionData, barriers, barrierMonitoringDates, transatlanticBarrier, domesticCurrency,
        to_string(paymentDate), to_string(qty), to_string(strike), "", "");

    barrierOption->build(engineFactory);
    return barrierOption;
}
} // namespace data
} // namespace ore
