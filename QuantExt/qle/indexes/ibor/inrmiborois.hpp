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

/*! \file inrmiborois.hpp
    \brief INR-MIBOROIS index
    \ingroup indexes
*/

#ifndef quantext_inrmiborois_hpp
#define quantext_inrmiborois_hpp

#include <ql/currencies/asia.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/india.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
using namespace QuantLib;

//! INR-MIBOROIS index
/*! Benchmark rate for Overnight Mumbai Interbank Outright Rate (MIBOR) rate overseen by FBIL.

    See <https://fbil.org.in/>.

    \remark Using India calendar, should be Mumbai (excluding Saturday).

    \warning Check roll convention and EOM.

            \ingroup indexes
*/
class INRMiborOis : public OvernightIndex {
public:
    INRMiborOis(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("INR-MIBOROIS", 0, INRCurrency(), India(), Actual365Fixed(), h) {}
};
} // namespace QuantExt

#endif
