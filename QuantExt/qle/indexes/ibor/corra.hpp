/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/ibor/corra.hpp
    \brief Canadian Overnight Repo Rate Average (CORRA) index class
    \ingroup indexes
*/

#ifndef quantext_corra_hpp
#define quantext_corra_hpp

#include <ql/currencies/america.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/canada.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantExt {
//! %CORRA rate
/*!
    \remark Using Canada calendar

    \ingroup indexes
*/
class CORRA : public OvernightIndex {
public:
    explicit CORRA(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("CORRA", 0, CADCurrency(), Canada(), Actual365Fixed(), h) {}
};
} // namespace QuantExt

#endif