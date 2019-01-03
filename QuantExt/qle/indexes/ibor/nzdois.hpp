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

/*! \file nzdois.hpp
    \brief New Zealand Dollars OID Rate
    \ingroup indexes
*/

#ifndef quantext_nzdois_hpp
#define quantext_nzdois_hpp

#include <ql/currencies/oceania.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/newzealand.hpp>
#include <ql/time/daycounters/actual360.hpp>


namespace QuantExt {
using namespace QuantLib;

//! %NZD OIS rate
/*! %New Zealand Dollar Overnight Rate

            \ingroup indexes
*/

class NZDois : public OvernightIndex {
public:
	NZDois(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("NZD-OIS", 1, NZDCurrency(), NewZealand(), Actual360(), h) {}
};
} // namespace QuantExt

#endif
