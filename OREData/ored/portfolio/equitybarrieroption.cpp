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

#include <ored/portfolio/builders/equitybarrieroption.hpp>
#include <ored/portfolio/equitybarrieroption.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/tradestrike.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void EquityBarrierOption::checkBarriers() {
    QL_REQUIRE(barrier().levels().size() == 1, "Invalid number of barrier levels");
    QL_REQUIRE(barrier().style().empty() || barrier().style() == "American", "Only american barrier style suppported");
}

QuantLib::ext::shared_ptr<QuantLib::PricingEngine> EquityBarrierOption::vanillaPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef,
    const QuantLib::Date& expiryDate, const QuantLib::Date& paymentDate) {    

    QuantLib::ext::shared_ptr<EngineBuilder> builder = ef->builder("EquityOption");
    QL_REQUIRE(builder, "No builder found for EquityOption");

    QuantLib::ext::shared_ptr<EquityEuropeanOptionEngineBuilder> eqOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<EquityEuropeanOptionEngineBuilder>(builder);
    QL_REQUIRE(eqOptBuilder, "No eqOptBuilder found");

    setSensitivityTemplate(*eqOptBuilder);

    return eqOptBuilder->engine(equityName(), tradeCurrency(), expiryDate);
}

QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
EquityBarrierOption::barrierPricingEngine(const QuantLib::ext::shared_ptr<EngineFactory>& ef,
                                            const QuantLib::Date& expiryDate, const QuantLib::Date& paymentDate) {

    QuantLib::ext::shared_ptr<EngineBuilder> builder = ef->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);

    QuantLib::ext::shared_ptr<EquityBarrierOptionEngineBuilder> eqBarrierOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<EquityBarrierOptionEngineBuilder>(builder);
    QL_REQUIRE(eqBarrierOptBuilder, "No eqBarrierOptBuilder found");

    setSensitivityTemplate(*eqBarrierOptBuilder);

    return eqBarrierOptBuilder->engine(equityName(), tradeCurrency(), expiryDate);
}

} // namespace data
} // namespace oreplus
