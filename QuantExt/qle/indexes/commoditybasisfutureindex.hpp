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

/*! \file qle/indexes/commoditybasisfutureindex.hpp
    \brief commodity basis future index class for holding price histories and forwarding.
    \ingroup indexes
*/

#pragma once

#include <ql/cashflow.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/termstructures/commoditybasispricecurve.hpp>

namespace QuantExt {

//! Commodity Basis Future Index
/*! This index can represent futures prices derived from basis future index and a base future index
    \ingroup indexes
*/
class CommodityBasisFutureIndex : public CommodityFuturesIndex {
public:
    CommodityBasisFutureIndex(const std::string& underlyingName, const QuantLib::Date& expiryDate,
                              const QuantLib::Calendar& fixingCalendar,
                              const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& basisFec,
                              const QuantLib::ext::shared_ptr<QuantExt::CommodityIndex>& baseIndex,
                              const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& baseFec,
                              const QuantLib::Handle<QuantExt::PriceTermStructure>& priceCurve =
                                  QuantLib::Handle<QuantExt::PriceTermStructure>(),
                              const bool addBasis = true, const QuantLib::Size monthOffset = 0,
                              const bool baseIsAveraging = false, const bool priceAsHistoricalFixing = true);

    CommodityBasisFutureIndex(const std::string& underlyingName, const QuantLib::Date& expiryDate,
                              const QuantLib::Calendar& fixingCalendar,
                              const QuantLib::ext::shared_ptr<CommodityBasisPriceTermStructure>& priceCurve);

    //! Implement the base clone. Ajust the base future to match the same contract month
    QuantLib::ext::shared_ptr<CommodityIndex>
    clone(const QuantLib::Date& expiryDate = QuantLib::Date(),
          const boost::optional<QuantLib::Handle<PriceTermStructure>>& ts = boost::none) const override;

    QuantLib::Real pastFixing(const QuantLib::Date& fixingDate) const override;

    const QuantLib::ext::shared_ptr<QuantExt::CommodityIndex>& baseIndex() { return baseIndex_; }
    QuantLib::ext::shared_ptr<QuantLib::CashFlow> baseCashflow(const QuantLib::Date& paymentDate = QuantLib::Date()) const;

private:
    QuantLib::ext::shared_ptr<FutureExpiryCalculator> basisFec_;
    QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> baseIndex_;
    QuantLib::ext::shared_ptr<FutureExpiryCalculator> baseFec_;
    bool addBasis_;
    QuantLib::Size monthOffset_;
    bool baseIsAveraging_;
    bool priceAsHistoricalFixing_;
    QuantLib::ext::shared_ptr<QuantLib::CashFlow> cashflow_;
};

} // namespace QuantExt