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

/*! \file dkkcibor.hpp
    \brief DKK-CIBOR index
    \ingroup indexes
*/

#ifndef quantext_dkkcibor_hpp
#define quantext_dkkcibor_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/denmark.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

//! DKK-CIBOR index
/*! DKK-CIBOR rate overseen by Danish Bankers Association.

    See <http://www.finansraadet.dk/Pages/forside.aspx>.

    \remark Using Denmark calendar, should be Copenhagen.
            There is another index, DKK-CIBOR2, that has a spot lag of 2D.

    \warning Check roll convention and EOM.

            \ingroup indexes
*/
class DKKCibor : public IborIndex {
public:
    DKKCibor(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("DKK-CIBOR", tenor, 0, DKKCurrency(), Denmark(), ModifiedFollowing, false, Actual360(), h) {}
};
} // namespace QuantExt

#endif
