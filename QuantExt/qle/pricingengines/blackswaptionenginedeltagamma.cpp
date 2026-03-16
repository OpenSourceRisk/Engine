/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/blackswaptionenginedeltagamma.hpp>

namespace QuantExt {

BlackSwaptionEngineDeltaGamma::BlackSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve,
                                                             Volatility vol, const DayCounter& dc, Real displacement,
                                                             const std::vector<Time>& bucketTimesDeltaGamma,
                                                             const std::vector<Time>& bucketTimesVegaOpt,
                                                             const std::vector<Time>& bucketTimesVegaUnd,
                                                             const bool computeDeltaVega, const bool computeGamma,
                                                             const bool linearInZero)
    : detail::BlackStyleSwaptionEngineDeltaGamma<detail::Black76Spec>(
          discountCurve, vol, dc, displacement, bucketTimesDeltaGamma, bucketTimesVegaOpt, bucketTimesVegaUnd,
          computeDeltaVega, computeGamma, linearInZero) {}

BlackSwaptionEngineDeltaGamma::BlackSwaptionEngineDeltaGamma(
    const Handle<YieldTermStructure>& discountCurve, const Handle<Quote>& vol, const DayCounter& dc, Real displacement,
    const std::vector<Time>& bucketTimesDeltaGamma, const std::vector<Time>& bucketTimesVegaOpt,
    const std::vector<Time>& bucketTimesVegaUnd, const bool computeDeltaVega, const bool computeGamma,
    const bool linearInZero)
    : detail::BlackStyleSwaptionEngineDeltaGamma<detail::Black76Spec>(
          discountCurve, vol, dc, displacement, bucketTimesDeltaGamma, bucketTimesVegaOpt, bucketTimesVegaUnd,
          computeDeltaVega, computeGamma, linearInZero) {}

BlackSwaptionEngineDeltaGamma::BlackSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve,
                                                             const Handle<SwaptionVolatilityStructure>& vol,
                                                             const std::vector<Time>& bucketTimesDeltaGamma,
                                                             const std::vector<Time>& bucketTimesVegaOpt,
                                                             const std::vector<Time>& bucketTimesVegaUnd,
                                                             const bool computeDeltaVega, const bool computeGamma,
                                                             const bool linearInZero)
    : detail::BlackStyleSwaptionEngineDeltaGamma<detail::Black76Spec>(discountCurve, vol, bucketTimesDeltaGamma,
                                                                      bucketTimesVegaOpt, bucketTimesVegaUnd,
                                                                      computeDeltaVega, computeGamma, linearInZero) {
    QL_REQUIRE(vol->volatilityType() == ShiftedLognormal,
               "BlackSwaptionEngineDeltaGamma requires (shifted) lognormal input "
               "volatility");
}

BachelierSwaptionEngineDeltaGamma::BachelierSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve,
                                                                     Volatility vol, const DayCounter& dc,
                                                                     const std::vector<Time>& bucketTimesDeltaGamma,
                                                                     const std::vector<Time>& bucketTimesVegaOpt,
                                                                     const std::vector<Time>& bucketTimesVegaUnd,
                                                                     const bool computeDeltaVega,
                                                                     const bool computeGamma, const bool linearInZero)
    : detail::BlackStyleSwaptionEngineDeltaGamma<detail::BachelierSpec>(
          discountCurve, vol, dc, 0.0, bucketTimesDeltaGamma, bucketTimesVegaOpt, bucketTimesVegaUnd, computeDeltaVega,
          computeGamma, linearInZero) {}

BachelierSwaptionEngineDeltaGamma::BachelierSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve,
                                                                     const Handle<Quote>& vol, const DayCounter& dc,
                                                                     const std::vector<Time>& bucketTimesDeltaGamma,
                                                                     const std::vector<Time>& bucketTimesVegaOpt,
                                                                     const std::vector<Time>& bucketTimesVegaUnd,
                                                                     const bool computeDeltaVega,
                                                                     const bool computeGamma, const bool linearInZero)
    : detail::BlackStyleSwaptionEngineDeltaGamma<detail::BachelierSpec>(
          discountCurve, vol, dc, 0.0, bucketTimesDeltaGamma, bucketTimesVegaOpt, bucketTimesVegaUnd, computeDeltaVega,
          computeGamma, linearInZero) {}

BachelierSwaptionEngineDeltaGamma::BachelierSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve,
                                                                     const Handle<SwaptionVolatilityStructure>& vol,
                                                                     const std::vector<Time>& bucketTimesDeltaGamma,
                                                                     const std::vector<Time>& bucketTimesVegaOpt,
                                                                     const std::vector<Time>& bucketTimesVegaUnd,
                                                                     const bool computeDeltaVega,
                                                                     const bool computeGamma, const bool linearInZero)
    : detail::BlackStyleSwaptionEngineDeltaGamma<detail::BachelierSpec>(discountCurve, vol, bucketTimesDeltaGamma,
                                                                        bucketTimesVegaOpt, bucketTimesVegaUnd,
                                                                        computeDeltaVega, computeGamma, linearInZero) {
    QL_REQUIRE(vol->volatilityType() == Normal, "BachelierSwaptionEngineDeltaGamma requires normal input volatility");
}

} // namespace QuantExt
