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

/*! \file qle/termstructures/iborfallbackcurve.hpp
    \brief projection curve for ibor fallback indices
*/

#pragma once

#include <ql/indexes/iborindex.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;
class SpreadedIndexYieldCurve : public QuantLib::YieldTermStructure {
public:
    SpreadedIndexYieldCurve(const QuantLib::ext::shared_ptr<IborIndex>& originalIndex,
                            const QuantLib::ext::shared_ptr<IborIndex>& referenceIndex, 
                            const Real spread, const bool spreadOnReference = true);

    QuantLib::ext::shared_ptr<IborIndex> originalIndex() const { return originalIndex_; };
    QuantLib::ext::shared_ptr<IborIndex> referenceIndex() const { return referenceIndex_; };
    Real spread() const { return spread_; }

    const Date& referenceDate() const override;
    Date maxDate() const override;
    Calendar calendar() const override;
    Natural settlementDays() const override;

protected:
    Real discountImpl(QuantLib::Time t) const override;

private:
    QuantLib::ext::shared_ptr<IborIndex> originalIndex_;
    QuantLib::ext::shared_ptr<IborIndex> referenceIndex_;
    Real spread_;
    bool spreadOnReference_ = true;
};

class IborFallbackCurve : public SpreadedIndexYieldCurve {
public:
    IborFallbackCurve(const QuantLib::ext::shared_ptr<IborIndex>& originalIndex,
                      const QuantLib::ext::shared_ptr<OvernightIndex>& rfrIndex, const Real spread,
                      const Date& switchDate)
        : SpreadedIndexYieldCurve(originalIndex, rfrIndex, spread), switchDate_(switchDate){};

    QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex() const { 
        auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(referenceIndex());
        QL_REQUIRE(on, "IborFallbackCurve: reference index is not an OvernightIndex");
        return on; 
    }
    const Date& switchDate() const { return switchDate_; };

private:
    Real discountImpl(QuantLib::Time t) const override;
    Date switchDate_;
};

} // namespace QuantExt
