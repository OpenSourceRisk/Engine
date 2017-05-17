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

/*! \file chftois.hpp
    \brief Swiss Franc T/N rate on Reuters page CHFTOIS
    \ingroup indexes
*/

#ifndef quantext_chftois_hpp
#define quantext_chftois_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/switzerland.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

//! %CHF TOIS rate
/*! %CHF T/N rate appearing on page CHFTOIS and calculated by Cosmorex AG,
    a division of Tullet Prebon.

    \remark Using Switzerland calendar, should be Zurich.

            \ingroup indexes
*/
class CHFTois : public OvernightIndex {
public:
    CHFTois(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("CHF-TOIS", 1, CHFCurrency(), Switzerland(), Actual360(), h) {}
};
} // namespace QuantExt

#endif
