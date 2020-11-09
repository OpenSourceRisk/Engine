/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file cnhshibor.hpp
    \brief CNH-SHIBOR index
    \ingroup indexes
*/

#ifndef quantext_cnh_shibor_hpp
#define quantext_cnh_shibor_hpp

#include <qle/currencies/asia.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/hongkong.hpp>
#include <ql/time/daycounters/actual360.hpp>

namespace QuantExt {
using namespace QuantLib;

//! CNH-SHIBOR index
/*! CNH-SHIBOR rate overseen by The Hong Kong Association of Banks.

    See <http://www.hkab.org.hk>.

    \warning Check roll convention and EOM.

            \ingroup indexes
*/
class CNHShibor : public IborIndex {
public:
    CNHShibor(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("CNH-SHIBOR", tenor, 0, CNHCurrency(), HongKong(), ModifiedFollowing, false, Actual360(), h) {}
};
} // namespace QuantExt

#endif
