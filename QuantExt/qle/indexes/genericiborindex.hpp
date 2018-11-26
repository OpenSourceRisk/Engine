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

/*! \file genericiborindex.hpp
    \brief Generic Ibor Index
    \ingroup indexes
*/

#ifndef _quantext_generic_index_hpp
#define _quantext_generic_index_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

//! Generic Ibor Index
/*! This Ibor Index allows you to wrap any arbitary currency in a generic index.

    We assume 2 settlement days, Target Calendar, ACT/360.

    The name is always CCY-GENERIC so there is no risk of collision with real ibor names
            \ingroup indexes
 */
class GenericIborIndex : public IborIndex {
public:
    GenericIborIndex(const Period& tenor, const Currency& ccy,
                     const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex(ccy.code() + "-GENERIC", tenor, 2, ccy, TARGET(), Following, false, Actual360(), h) {}
};
} // namespace QuantExt

#endif
