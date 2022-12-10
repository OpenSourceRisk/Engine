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

/*! \file chfsaron.hpp
    \brief Swiss Average Rate Overnight (SARON)
    \ingroup indexes
*/

#ifndef quantext_saron_hpp
#define quantext_saron_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/switzerland.hpp>
#include <ql/time/daycounters/actual360.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %CHF %SARON rate
/*! Swiss Average Rate Overnight published by SNB.

See <https://www.snb.ch/en/ifor/finmkt/id/finmkt_repos_saron>.

\ingroup indexes
*/
class CHFSaron : public OvernightIndex {
public:
    CHFSaron(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("CHF-SARON", 0, CHFCurrency(), Switzerland(), Actual360(), h) {}
};
} // namespace QuantExt

#endif
