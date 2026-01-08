/*
  Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builder/fxeuropeanbarrieroption.hpp>
#include <ored/portfolio/fxeuropeanbarrieroption.hpp>
#include <ored/portfolio/genericbarrieroption.hpp>
#include <ored/portfolio/underlying.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<ore::data::Trade>
FxEuropeanBarrierOptionEngineBuilder::build(const Trade* trade, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    auto fxEuropeanBarrierOption = dynamic_cast<const ore::data::FxEuropeanBarrierOption*>(trade);

    QL_REQUIRE(
        fxEuropeanBarrierOption != nullptr,
        "FxEuropeanBarrierOptionEngineBuilder: internal error, could not cast to ore::data::FxEuropeanBarrierOption. Contact dev.");

    auto underlying = Quantib::ext::make_shared<FXUnderlying>("FX", fxEuropeanBarrierOption->fxIndex(), 1.0);
    
    auto optionData = fxEuropeanBarrierOption->optionData();

    std::string exerciseDate = optionData->exerciseDates().begin();
    ScheduleDates monitoringDates("NullCalendar", "", "0D", {exerciseDate} );
    ScheduleData barrierMonitoringDates(monitoringDates);

    //! Empty transatlantic barrier
    auto transatlanticBarrier = BarrierData();
    //! Observation date for schedule monitoring dates only at the end
    
    std::string domesticCurrency = fxEuropeanBarrierOption->soldCurrency();

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
    auto qty = fxEuropeanBarrierOption->boughtAmount();
    auto strike = fxEuropeanBarrierOption->strike();

    auto barrierOption = QuantLib::ext::make_shared<GenericBarrierOption>(
        underlying, optionData, {fxEuropeanBarrierOption->barrierData()}, barrierMonitoringDates, transatlanticBarrier,
        domesticCurrency, to_string(paymentDate), to_string(qty), to_string(strike), "", "");

    barrierOption->build(engineFactory);
    return barrierOption;
}

} // namespace data
} // namespace oreplus
