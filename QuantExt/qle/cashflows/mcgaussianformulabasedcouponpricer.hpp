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

/*! \file qle/cashflows/mcgaussianformulabasedcouponpricer.hpp
    \brief formula based coupon pricer
    \ingroup cashflows
*/

#pragma once

#include <qle/cashflows/formulabasedcoupon.hpp>

#include <ql/math/matrixutilities/pseudosqrt.hpp>

#include <qle/termstructures/correlationtermstructure.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Formula based coupon pricer
/*! - we assume a generalised version of the model in QuantLib::LognormalCmsSpreadPricer
    - TODO possibly we want to add a cache similar to LognormalCmsSpreadPricer?
*/

class MCGaussianFormulaBasedCouponPricer : public FormulaBasedCouponPricer {
public:
    /*! ibor pricers and cms pricers must be given by (index) currency codes, fx vols must be given by index currency
      codes (if not equal to payment currency), these must all be vs. payment currency; correlations must be given by
      pairs of index names, resp. by a pair of an index name and the special string "FX" indicating the correlation
      of the index rate and the the FX rate "index ccy vs. payment currency" for the quanto adjustemnts;
      missing correlations in the given map are assumed to be zero; the coupon discount curve must be in payment ccy

      Warning: The given fx vol structures must return the atm vol if a strike = Null<Real>() is passed to them. */

    MCGaussianFormulaBasedCouponPricer(
        const std::string& paymentCurrencyCode,
        const std::map<std::string, QuantLib::ext::shared_ptr<IborCouponPricer>>& iborPricers,
        const std::map<std::string, QuantLib::ext::shared_ptr<CmsCouponPricer>>& cmsPricers,
        const std::map<std::string, Handle<BlackVolTermStructure>>& fxVolatilities,
        const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlation,
        const Handle<YieldTermStructure>& couponDiscountCurve, const Size samples = 10000, const Size seed = 42,
        const bool useSobol = true, SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::None);
    /* */
    virtual Real swapletPrice() const override;
    virtual Rate swapletRate() const override;
    virtual Real capletPrice(Rate effectiveCap) const override;
    virtual Rate capletRate(Rate effectiveCap) const override;
    virtual Real floorletPrice(Rate effectiveFloor) const override;
    virtual Rate floorletRate(Rate effectiveFloor) const override;
    /* */

private:
    void initialize(const FloatingRateCoupon& coupon) override;
    //
    void compute() const;
    //
    const std::map<std::string, QuantLib::ext::shared_ptr<IborCouponPricer>> iborPricers_;
    const std::map<std::string, QuantLib::ext::shared_ptr<CmsCouponPricer>> cmsPricers_;
    const Handle<YieldTermStructure> couponDiscountCurve_;
    const Size samples_, seed_;
    const bool useSobol_;
    const SalvagingAlgorithm::Type salvaging_;
    //
    Size n_;
    Date today_, fixingDate_, paymentDate_;
    Real discount_;
    QuantLib::ext::shared_ptr<FormulaBasedIndex> index_;
    const FormulaBasedCoupon* coupon_;
    //
    std::vector<VolatilityType> volType_;
    std::vector<Real> volShift_, atmRate_;
    Array mean_;
    Matrix covariance_;
    //
    mutable Real rateEstimate_;
};

} // namespace QuantExt
