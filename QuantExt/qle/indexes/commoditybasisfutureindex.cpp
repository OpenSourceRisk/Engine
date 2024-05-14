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

#include <qle/indexes/commoditybasisfutureindex.hpp>

namespace QuantExt {
CommodityBasisFutureIndex::CommodityBasisFutureIndex(const std::string& underlyingName,
                                                     const QuantLib::Date& expiryDate,
                                                     const QuantLib::Calendar& fixingCalendar,
                                                     const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& basisFec,
                                                     const QuantLib::ext::shared_ptr<QuantExt::CommodityIndex>& baseIndex,
                                                     const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& baseFec,
                                                     const QuantLib::Handle<QuantExt::PriceTermStructure>& priceCurve,
                                                     const bool addBasis, const QuantLib::Size monthOffset,
                                                     const bool baseIsAveraging, const bool priceAsHistoricalFixing)
    : CommodityFuturesIndex(underlyingName, expiryDate, fixingCalendar, priceCurve), basisFec_(basisFec),
      baseIndex_(baseIndex), baseFec_(baseFec), addBasis_(addBasis), monthOffset_(monthOffset),
      baseIsAveraging_(baseIsAveraging), priceAsHistoricalFixing_(priceAsHistoricalFixing) {
    QL_REQUIRE(expiryDate_ != Date(), "non-empty expiry date expected for CommodityFuturesIndex");
    QL_REQUIRE(baseIndex_ != nullptr, "non-null baseIndex required for CommodityBasisFutureIndex");
    QL_REQUIRE(basisFec_ != nullptr,
               "non-null future expiry calculator for the basis conventions CommodityBasisFutureIndex");
    QL_REQUIRE(baseFec_ != nullptr,
               "non-null future expiry calculator for the base conventions CommodityBasisFutureIndex");
    registerWith(baseIndex);
    if (priceAsHistoricalFixing_ == false)
        cashflow_ = baseCashflow();
}

CommodityBasisFutureIndex::CommodityBasisFutureIndex(
    const std::string& underlyingName, const QuantLib::Date& expiryDate, const QuantLib::Calendar& fixingCalendar,
    const QuantLib::ext::shared_ptr<CommodityBasisPriceTermStructure>& priceCurve)
    : CommodityBasisFutureIndex(underlyingName, expiryDate, fixingCalendar, priceCurve->basisFutureExpiryCalculator(),
                                priceCurve->baseIndex(), priceCurve->baseFutureExpiryCalculator(),
                                Handle<PriceTermStructure>(priceCurve), priceCurve->addBasis(),
                                priceCurve->monthOffset(), priceCurve->averagingBaseCashflow(),
                                priceCurve->priceAsHistoricalFixing()) {}

QuantLib::ext::shared_ptr<CommodityIndex>
CommodityBasisFutureIndex::clone(const QuantLib::Date& expiry,
                                 const boost::optional<QuantLib::Handle<PriceTermStructure>>& ts) const {
    const auto& pts = ts ? *ts : priceCurve();
    const auto& ed = expiry == Date() ? expiryDate() : expiry;
    return QuantLib::ext::make_shared<CommodityBasisFutureIndex>(underlyingName(), ed, fixingCalendar(), basisFec_, baseIndex_,
                                                         baseFec_, pts, addBasis_, monthOffset_, baseIsAveraging_,
                                                         priceAsHistoricalFixing_);
}

QuantLib::ext::shared_ptr<QuantLib::CashFlow> CommodityBasisFutureIndex::baseCashflow(const QuantLib::Date& paymentDate) const {
    // Fail-safe if expiryDate_ is not a future expiry date 
    auto nextFutureExpiry = basisFec_->nextExpiry(true, expiryDate_);
    // Imply the contract month from the future expiry data
    auto contractDate = basisFec_->contractDate(nextFutureExpiry);
    Date periodStart = Date(1,contractDate.month(), contractDate.year()) - monthOffset_ * Months;
    Date periodEnd = (periodStart + 1 * Months) - 1 * Days;
    // Build the corresponding base-future casflow
    return makeCommodityCashflowForBasisFuture(periodStart, periodEnd, baseIndex_, baseFec_, baseIsAveraging_,
                                               paymentDate);
}

QuantLib::Real CommodityBasisFutureIndex::pastFixing(const QuantLib::Date& fixingDate) const {
    auto basisFixing = CommodityFuturesIndex::pastFixing(fixingDate);
    if (priceAsHistoricalFixing_) {
        return basisFixing;
    } else if (basisFixing == QuantLib::Null<Real>()) {
        return QuantLib::Null<Real>();
    } else {
        return addBasis_ ? cashflow_->amount() + basisFixing : cashflow_->amount() - basisFixing;
    }
}
} // namespace QuantExt
