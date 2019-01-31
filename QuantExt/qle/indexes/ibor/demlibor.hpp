/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file demlibor.hpp
    \brief DEM-LIBOR index
    \ingroup indexes
*/

#ifndef quantext_demlibor_hpp
#define quantext_demlibor_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/libor.hpp>
#include <ql/time/calendars/germany.hpp>
#include <ql/time/daycounters/actual360.hpp>

namespace QuantExt {
using namespace QuantLib;

//! DEM-LIBOR index
/*! DEM LIBOR.

\ingroup indexes
*/
class DEMLibor : public Libor {
public:
    DEMLibor(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : Libor("DEM-LIBOR", tenor, 2, DEMCurrency(), Germany(Germany::Settlement), Actual360(), h) {}
};
} // namespace QuantExt

#endif
