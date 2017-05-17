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

/*! \file mxntiie.hpp
    \brief MXN-TIIE index
    \ingroup indexes
*/

#ifndef quantext_mxntiie_hpp
#define quantext_mxntiie_hpp

#include <ql/currencies/america.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/mexico.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;

namespace QuantExt {

//! MXN-TIIE index
/*! MXN-TIIE rate published by Banco de Mexico.

    See <http://www.banxico.org.mx/indexEn.html>.

    \remark Using Mexico calendar, should be Meixco City.

    \warning Check roll convention and EOM.

            \ingroup indexes
*/
class MXNTiie : public IborIndex {
public:
    MXNTiie(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("MXN-TIIE", tenor, 1, MXNCurrency(), Mexico(), Following, false, Actual360(), h) {}
};
} // namespace QuantExt

#endif
