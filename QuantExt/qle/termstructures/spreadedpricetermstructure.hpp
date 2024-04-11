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

/*! \file qle/termstructures/spreadedpricetermstructure.hpp
    \brief Spreaded Term structure of prices
*/

#pragma once

#include <qle/termstructures/pricetermstructure.hpp>

#include <ql/math/interpolation.hpp>
#include <ql/patterns/lazyobject.hpp>

namespace QuantExt {

//! Spreaded Price term structure
class SpreadedPriceTermStructure : public PriceTermStructure, public QuantLib::LazyObject {
public:
    //! times should be consistent with reference curve day counter
    SpreadedPriceTermStructure(const QuantLib::Handle<PriceTermStructure>& referenceCurve,
                               const std::vector<QuantLib::Real>& times,
                               const std::vector<QuantLib::Handle<QuantLib::Quote>>& priceSpreads);

    QuantLib::Date maxDate() const override;
    void update() override;
    const QuantLib::Date& referenceDate() const override;
    QuantLib::Calendar calendar() const override;
    QuantLib::Natural settlementDays() const override;

    QuantLib::Time minTime() const override;
    const QuantLib::Currency& currency() const override;
    std::vector<QuantLib::Date> pillarDates() const override;

private:
    void performCalculations() const override;
    QuantLib::Real priceImpl(QuantLib::Time) const override;

    QuantLib::Handle<PriceTermStructure> referenceCurve_;
    mutable std::vector<QuantLib::Real> times_;
    std::vector<QuantLib::Handle<QuantLib::Quote>> priceSpreads_;

    mutable std::vector<QuantLib::Real> data_;
    QuantLib::ext::shared_ptr<QuantLib::Interpolation> interpolation_;
};

} // namespace QuantExt
