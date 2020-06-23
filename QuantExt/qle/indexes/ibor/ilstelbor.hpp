/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ilstelbor.hpp
    \brief ILS-TELBOR index
    \ingroup indexes
*/

#ifndef quantext_ilstelbor_hpp
#define quantext_ilstelbor_hpp

#include <ql/currencies/asia.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/israel.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/calendars/israel.hpp>

namespace QuantExt {
using namespace QuantLib;

//! ILS-TELBOR index
/*! ILS-TELBOR rate overseen by Bank of Israel.

    See <https://www.boi.org.il/en/Markets/TelborMarket/Pages/telbor.aspx>.

            \ingroup indexes
*/
class ILSTelbor : public IborIndex {
public:
    ILSTelbor(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("ILS-TELBOR", tenor, 2, ILSCurrency(), QuantExt::Israel(QuantExt::Israel::Telbor),
                    ModifiedFollowing, false, Actual360(), h) {}
};
} // namespace QuantExt

#endif
