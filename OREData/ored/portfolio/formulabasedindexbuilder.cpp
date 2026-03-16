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

#include <ored/portfolio/formulabasedindexbuilder.hpp>
#include <ored/utilities/formulaparser.hpp>

#include <ored/utilities/indexparser.hpp>

#include <ql/time/calendars/jointcalendar.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<QuantExt::FormulaBasedIndex>
makeFormulaBasedIndex(const std::string& formula, const QuantLib::ext::shared_ptr<ore::data::Market> market,
                      const std::string& configuration,
                      std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexMaps,
                      const Calendar& fixingCalendar) {

    indexMaps.clear();
    std::vector<QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>> indices;
    std::vector<std::string> variables;
    QuantExt::CompiledFormula compiledFormula = parseFormula(formula, variables);
    Calendar fixCal = NullCalendar();
    for (auto const& v : variables) {
        QuantLib::ext::shared_ptr<InterestRateIndex> index;
        QuantLib::ext::shared_ptr<IborIndex> dummyIborIndex;
        if (ore::data::tryParseIborIndex(v, dummyIborIndex)) {
            index = *market->iborIndex(v, configuration);
        } else {
            // if it is not an ibor index, we know it must be a swap index
            index = *market->swapIndex(v, configuration);
        }
        QL_REQUIRE(index != nullptr, "makeFormulaBasedIndex("
                                         << formula << "): variable \"" << v
                                         << "\" could not resolved as an ibor or swap index in the given market");
        indices.push_back(index);
        fixCal = JointCalendar(fixCal, index->fixingCalendar());
        indexMaps[v] = index;
    }

    QuantLib::ext::shared_ptr<QuantExt::FormulaBasedIndex> fbi = QuantLib::ext::make_shared<QuantExt::FormulaBasedIndex>(
        "FormulaBasedIndex", indices, compiledFormula, fixingCalendar == Calendar() ? fixCal : fixingCalendar);
    return fbi;
}

} // namespace data
} // namespace ore
