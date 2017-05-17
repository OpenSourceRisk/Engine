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

/*! \file plnwibor.hpp
    \brief PLN-WIBOR index
    \ingroup indexes
*/

#ifndef quantext_plnwibor_hpp
#define quantext_plnwibor_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/poland.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;

namespace QuantExt {

//! PLN-WIBOR index
/*! PLN-WIBOR rate published by ACI Polska.

    See <http://acipolska.pl/english.html>.

    \remark Using Poland calendar, should be Warsaw.

    \warning Check roll convention and EOM.

            \ingroup indexes
*/
class PLNWibor : public IborIndex {
public:
    PLNWibor(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("PLN-WIBOR", tenor, 2, PLNCurrency(), Poland(), ModifiedFollowing, false, Actual365Fixed(), h) {}
};
} // namespace QuantExt

#endif
