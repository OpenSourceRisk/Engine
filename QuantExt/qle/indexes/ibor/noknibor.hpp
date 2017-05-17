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

/*! \file noknibor.hpp
    \brief NOK-NIBOR index
    \ingroup indexes
*/

#ifndef quantext_noknibor_hpp
#define quantext_noknibor_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/norway.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

//! NOK-NIBOR index
/*! NOK-NIBOR rate published by Oslo Boers.

    See <http://www.oslobors.no/ob_eng>.

    \remark Using Norway calendar, should be Oslo.

    \warning Check roll convention and EOM.

            \ingroup indexes
*/
class NOKNibor : public IborIndex {
public:
    NOKNibor(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("NOK-NIBOR", tenor, 2, NOKCurrency(), Norway(), ModifiedFollowing, false, Actual360(), h) {}
};
} // namespace QuantExt

#endif
