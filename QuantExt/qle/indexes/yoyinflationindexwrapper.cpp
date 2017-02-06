/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*
 Copyright (C) 2007 Chris Kenyon

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <qle/indexes/yoyinflationindexwrapper.hpp>

namespace QuantExt {

YoYInflationIndexWrapper::YoYInflationIndexWrapper(const boost::shared_ptr<ZeroInflationIndex> zeroIndex,
                                                   const Handle<YoYInflationTermStructure>& ts)
    : YoYInflationIndex(zeroIndex->familyName(), zeroIndex->region(), zeroIndex->revised(), zeroIndex->interpolated(),
                        true, zeroIndex->frequency(), zeroIndex->availabilityLag(), zeroIndex->currency(), ts),
      zeroIndex_(zeroIndex) {}

Real YoYInflationIndexWrapper::forecastFixing(const Date& fixingDate) const {
    if (!yoyInflationTermStructure().empty())
        return YoYInflationIndex::fixing(fixingDate);
    Real f1 = zeroIndex_->fixing(fixingDate);
    Real f0 = zeroIndex_->fixing(fixingDate - 1 * Years); // FIXME convention ?
    return (f1 - f0) / f0;
}

} // namesapce QuantExt
