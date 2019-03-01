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

/*! \file qle/indexes/ibor/nowa.hpp
    \brief Norwegian Overnight Weighted Average (NOWA) index class
    \ingroup indexes
*/

#ifndef quantext_nowa_hpp
#define quantext_nowa_hpp

#include <ql/currencies/europe.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/calendars/norway.hpp>
#include <ql/time/daycounters/actualactual.hpp>

namespace QuantExt {
/*! %NOWA rate
    https://www.finansnorge.no/en/interest-rates/nowa---the-norwegian-overnight-weighted-average/

    \ingroup indexes
*/
class Nowa : public QuantLib::OvernightIndex {
public:
    explicit Nowa(
        const QuantLib::Handle<QuantLib::YieldTermStructure>& h = QuantLib::Handle<QuantLib::YieldTermStructure>())
        : OvernightIndex("Nowa", 0, QuantLib::NOKCurrency(), QuantLib::Norway(),
                         QuantLib::ActualActual(QuantLib::ActualActual::ISMA), h) {}
};

} // namespace QuantExt

#endif