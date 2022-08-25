/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
 Copyright (C) 2022 Oleg Kulkov
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

/*! \file thor.hpp
    \brief THB-THOR index
    \ingroup indexes
*/

#ifndef quantext_thor_hpp
#define quantext_thor_hpp

#include <ql/currencies/asia.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/thailand.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
using namespace QuantLib;

//! THB-THOR index
/*! Benchmark rate for Thai Overnight Repurchase Rate (THOR).
    See <https://www.bot.or.th/English/FinancialMarkets/Documents/THOR_userguide_EN.pdf>.
    \remark Using Thai calendar, to be checked whether identical to Bangkok.
    \warning Check roll convention and EOM.
    \ingroup indexes
*/
class THBThor : public OvernightIndex {
public:
    THBThor(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("THB-THOR", 0, THBCurrency(), Thailand(), Actual365Fixed(), h) {}
};
} // namespace QuantExt

#endif
