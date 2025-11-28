/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/fallbackiborindex.hpp
    \brief wrapper class for ibor index managing the fallback rules
    \ingroup indexes
*/

#include <qle/indexes/fallbackovernightindex.hpp>

namespace QuantExt {

FallbackOvernightIndex::FallbackOvernightIndex(const QuantLib::ext::shared_ptr<OvernightIndex> originalIndex,
					       const QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex, const Real spread,
					       const Date& switchDate, const bool useRfrCurve)
    : OvernightIndex(originalIndex->familyName(), originalIndex->fixingDays(), originalIndex->currency(),
                     originalIndex->fixingCalendar(), originalIndex->dayCounter(), Handle<YieldTermStructure>()) {
    iborFallbackIndex_ = std::make_unique<FallbackIborIndex>(originalIndex, rfrIndex, spread, switchDate, useRfrCurve);
    termStructure_ = iborFallbackIndex_->forwardingTermStructure();
}

FallbackOvernightIndex::FallbackOvernightIndex(const QuantLib::ext::shared_ptr<OvernightIndex> originalIndex,
					       const QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex, const Real spread,
					       const Date& switchDate, const Handle<YieldTermStructure>& forwardingCurve)
    : OvernightIndex(originalIndex->familyName(), originalIndex->fixingDays(),
		     originalIndex->currency(), originalIndex->fixingCalendar(), 
		     originalIndex->dayCounter(), forwardingCurve) {
    iborFallbackIndex_ =
        std::make_unique<FallbackIborIndex>(originalIndex, rfrIndex, spread, switchDate, forwardingCurve);
    termStructure_ = iborFallbackIndex_->forwardingTermStructure();
}

} // namespace QuantExt
