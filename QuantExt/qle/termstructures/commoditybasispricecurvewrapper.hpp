/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file qle/termstructures/commoditybasispricecurvewrapper.hpp
    \brief A commodity price curve created from a generic price curve and a basis curve
    \ingroup termstructures
*/

#pragma once

#include <ql/patterns/lazyobject.hpp>
#include <qle/termstructures/commoditybasispricetermstructure.hpp>

namespace QuantExt {

class CommodityBasisPriceCurveWrapper : public QuantExt::CommodityBasisPriceTermStructure, public QuantLib::LazyObject {
public:
    CommodityBasisPriceCurveWrapper(const QuantLib::Date& referenceDate,
                                    const QuantLib::ext::shared_ptr<PriceTermStructure>& priceCurve,
                                    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& basisFec,
                                    const QuantLib::ext::shared_ptr<CommodityIndex>& baseIndex,
                                    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& baseFec, bool addBasis = true,
                                    QuantLib::Size monthOffset = 0, bool averagingBaseCashflow = false,
                                    bool priceAsHistFixing = true)
        : CommodityBasisPriceTermStructure(referenceDate, basisFec, baseIndex, baseFec, addBasis, monthOffset,
                                           averagingBaseCashflow, priceAsHistFixing),
          priceCurve_(priceCurve) {
        registerWith(priceCurve_);
    }

    CommodityBasisPriceCurveWrapper(const QuantLib::ext::shared_ptr<CommodityBasisPriceTermStructure>& referenceCurve,
                                    const QuantLib::ext::shared_ptr<CommodityIndex>& baseIndex,
                                    const QuantLib::ext::shared_ptr<PriceTermStructure>& priceCurve)
        : CommodityBasisPriceTermStructure(
              referenceCurve->referenceDate(), referenceCurve->calendar(), referenceCurve->dayCounter(),
              referenceCurve->basisFutureExpiryCalculator(), baseIndex, referenceCurve->baseFutureExpiryCalculator(),
              referenceCurve->addBasis(), referenceCurve->monthOffset(), referenceCurve->averagingBaseCashflow(),
              referenceCurve->priceAsHistoricalFixing()),
          priceCurve_(priceCurve) {
        registerWith(priceCurve_);
    };

    QuantLib::Date maxDate() const override { return priceCurve_->maxDate(); }

    void update() override {
        LazyObject::update();
        TermStructure::update();
    }

    QuantLib::Natural settlementDays() const override { return priceCurve_->settlementDays(); }

    QuantLib::Time minTime() const override { return priceCurve_->minTime(); }
    const QuantLib::Currency& currency() const override { return priceCurve_->currency(); }
    std::vector<QuantLib::Date> pillarDates() const override { return priceCurve_->pillarDates(); }

private:
    void performCalculations() const override {}
    QuantLib::Real priceImpl(QuantLib::Time t) const override { return priceCurve_->price(t, allowsExtrapolation()); }

    QuantLib::ext::shared_ptr<QuantExt::PriceTermStructure> priceCurve_;
};
} // namespace QuantExt