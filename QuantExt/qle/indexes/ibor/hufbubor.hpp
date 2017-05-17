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

/*! \file hufbubor.hpp
    \brief HUF-BUBOR index
    \ingroup indexes
*/

#ifndef quantext_hufbubor_hpp
#define quantext_hufbubor_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/hungary.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

//! HUF-BUBOR index
/*! HUF-BUBOR rate overseen by MFT in association with the National
    Bank of Hungary.

    See <http://www.acihungary.hu/en/index.php?id=bubor>.

    \remark Using Hungary calendar, should be Budapest.

    \warning Check roll convention and EOM.

            \ingroup indexes
*/
class HUFBubor : public IborIndex {
public:
    HUFBubor(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("HUF-BUBOR", tenor, 2, HUFCurrency(), Hungary(), ModifiedFollowing, false, Actual360(), h) {}
};
} // namespace QuantExt

#endif
