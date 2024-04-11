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

#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/indexes/genericiborindex.hpp>

namespace QuantExt {

GenericIborIndex::GenericIborIndex(const Period& tenor, const Currency& ccy, const Handle<YieldTermStructure>& h)
    : IborIndex(ccy.code() + "-GENERIC", tenor, 2, ccy, TARGET(), Following, false, Actual360(), h) {}

Rate GenericIborIndex::pastFixing(const Date& fixingDate) const {
    Date fixDate = fixingCalendar().adjust(Settings::instance().evaluationDate(), Following);
    return fixing(fixDate, true);
}

QuantLib::ext::shared_ptr<IborIndex> GenericIborIndex::clone(const Handle<YieldTermStructure>& h) const {
    return QuantLib::ext::make_shared<GenericIborIndex>(tenor(), currency(), h);
}

} // namespace QuantExt
