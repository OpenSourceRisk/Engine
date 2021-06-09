/*
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

/*! \file sekstina.hpp
    \brief SEK T/N rate
    \ingroup indexes
*/

#ifndef quantext_sekstina_hpp
#define quantext_sekstina_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/sweden.hpp>
#include <ql/time/daycounters/actual360.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %SEK STINA
/*! %SEK Stockholm T/N Interbank Average rate

*/
class SEKStina : public OvernightIndex {
public:
    SEKStina(const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : OvernightIndex("SEK-STINA", 1, SEKCurrency(), Sweden(), Actual360(), h) {}
};
} // namespace QuantExt

#endif
