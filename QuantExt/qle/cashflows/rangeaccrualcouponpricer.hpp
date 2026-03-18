/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file qle/cashflows/rangeaccrualcouponpricer.hpp
    \brief Range accrual coupon pricer using call-spread replication on optionlet vols
    \ingroup cashflows

    Prices the range accrual digital condition for each daily observation as a
    call-spread on vanilla caplet/floorlet prices:
        Digital(K) ≈ [C(K − ε/2, σ−) − C(K + ε/2, σ+)] / ε

    Uses the market optionlet (capFloor) volatility surface, supporting both
    Black (shifted-lognormal) and Bachelier (normal) vol types.  This is the
    same model-free replication approach used by RateDigitalCallSpreadEngine
    for the RateDigitalOption trade type.
*/

#ifndef quantext_rangeaccrualcouponpricer_hpp
#define quantext_rangeaccrualcouponpricer_hpp

#include <ql/cashflows/rangeaccrual.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

namespace QuantExt {

//! Range accrual coupon pricer using call-spread digital replication on optionlet vols.
/*! For each daily observation the range indicator 1_{L <= fixing <= U} is priced as:
        Digital_call(L) - Digital_call(U)
    where each digital is replicated via a call-spread:
        Digital_call(K) = [C(K-eps/2) - C(K+eps/2)] / eps

    The vanilla call prices C(K,sigma(K)) use Black or Bachelier formula
    depending on the optionlet volatility surface type.

    Supports both fixed-rate mode (fixedRate * n/N * tau * D) and floating mode
    (gearing * L0 * n/N * tau * D + spread).

    \ingroup cashflows
*/
class RangeAccrualPricerByCallSpread : public QuantLib::RangeAccrualPricer {
public:
    /*! \param ovs         Optionlet (capFloor) volatility surface
        \param eps         Width of the call-spread (in rate terms, default 1bp)
    */
    RangeAccrualPricerByCallSpread(QuantLib::Handle<QuantLib::OptionletVolatilityStructure> ovs,
                                   QuantLib::Real eps = 1.0e-4);

    QuantLib::Real swapletPrice() const override;

private:
    //! Price a single digital call: Pr(fixing > strike) * deflator
    QuantLib::Real digitalPrice(QuantLib::Real strike, QuantLib::Real forward,
                                QuantLib::Real fixingDate, QuantLib::Real deflator) const;

    //! Price the range indicator: Pr(lower <= fixing <= upper) * deflator
    QuantLib::Real digitalRangePrice(QuantLib::Real lower, QuantLib::Real upper,
                                     QuantLib::Real forward, QuantLib::Real fixingDate,
                                     QuantLib::Real deflator) const;

    QuantLib::Handle<QuantLib::OptionletVolatilityStructure> ovs_;
    QuantLib::Real eps_;
};

} // namespace QuantExt

#endif
