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

/*! \file sora.hpp
    \brief Singapore Overnight Average Rate (sora)
    \ingroup indexes
*/

#ifndef quantext_sora_hpp
#define quantext_sora_hpp

#include <ql/currencies/asia.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/singapore.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %SGD %SORA rate
/*! Singapore Overnight Rate Average (SORA).

    See <https://www.mas.gov.sg/monetary-policy/sora>.

    \remark There is a publication lag of 1 business day.
            Using Malaysian calendar, should be Singapore.

            \ingroup indexes
*/
class Sora : public OvernightIndex {
public:
    Sora(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("SGD-SORA", 0, SGDCurrency(), Singapore(), Actual365Fixed(), h) {}
};
} // namespace QuantExt

#endif
