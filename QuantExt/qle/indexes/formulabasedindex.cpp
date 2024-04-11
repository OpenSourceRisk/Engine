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

#include <qle/indexes/formulabasedindex.hpp>

#include <ql/time/daycounters/simpledaycounter.hpp>

namespace QuantExt {

FormulaBasedIndex::FormulaBasedIndex(const std::string& familyName,
                                     const std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>>& indices,
                                     const CompiledFormula& formula, const Calendar& fixingCalendar)
    : InterestRateIndex(familyName, 0 * Days, indices.front()->fixingDays(), Currency(), fixingCalendar,
                        SimpleDayCounter()),
      indices_(indices), formula_(formula) {

    std::ostringstream name;
    name << familyName << "(";
    for (Size i = 0; i < indices_.size(); ++i) {
        registerWith(indices_[i]);
        name << indices_[i]->name();
        if (i < indices_.size() - 1)
            name << ", ";
    }
    name << ")";
    name_ = name.str();
} // FormulaBasedIndex

Rate FormulaBasedIndex::forecastFixing(const Date& fixingDate) const {
    std::vector<Real> values;
    for (auto const& i : indices_) {
        // this also handles the case when one of indices has
        // a historic fixing on the evaluation date
        values.push_back(i->fixing(fixingDate, false));
    }
    return formula_(values.begin(), values.end());
}

Rate FormulaBasedIndex::pastFixing(const Date& fixingDate) const {
    std::vector<Real> values;
    for (auto const& i : indices_) {
        Real f = i->pastFixing(fixingDate);
        // if one of the fixings is missing, the fixing of the composed
        // index is also missing, this is indicated by a null value
        if (f == Null<Real>())
            return Null<Real>();
        values.push_back(f);
    }
    return formula_(values.begin(), values.end());
}

} // namespace QuantExt
