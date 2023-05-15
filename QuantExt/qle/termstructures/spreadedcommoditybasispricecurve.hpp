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

/*! \file qle/termstructures/spreadedcommoditybasispricecurve.hpp
    \brief A commodity price curve created from a base price curve and a collection of basis quotes
    \ingroup termstructures
*/

#pragma once

#include <qle/termstructures/spreadedpricetermstructure.hpp>
#include <qle/termstructures/commoditybasispricecurve.hpp>

namespace QuantExt {

class SpreadedCommodityBasisPriceCurve : public QuantExt::CommodityBasisPriceTermStructure,
                                         public QuantLib::LazyObject {
public:
    SpreadedCommodityBasisPriceCurve(const boost::shared_ptr<CommodityBasisPriceTermStructure>& referenceCurve,
                                     const boost::shared_ptr<CommodityIndex>& baseIndex,
                                     const boost::shared_ptr<PriceTermStructure>& spreadCurve)
        : CommodityBasisPriceTermStructure(
              referenceCurve->referenceDate(), referenceCurve->calendar(), referenceCurve->dayCounter(),
              referenceCurve->basisFutureExpiryCalculator(), baseIndex->priceCurve(), baseIndex, referenceCurve->baseFutureExpiryCalculator(),
              referenceCurve->addBasis(), referenceCurve->monthOffset(), referenceCurve->baseIsAveraging()),
          spreadCurve_(spreadCurve) {
        registerWith(baseIndex_);
        registerWith(spreadCurve_);
    };

    QuantLib::Date maxDate() const override { return spreadCurve_->maxDate(); }
    
    void update() override {
        LazyObject::update();
        TermStructure::update();
    }

    QuantLib::Natural settlementDays() const override { return spreadCurve_->settlementDays(); }
    

    QuantLib::Time minTime() const override { return spreadCurve_->minTime(); }
    const QuantLib::Currency& currency() const override { return spreadCurve_->currency(); }
    std::vector<QuantLib::Date> pillarDates() const override { return spreadCurve_->pillarDates(); }

private:
    void performCalculations() const override{}
    QuantLib::Real priceImpl(QuantLib::Time t) const override { return spreadCurve_->price(t, allowsExtrapolation()); }

    boost::shared_ptr<QuantExt::PriceTermStructure> spreadCurve_;
};
} // namespace QuantExt