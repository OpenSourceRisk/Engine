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

/*! \file czkpribor.hpp
    \brief CZK-PRIBOR index
    \ingroup indexes
*/

#ifndef quantext_czkpribor_hpp
#define quantext_czkpribor_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/czechrepublic.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

//! CZK-PRIBOR index
/*! CZK-PRIBOR rate overseen by the Czech National Bank.

    See <http://www.cnb.cz/en/financial_markets/money_market/pribor/>.

    \remark Using CzechRepublic calendar, should be Prague.

    \warning Check roll convention and EOM.

            \ingroup indexes
*/
class CZKPribor : public IborIndex {
public:
    CZKPribor(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("CZK-PRIBOR", tenor, 2, CZKCurrency(), CzechRepublic(), ModifiedFollowing, false, Actual360(), h) {}
};
} // namespace QuantExt

#endif
