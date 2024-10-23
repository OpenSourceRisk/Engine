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

/*! \file krwcd.hpp
    \brief KRW-CD index
    \ingroup indexes
*/

#ifndef quantext_krwcd_hpp
#define quantext_krwcd_hpp

#include <ql/currencies/asia.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/southkorea.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
using namespace QuantLib;

//! KRW-CD index
/*! KRW-CD rate.

\warning Check roll convention and EOM.

\ingroup indexes
*/
class KRWCd : public IborIndex {
public:
    KRWCd(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("KRW-CD", tenor, 1, KRWCurrency(), SouthKorea(SouthKorea::Settlement), ModifiedFollowing, false,
                    Actual365Fixed(), h) {}
};
} // namespace QuantExt

#endif
