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

/*! \file qle/indexes/fallbackovernightindex.hpp
    \brief wrapper class for overnight index managing the fallback rules
    \ingroup indexes
*/

#pragma once

#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/indexes/fallbackiborindex.hpp>
#include <ql/indexes/iborindex.hpp>

#include <vector>

namespace QuantExt {
using namespace QuantLib;

class FallbackOvernightIndex : public QuantLib::OvernightIndex {
public:
    FallbackOvernightIndex(const QuantLib::ext::shared_ptr<OvernightIndex> originalIndex,
			   const QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex, const Real spread, const Date& switchDate,
			   const bool useRfrCurve);
    FallbackOvernightIndex(const QuantLib::ext::shared_ptr<OvernightIndex> originalIndex,
			   const QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex, const Real spread, const Date& switchDate,
			   const Handle<YieldTermStructure>& forwardingCurve);

    void addFixing(const Date& fixingDate, Real fixing, bool forceOverwrite = false) override {
        return iborFallbackIndex_->addFixing(fixingDate, fixing, forceOverwrite);
    }
    Real fixing(const Date& fixingDate, bool forecastTodaysFixing = false) const override {
		return iborFallbackIndex_->fixing(fixingDate, forecastTodaysFixing);
	}
    Rate pastFixing(const Date& fixingDate) const override {
		return iborFallbackIndex_->pastFixing(fixingDate);
    }
    QuantLib::ext::shared_ptr<IborIndex> clone(const Handle<YieldTermStructure>& forwarding) const override {
        return QuantLib::ext::make_shared<FallbackOvernightIndex>(originalIndex(), rfrIndex(), spread(), switchDate(),
                                                                 forwarding);
    }

    QuantLib::ext::shared_ptr<OvernightIndex> originalIndex() const {
        auto oi = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(iborFallbackIndex_->originalIndex());
        QL_REQUIRE(oi, "original index is not an OvernightIndex");
        return oi;
    }
    QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex() const { return iborFallbackIndex_->rfrIndex(); }
    Real spread() const { return iborFallbackIndex_->spread(); }
    const Date& switchDate() const { return iborFallbackIndex_->switchDate(); }


protected:
    Rate forecastFixing(const Date& valueDate, const Date& endDate, Time t) const override {
		return iborFallbackIndex_->forecastFixing(valueDate, endDate, t);
    }

private:
    std::unique_ptr<FallbackIborIndex> iborFallbackIndex_;
};

} // namespace QuantExt
