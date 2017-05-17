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

/*! \file skkbribor.hpp
    \brief SKK-BRIBOR index
    \ingroup indexes
*/

#ifndef quantext_skkbribor_hpp
#define quantext_skkbribor_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/slovakia.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

//! SKK-BRIBOR index
/*! SKK-BRIBOR rate overseen by the central bank of the Slovak Republic.

    See <http://www.nbs.sk/en/home>.

    \remark Using Slovakia calendar, should be Bratislava.

    \warning Check roll convention and EOM.

            \ingroup indexes
*/
class SKKBribor : public IborIndex {
public:
    SKKBribor(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("SKK-BRIBOR", tenor, 2, SKKCurrency(), Slovakia(), ModifiedFollowing, false, Actual360(), h) {}
};
} // namespace QuantExt

#endif
