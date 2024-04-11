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

#pragma once

#include <qle/cashflows/overnightindexedcoupon.hpp>

#include <ql/indexes/iborindex.hpp>

#include <vector>

namespace QuantExt {
using namespace QuantLib;

class FallbackIborIndex : public QuantLib::IborIndex {
public:
    FallbackIborIndex(const QuantLib::ext::shared_ptr<IborIndex> originalIndex,
                      const QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex, const Real spread, const Date& switchDate,
                      const bool useRfrCurve);
    FallbackIborIndex(const QuantLib::ext::shared_ptr<IborIndex> originalIndex,
                      const QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex, const Real spread, const Date& switchDate,
                      const Handle<YieldTermStructure>& forwardingCurve);

    void addFixing(const Date& fixingDate, Real fixing, bool forceOverwrite = false) override;
    Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const override;
    Rate pastFixing(const Date& fixingDate) const override;
    QuantLib::ext::shared_ptr<IborIndex> clone(const Handle<YieldTermStructure>& forwarding) const override;

    QuantLib::ext::shared_ptr<IborIndex> originalIndex() const;
    QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex() const;
    Real spread() const;
    const Date& switchDate() const;

    QuantLib::ext::shared_ptr<OvernightIndexedCoupon> onCoupon(const Date& iborFixingDate,
                                                       const bool telescopicValueDates = false) const;

private:
    Rate forecastFixing(const Date& valueDate, const Date& endDate, Time t) const override;
    QuantLib::ext::shared_ptr<IborIndex> originalIndex_;
    QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex_;
    Real spread_;
    Date switchDate_;
};

} // namespace QuantExt
