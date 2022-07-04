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

/*! \file phpphiref.hpp
    \brief PHP-PHIREF index
    \ingroup indexes
*/

#ifndef quantext_phpphiref_hpp
#define quantext_phpphiref_hpp

#include <ql/currencies/asia.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/calendars/philippines.hpp>

namespace QuantExt {
using namespace QuantLib;

//! PHP-PHIREF index
/*! PHP-PHIREF rate.

No PHP Calendar in QuantLib

\ingroup indexes
*/
class PHPPhiref : public IborIndex {
public:
    PHPPhiref(const Period& tenor, const Handle<YieldTermStructure>& h = Handle<YieldTermStructure>())
        : IborIndex("PHP-PHIREF", tenor, 1, PHPCurrency(), Philippines(), ModifiedFollowing, false, Actual360(), h) {}
};
} // namespace QuantExt

#endif
