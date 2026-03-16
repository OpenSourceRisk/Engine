/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/ibor/termrateindex.hpp
    \brief ibor index class to represent term rates like SOFR-1M, 3M, 6M, 12M
    \ingroup indexes
*/

#pragma once

#include <ql/indexes/iborindex.hpp>

namespace QuantExt {
using namespace QuantLib;

class TermRateIndex : public IborIndex {
public:
    TermRateIndex(const std::string& familyName, const Period& tenor, Natural settlementDays, const Currency& currency,
                  const Calendar& fixingCalendar, BusinessDayConvention convention, bool endOfMonth,
                  const DayCounter& dayCounter, Handle<YieldTermStructure> h = Handle<YieldTermStructure>(),
                  const QuantLib::ext::shared_ptr<OvernightIndex>& rfrIndex = nullptr)
        : IborIndex(familyName, tenor, settlementDays, currency, fixingCalendar, convention, endOfMonth, dayCounter, h),
          rfrIndex_(rfrIndex) {}

    QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex() const { return rfrIndex_; }

private:
    QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex_;
};

} // namespace QuantExt
