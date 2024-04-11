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

#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/portfolio/equityfxlegbuilder.hpp>
#include <ored/portfolio/equityfxlegdata.hpp>
#include <ored/portfolio/fixingdates.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/one.hpp>

using namespace ore::data;
using namespace QuantExt;
using namespace QuantLib;

using std::string;

namespace ore {
namespace data {

Leg EquityMarginLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                     RequiredFixings& requiredFixings, const string& configuration,
                                     const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    auto eqMarginData = QuantLib::ext::dynamic_pointer_cast<EquityMarginLegData>(data.concreteLegData());
    QL_REQUIRE(eqMarginData, "Wrong LegType, expected EquityMargin");
     
    QuantLib::ext::shared_ptr<EquityLegData> eqData = eqMarginData->equityLegData();
    
    string eqName = eqData->eqName();
    auto eqCurve = *engineFactory->market()->equityCurve(eqName, configuration);

    Currency dataCurrency = parseCurrencyWithMinors(data.currency());
    Currency eqCurrency = eqCurve->currency();

    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex = nullptr;
    // if equity currency differs from the leg currency we need an FxIndex
    if (dataCurrency != eqCurrency) {
        QL_REQUIRE(eqData->fxIndex() != "",
                   "No FxIndex - if equity currency differs from leg currency an FxIndex must be provided");
        fxIndex = buildFxIndex(eqData->fxIndex(), data.currency(), eqCurrency.code(), engineFactory->market(),
                               configuration);
    }

    Leg result = makeEquityMarginLeg(data, eqCurve, fxIndex, openEndDateReplacement);
    addToRequiredFixings(result, QuantLib::ext::make_shared<ore::data::FixingDateGetter>(requiredFixings));
    return result;
}

} // namespace data
} // namespace ore
