/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file tonar.hpp
    \brief Toyko Overnight Average Rate (TONAR)
    \ingroup indexes
*/

#ifndef quantext_tonar_hpp
#define quantext_tonar_hpp

#include <ql/currencies/asia.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/japan.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;

namespace QuantExt {

//! %JPY %TONAR rate
/*! Toyko Overnight Average Rate published by BOJ.

    See <http://www.boj.or.jp/en/statistics/market/short/mutan/>.

    \remark There is a publication lag of 1 business day.
            Using Japan calendar, should be Tokyo.

            \ingroup indexes
*/
class Tonar : public OvernightIndex {
public:
    Tonar(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("TONAR", 0, JPYCurrency(), Japan(), Actual365Fixed(), h) {}
};
} // namespace QuantExt

#endif
