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

/*! \file interpolatediborindex.hpp
    \brief Interpolated Ibor Index
    \ingroup indexes
*/

#pragma once

#include <ql/indexes/iborindex.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;

namespace QuantExt {

class InterpolatedIborIndex : public QuantLib::InterestRateIndex {
public:
    // if longIndex = shortIndex, the original index is reproduced, if in addition parCouponMode = true
    // the index is estimated on the period valueDate, valueDate+calendarDays
    InterpolatedIborIndex(const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& shortIndex,
                          const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& longIndex,
                          const Size calendarDays, const QuantLib::Rounding& rounding = QuantLib::Rounding(),
                          const QuantLib::Handle<QuantLib::YieldTermStructure> overwriteEstimationCurve =
                              QuantLib::Handle<QuantLib::YieldTermStructure>(),
                          const bool parCouponMode = false);

    // override
    Date maturityDate(const Date& valueDate) const override;
    Real forecastFixing(const Date& fixingDate) const override;
    Real pastFixing(const Date& fixingDate) const override;
    bool allowsNativeFixings() override { return false; };

    // inspectors
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex> shortIndex() const { return shortIndex_; }
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex> longIndex() const { return longIndex_; }
    Size calendarDays() const { return calendarDays_; }
    const QuantLib::Rounding rounding() const { return rounding_; }
    const QuantLib::Handle<QuantLib::YieldTermStructure> overwriteEstimationCurve() {
        return overwriteEstimationCurve_;
    }
    bool parCouponMode() const { return parCouponMode_; }

    Real shortWeight(const Date& fixingDate) const;
    Real longWeight(const Date& fixingDate) const;


    // clone
    QuantLib::ext::shared_ptr<InterpolatedIborIndex>
    clone(const QuantLib::Handle<QuantLib::YieldTermStructure>& shortForwardCurve,
          const QuantLib::Handle<QuantLib::YieldTermStructure>& longForwardCurve) const;
    QuantLib::ext::shared_ptr<InterpolatedIborIndex>
    clone(const QuantLib::Handle<QuantLib::YieldTermStructure>& overwriteForwardCurve) const;

private:
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex> shortIndex_, longIndex_;
    const Size calendarDays_;
    const QuantLib::Rounding rounding_;
    const QuantLib::Handle<QuantLib::YieldTermStructure> overwriteEstimationCurve_;
    const bool parCouponMode_;

    bool noInterpolation_;
};

} // namespace QuantExt
