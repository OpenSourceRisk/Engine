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

/*! \file nzdbkbm.hpp
    \brief NZD-BKBM index
    \ingroup indexes
*/

#ifndef quantext_nzdbkbm_hpp
#define quantext_nzdbkbm_hpp

#include <ql/currencies/oceania.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/newzealand.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;

namespace QuantExt {

//! NZD-BKBM index
/*! NZD-BKBM rate published by NZFMA.

    See <http://www.nzfma.org>.

    \warning Check roll convention and EOM.

            \ingroup indexes
*/
class NZDBKBM : public IborIndex {
public:
    NZDBKBM(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("NZD-BKBM", tenor, 2, NZDCurrency(), NewZealand(), ModifiedFollowing, false, ActualActual(), h) {}
};
} // namespace QuantExt

#endif
