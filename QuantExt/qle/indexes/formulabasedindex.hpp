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

/*! \file qle/indexes/formulabasedindex.hpp
    \brief formula based index
    \ingroup indexes
*/

#pragma once

#include <qle/math/compiledformula.hpp>

#include <ql/indexes/interestrateindex.hpp>

namespace QuantExt {
using namespace QuantLib;

//! formula based index class
/*! The variables in the given formula must correspond to index vector, both w.r.t.
  size and position.

  Warning:
  - tenor is set to 0d for this index, since it doesn't have a meaningful interpretation
  - fixingDays are set to the value of the first index, because Null<Size> could be interpreted
               as the actual (big) number of fixing days by client code
  - currency is set to Currency()
  - dayCounter is set to SimpleDayCounter(), because it's used in InterestRateIndex to set the name,
               which is overwritten here though
  - fixingCalendar should be explicitly given, since it is used to derive the fixing date in
                   formula based coupons (and to determine valid fixing dates)
*/

class FormulaBasedIndex : public QuantLib::InterestRateIndex {
public:
    FormulaBasedIndex(const std::string& familyName, const std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>>& indices,
                      const CompiledFormula& formula, const Calendar& fixingCalendar);

    // InterestRateIndex interface
    Date maturityDate(const Date& valueDate) const override {
        QL_FAIL("FormulaBasedIndex does not provide a single maturity date");
    }
    Rate forecastFixing(const Date& fixingDate) const override;
    Rate pastFixing(const Date& fixingDate) const override;
    bool allowsNativeFixings() override { return false; }

    // Inspectors
    const std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>>& indices() const { return indices_; }
    const CompiledFormula& formula() const { return formula_; }

private:
    const std::vector<QuantLib::ext::shared_ptr<InterestRateIndex>> indices_;
    const CompiledFormula formula_;
};

} // namespace QuantExt
