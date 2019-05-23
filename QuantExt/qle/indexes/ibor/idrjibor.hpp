/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file idrjibor.hpp
    \brief IDR-JIBOR index
    \ingroup indexes
*/

#ifndef quantext_idrjibor_hpp
#define quantext_idrjibor_hpp

#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/indonesia.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/currencies/asia.hpp>

namespace QuantExt {
using namespace QuantLib;

//! IDR-JIBOR index
/*! IDR-JIBOR rate.

    See <https://www.bi.go.id/en/moneter/jibor/tentang/Contents/Default.aspx>.

            \ingroup indexes
*/
class IDRJibor : public IborIndex {
public:
    IDRJibor(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("IDR-JIBOR", tenor, 2, IDRCurrency(), Indonesia(), ModifiedFollowing, false, Actual360(), h) {}
};
} // namespace QuantExt

#endif
