/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/formulabasedlegbuilder.hpp>

#include <ored/portfolio/formulabasedindexbuilder.hpp>
#include <ored/portfolio/formulabasedlegdata.hpp>
namespace ore {
namespace data {

Leg FormulaBasedLegBuilder::buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                                     RequiredFixings& requiredFixings, const string& configuration,
                                     const QuantLib::Date& openEndDateReplacement, const bool useXbsCurves) const {
    auto formulaData = QuantLib::ext::dynamic_pointer_cast<FormulaBasedLegData>(data.concreteLegData());
    QL_REQUIRE(formulaData, "Wrong LegType, expected Formula");
    string formula = formulaData->formulaBasedIndex();
    Calendar cal;
    if (formulaData->fixingCalendar() != "")
        cal = parseCalendar(formulaData->fixingCalendar());
    std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>> indexMaps;
    auto formulaIndex =
        makeFormulaBasedIndex(formula, engineFactory->market(), configuration, indexMaps, cal);
    Leg result = makeFormulaBasedLeg(data, formulaIndex, engineFactory, indexMaps, openEndDateReplacement);
    // add required fixing dates
    for (auto const& m : indexMaps) {
        for (auto const& c : result) {
            auto f = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c);
            QL_REQUIRE(f != nullptr, "expected FloatingRateCoupon in FormulaBasedLegBuilder");
            requiredFixings.addFixingDate(f->fixingDate(), m.first, f->date(), false);
        }
    }
    return result;
}

} // namespace data
} // namespace ore
