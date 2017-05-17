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

/*! \file audbbsw.hpp
    \brief AUD-BBSW index
    \ingroup indexes
*/

#ifndef quantext_audbbsw_hpp
#define quantext_audbbsw_hpp

#include <ql/currencies/oceania.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/australia.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;

namespace QuantExt {

//! AUD-BBSW index
/*! AUD-BBSW rate fixed by the AFMA.

    See <http://www.afma.com.au/data/bbsw.html>.

    \remark Using Australia calendar, should be Sydney.

    \warning Convention should be Modified Following Bimonthly.
    \warning Check EOM.

            \ingroup indexes
*/
class AUDbbsw : public IborIndex {
public:
    AUDbbsw(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("AUD-BBSW", tenor, 0, AUDCurrency(), Australia(), ModifiedFollowing, false, Actual365Fixed(), h) {}
};
} // namespace QuantExt

#endif
