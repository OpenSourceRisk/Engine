/*
 Copyright (C) 2023  Quaternion Risk Management Ltd
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

/*! \file qle/indexes/commoditybasisfutureindex.cpp
    \brief equity index class for holding commodity basis future fixing histories and forwarding.
    \ingroup indexes
*/
#include <qle/indexes/commoditybasisfutureindex.hpp>

namespace QuantExt {
CommodityBasisFutureIndex::CommodityBasisFutureIndex(const std::string& underlyingName,
                                                     const QuantLib::Date& expiryDate,
                                                     const QuantLib::Calendar& fixingCalendar,
                                                     const boost::shared_ptr<FutureExpiryCalculator>& basisFec,
                                                     const boost::shared_ptr<QuantExt::CommodityIndex>& baseIndex,
                                                     const boost::shared_ptr<FutureExpiryCalculator>& baseFec,
                                                     const QuantLib::Handle<QuantExt::PriceTermStructure>& priceCurve,
                                                     const bool addBasis, const QuantLib::Size monthOffset,
                                                     const bool baseIsAveraging)
    : CommodityFuturesIndex(underlyingName, expiryDate, fixingCalendar, priceCurve), basisFec_(basisFec),
      baseIndex_(baseIndex), baseFec_(baseFec), addBasis_(addBasis), monthOffset_(monthOffset),
      baseIsAveraging_(baseIsAveraging) {
    QL_REQUIRE(expiryDate_ != Date(), "non-empty expiry date expected CommodityFuturesIndex");
    QL_REQUIRE(baseIndex_ != nullptr, "non-null baseIndex required for CommodityBasisFutureIndex");
    QL_REQUIRE(baseFec_ != nullptr,
               "non-null future expiry calculator for the basis conventions CommodityBasisFutureIndex");
    QL_REQUIRE(baseFec_ != nullptr,
               "non-null future expiry calculator for the base conventions CommodityBasisFutureIndex");
    registerWith(baseIndex);
    registerWith(baseIndex->priceCurve());
    cashflow_ = baseCashflow();
}

CommodityBasisFutureIndex::CommodityBasisFutureIndex(
    const std::string& underlyingName, const QuantLib::Date& expiryDate, const QuantLib::Calendar& fixingCalendar,
    const boost::shared_ptr<CommodityBasisPriceTermStructure>& priceCurve)
    : CommodityBasisFutureIndex(underlyingName, expiryDate, fixingCalendar, priceCurve->basisFutureExpiryCalculator(),
                                priceCurve->baseIndex(), priceCurve->baseFutureExpiryCalculator(),
                                Handle<PriceTermStructure>(priceCurve), priceCurve->addBasis(),
                                priceCurve->monthOffset(), priceCurve->baseIsAveraging()) {}

boost::shared_ptr<CommodityIndex>
CommodityBasisFutureIndex::clone(const QuantLib::Date& expiry,
                                 const boost::optional<QuantLib::Handle<PriceTermStructure>>& ts) const {
    const auto& pts = ts ? *ts : priceCurve();
    const auto& ed = expiry == Date() ? expiryDate() : expiry;
    return boost::make_shared<CommodityBasisFutureIndex>(underlyingName(), ed, fixingCalendar(), basisFec_, baseIndex_,
                                                         baseFec_, pts, addBasis_, monthOffset_, baseIsAveraging_);
}

boost::shared_ptr<QuantLib::CashFlow> CommodityBasisFutureIndex::baseCashflow(const QuantLib::Date& paymentDate) const {
    auto contractDate = Commodity::Utilities::getContractDate(expiryDate_, basisFec_);
    Date periodStart = contractDate - monthOffset_ * Months;
    Date periodEnd = (periodStart + 1 * Months) - 1 * Days;
    return Commodity::Utilities::makeCommodityCashflowForBasisFuture(periodStart, periodEnd, baseIndex_, baseFec_,
                                                                     baseIsAveraging_, paymentDate);
}

QuantLib::Real CommodityBasisFutureIndex::pastFixing(const QuantLib::Date& fixingDate) const {
    auto basisFixing = CommodityFuturesIndex::pastFixing(fixingDate);
    auto lambda = addBasis_ ? 1.0 : -1.0;
    auto baseValue = cashflow_->amount();
    return baseValue + lambda * basisFixing;
}
} // namespace QuantExt
