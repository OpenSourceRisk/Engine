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

/*! \file qle/pricingengines/blackswaptionenginedeltagamma.hpp
    \brief Swaption engine providing analytical deltas for vanilla swaps

    \ingroup engines
*/

#ifndef quantext_pricers_black_swaption_deltagamma_hpp
#define quantext_pricers_black_swaption_deltagamma_hpp

#include <qle/pricingengines/discountingswapenginedeltagamma.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/exercise.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <boost/make_shared.hpp>

namespace QuantLib {
class Quote;
}

namespace QuantExt {
using namespace QuantLib;
namespace detail {

/*! Generic Black-style-formula swaption engine
    This is the base class for the Black and Bachelier swaption engines
    See also discountingswapenginedeltagamma.hpp. The vega is rebucketed as well (linear in volatility), w.r.t
    the given bucketTimesVega, although this is only one number. The interest rate deltas are sticky strike deltas.

    The additional results of this engine are

    deltaDiscount    (vector<Real>           ): Delta on discount curve, rebucketed on time grid
    deltaForward     (vector<Real>           ): Delta on forward curve, rebucketed on time grid
    vega             (Matrix                 ): Vega, rebucketed on time grid (rows = opt, cols = und)

    gamma            (Matrix                 ): Gamma matrix with blocks | dsc-dsc dsc-fwd |
                                                                         | dsc-fwd fwd-fwd |

    theta            (Real                   ): Theta

    bucketTimesDeltaGamma    (vector<Real>   ): Bucketing grid for deltas and gammas
    bucketTimesVegaOpt       (vector<Real>   ): Bucketing grid for vega (option)
    bucketTimesVegaUnd       (vector<Real>   ): Bucketing grid for vega (underlying)

    \warning Cash settled swaption are priced, but the annuity used is the one from physical settlement currently
*/

template <class Spec> class BlackStyleSwaptionEngineDeltaGamma : public QuantLib::Swaption::engine {
public:
    BlackStyleSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve, Volatility vol,
                                       const DayCounter& dc = Actual365Fixed(), Real displacement = 0.0,
                                       const std::vector<Time>& bucketTimesDeltaGamma = std::vector<Time>(),
                                       const std::vector<Time>& bucketTimesVegaOpt = std::vector<Time>(),
                                       const std::vector<Time>& bucketTimesVegaUnd = std::vector<Time>(),
                                       const bool computeDeltaVega = false, const bool computeGamma = false,
                                       const bool linearInZero = true);
    BlackStyleSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve, const Handle<Quote>& vol,
                                       const DayCounter& dc = Actual365Fixed(), Real displacement = 0.0,
                                       const std::vector<Time>& bucketTimesDeltaGamma = std::vector<Time>(),
                                       const std::vector<Time>& bucketTimesVegaOpt = std::vector<Time>(),
                                       const std::vector<Time>& bucketTimesVegaUnd = std::vector<Time>(),
                                       const bool computeDeltaVega = false, const bool computeGamma = false,
                                       const bool linearInZero = true);
    BlackStyleSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve,
                                       const Handle<SwaptionVolatilityStructure>& vol,
                                       const std::vector<Time>& bucketTimesDeltaGamma = std::vector<Time>(),
                                       const std::vector<Time>& bucketTimesVegaOpt = std::vector<Time>(),
                                       const std::vector<Time>& bucketTimesVegaUnd = std::vector<Time>(),
                                       const bool computeDeltaVega = false, const bool computeGamma = false,
                                       const bool linearInZero = true);
    void calculate() const override;
    Handle<YieldTermStructure> termStructure() { return discountCurve_; }
    Handle<SwaptionVolatilityStructure> volatility() { return vol_; }

private:
    Handle<YieldTermStructure> discountCurve_;
    Handle<SwaptionVolatilityStructure> vol_;
    const Real displacement_;
    const std::vector<Time> bucketTimesDeltaGamma_, bucketTimesVegaOpt_, bucketTimesVegaUnd_;
    const bool computeDeltaVega_, computeGamma_, linearInZero_;
};

// shifted lognormal type engine
struct Black76Spec {
    static const VolatilityType type = ShiftedLognormal;
    Real value(const Option::Type type, const Real strike, const Real atmForward, const Real stdDev, const Real annuity,
               const Real displacement) {
        return blackFormula(type, strike, atmForward, stdDev, annuity, displacement);
    }
    Real vega(const Real strike, const Real atmForward, const Real stdDev, const Real exerciseTime, const Real annuity,
              const Real displacement) {
        return std::sqrt(exerciseTime) *
               blackFormulaStdDevDerivative(strike, atmForward, stdDev, annuity, displacement);
    }
    Real delta(const Option::Type type, const Real strike, const Real atmForward, const Real stdDev, const Real annuity,
               const Real displacement) {
        QuantLib::CumulativeNormalDistribution cnd;
        Real d1 = std::log((atmForward + displacement) / (strike + displacement)) / stdDev + 0.5 * stdDev;
        Real w = type == Option::Call ? 1.0 : -1.0;
        return annuity * w * cnd(w * d1);
    }
    Real gamma(const Real strike, const Real atmForward, const Real stdDev, const Real annuity,
               const Real displacement) {
        QuantLib::CumulativeNormalDistribution cnd;
        Real d1 = std::log((atmForward + displacement) / (strike + displacement)) / stdDev + 0.5 * stdDev;
        return annuity * cnd.derivative(d1) / ((atmForward + displacement) * stdDev);
    }
    Real theta(const Real strike, const Real atmForward, const Real stdDev, const Real exerciseTime, const Real annuity,
               const Real displacement) {
        QuantLib::CumulativeNormalDistribution cnd;
        Real d1 = std::log((atmForward + displacement) / (strike + displacement)) / stdDev + 0.5 * stdDev;
        return -0.5 * annuity * cnd.derivative(d1) * (atmForward + displacement) * stdDev / exerciseTime;
    }
};

// normal type engine
struct BachelierSpec {
    static const VolatilityType type = Normal;
    Real value(const Option::Type type, const Real strike, const Real atmForward, const Real stdDev, const Real annuity,
               const Real) {
        return bachelierBlackFormula(type, strike, atmForward, stdDev, annuity);
    }
    Real vega(const Real strike, const Real atmForward, const Real stdDev, const Real exerciseTime, const Real annuity,
              const Real) {
        return std::sqrt(exerciseTime) * bachelierBlackFormulaStdDevDerivative(strike, atmForward, stdDev, annuity);
    }
    Real delta(const Option::Type type, const Real strike, const Real atmForward, const Real stdDev, const Real annuity,
               const Real displacement) {
        QuantLib::CumulativeNormalDistribution cnd;
        Real d1 = (atmForward - strike) / stdDev;
        Real w = type == Option::Call ? 1.0 : -1.0;
        return annuity * w * cnd(w * d1);
    }
    Real gamma(const Real strike, const Real atmForward, const Real stdDev, const Real annuity,
               const Real displacement) {
        QuantLib::CumulativeNormalDistribution cnd;
        Real d1 = (atmForward - strike) / stdDev;
        return annuity * cnd.derivative(d1) / stdDev;
    }
    Real theta(const Real strike, const Real atmForward, const Real stdDev, const Real exerciseTime, const Real annuity,
               const Real displacement) {
        QuantLib::CumulativeNormalDistribution cnd;
        Real d1 = (atmForward - strike) / stdDev;
        return -0.5 * annuity * cnd.derivative(d1) * stdDev / exerciseTime;
    }
};

} // namespace detail

//! Shifted Lognormal Black-formula swaption engine
/*! \ingroup swaptionengines

    \warning The engine assumes that the exercise date equals the
             start date of the passed swap.
*/

class BlackSwaptionEngineDeltaGamma : public detail::BlackStyleSwaptionEngineDeltaGamma<detail::Black76Spec> {
public:
    BlackSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve, Volatility vol,
                                  const DayCounter& dc = Actual365Fixed(), Real displacement = 0.0,
                                  const std::vector<Time>& bucketTimesDeltaGamma = std::vector<Time>(),
                                  const std::vector<Time>& bucketTimesVegaOpt = std::vector<Time>(),
                                  const std::vector<Time>& bucketTimesVegaUnd = std::vector<Time>(),
                                  const bool computeDeltaVega = false, const bool computeGamma = false,
                                  const bool linearInZero = true);
    BlackSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve, const Handle<Quote>& vol,
                                  const DayCounter& dc = Actual365Fixed(), Real displacement = 0.0,
                                  const std::vector<Time>& bucketTimesDeltaGamma = std::vector<Time>(),
                                  const std::vector<Time>& bucketTimesVegaOpt = std::vector<Time>(),
                                  const std::vector<Time>& bucketTimesVegaUnd = std::vector<Time>(),
                                  const bool computeDeltaVega = false, const bool computeGamma = false,
                                  const bool linearInZero = true);
    BlackSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve,
                                  const Handle<SwaptionVolatilityStructure>& vol,
                                  const std::vector<Time>& bucketTimesDeltaGamma = std::vector<Time>(),
                                  const std::vector<Time>& bucketTimesVegaOpt = std::vector<Time>(),
                                  const std::vector<Time>& bucketTimesVegaUnd = std::vector<Time>(),
                                  const bool computeDeltaVega = false, const bool computeGamma = false,
                                  const bool linearInZero = true);
};

//! Normal Bachelier-formula swaption engine
/*! \ingroup swaptionengines

    \warning The engine assumes that the exercise date equals the
             start date of the passed swap.
*/

class BachelierSwaptionEngineDeltaGamma : public detail::BlackStyleSwaptionEngineDeltaGamma<detail::BachelierSpec> {
public:
    BachelierSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve, Volatility vol,
                                      const DayCounter& dc = Actual365Fixed(),
                                      const std::vector<Time>& bucketTimesDeltaGamma = std::vector<Time>(),
                                      const std::vector<Time>& bucketTimesVegaOpt = std::vector<Time>(),
                                      const std::vector<Time>& bucketTimesVegaUnd = std::vector<Time>(),
                                      const bool computeDeltaVega = false, const bool computeGamma = false,
                                      const bool linearInZero = true);
    BachelierSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve, const Handle<Quote>& vol,
                                      const DayCounter& dc = Actual365Fixed(),
                                      const std::vector<Time>& bucketTimesDeltaGamma = std::vector<Time>(),
                                      const std::vector<Time>& bucketTimesVegaOpt = std::vector<Time>(),
                                      const std::vector<Time>& bucketTimesVegaUnd = std::vector<Time>(),
                                      const bool computeDeltaVega = false, const bool computeGamma = false,
                                      const bool linearInZero = true);
    BachelierSwaptionEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve,
                                      const Handle<SwaptionVolatilityStructure>& vol,
                                      const std::vector<Time>& bucketTimesDeltaGamma = std::vector<Time>(),
                                      const std::vector<Time>& bucketTimesVegaOpt = std::vector<Time>(),
                                      const std::vector<Time>& bucketTimesVegaUnd = std::vector<Time>(),
                                      const bool computeDeltaVega = false, const bool computeGamma = false,
                                      const bool linearInZero = true);
};

// implementation

namespace detail {

template <class Spec>
BlackStyleSwaptionEngineDeltaGamma<Spec>::BlackStyleSwaptionEngineDeltaGamma(
    const Handle<YieldTermStructure>& discountCurve, Volatility vol, const DayCounter& dc, Real displacement,
    const std::vector<Time>& bucketTimesDeltaGamma, const std::vector<Time>& bucketTimesVegaOpt,
    const std::vector<Time>& bucketTimesVegaUnd, const bool computeDeltaVega, const bool computeGamma,
    const bool linearInZero)
    : discountCurve_(discountCurve), vol_(QuantLib::ext::shared_ptr<SwaptionVolatilityStructure>(new ConstantSwaptionVolatility(
                                         0, NullCalendar(), Following, vol, dc, Spec().type, displacement))),
      displacement_(displacement), bucketTimesDeltaGamma_(bucketTimesDeltaGamma),
      bucketTimesVegaOpt_(bucketTimesVegaOpt), bucketTimesVegaUnd_(bucketTimesVegaUnd),
      computeDeltaVega_(computeDeltaVega), computeGamma_(computeGamma), linearInZero_(linearInZero) {
    registerWith(discountCurve_);
    QL_REQUIRE((!bucketTimesDeltaGamma_.empty() && !bucketTimesVegaOpt_.empty() && !bucketTimesVegaUnd_.empty()) ||
                   (!computeDeltaVega && !computeGamma),
               "bucket times are empty, although sensitivities have to be calculated");
}

template <class Spec>
BlackStyleSwaptionEngineDeltaGamma<Spec>::BlackStyleSwaptionEngineDeltaGamma(
    const Handle<YieldTermStructure>& discountCurve, const Handle<Quote>& vol, const DayCounter& dc, Real displacement,
    const std::vector<Time>& bucketTimesDeltaGamma, const std::vector<Time>& bucketTimesVegaOpt,
    const std::vector<Time>& bucketTimesVegaUnd, const bool computeDeltaVega, const bool computeGamma,
    const bool linearInZero)
    : discountCurve_(discountCurve), vol_(QuantLib::ext::shared_ptr<SwaptionVolatilityStructure>(new ConstantSwaptionVolatility(
                                         0, NullCalendar(), Following, vol, dc, Spec().type, displacement))),
      displacement_(displacement), bucketTimesDeltaGamma_(bucketTimesDeltaGamma),
      bucketTimesVegaOpt_(bucketTimesVegaOpt), bucketTimesVegaUnd_(bucketTimesVegaUnd),
      computeDeltaVega_(computeDeltaVega), computeGamma_(computeGamma), linearInZero_(linearInZero) {
    registerWith(discountCurve_);
    registerWith(vol_);
    QL_REQUIRE((!bucketTimesDeltaGamma_.empty() && !bucketTimesVegaOpt_.empty() && !bucketTimesVegaUnd_.empty()) ||
                   (!computeDeltaVega && !computeGamma),
               "bucket times are empty, although sensitivities have to be calculated");
}

template <class Spec>
BlackStyleSwaptionEngineDeltaGamma<Spec>::BlackStyleSwaptionEngineDeltaGamma(
    const Handle<YieldTermStructure>& discountCurve, const Handle<SwaptionVolatilityStructure>& volatility,
    const std::vector<Time>& bucketTimesDeltaGamma, const std::vector<Time>& bucketTimesVegaOpt,
    const std::vector<Time>& bucketTimesVegaUnd, const bool computeDeltaVega, const bool computeGamma,
    const bool linearInZero)
    : discountCurve_(discountCurve), vol_(volatility), displacement_(0.0),
      bucketTimesDeltaGamma_(bucketTimesDeltaGamma), bucketTimesVegaOpt_(bucketTimesVegaOpt),
      bucketTimesVegaUnd_(bucketTimesVegaUnd), computeDeltaVega_(computeDeltaVega), computeGamma_(computeGamma),
      linearInZero_(linearInZero) {
    registerWith(discountCurve_);
    registerWith(vol_);
    QL_REQUIRE((!bucketTimesDeltaGamma_.empty() && !bucketTimesVegaOpt_.empty() && !bucketTimesVegaUnd_.empty()) ||
                   (!computeDeltaVega && !computeGamma),
               "bucket times are empty, although sensitivities have to be calculated");
}

template <class Spec> void BlackStyleSwaptionEngineDeltaGamma<Spec>::calculate() const {
    Date exerciseDate = arguments_.exercise->date(0);
    VanillaSwap swap = *arguments_.swap;
    Rate strike = swap.fixedRate();

    std::vector<Leg> floatLeg(1, swap.leg(1)), fixedLeg(1, swap.leg(0));
    std::vector<bool> payerFloat(1, false), payerFixed(1, false);
    QuantLib::Swap swapFloatLeg(floatLeg, payerFloat), swapFixedLeg(fixedLeg, payerFixed);

    QuantLib::ext::shared_ptr<PricingEngine> engine = QuantLib::ext::make_shared<DiscountingSwapEngine>(discountCurve_);
    QuantLib::ext::shared_ptr<PricingEngine> engine1 = QuantLib::ext::make_shared<DiscountingSwapEngineDeltaGamma>(
        discountCurve_, bucketTimesDeltaGamma_, computeDeltaVega_, computeGamma_, false, linearInZero_);
    QuantLib::ext::shared_ptr<PricingEngine> engine2 = QuantLib::ext::make_shared<DiscountingSwapEngineDeltaGamma>(
        discountCurve_, bucketTimesDeltaGamma_, computeDeltaVega_, computeGamma_, true, linearInZero_);

    swap.setPricingEngine(engine);
    swapFloatLeg.setPricingEngine(engine1);
    swapFixedLeg.setPricingEngine(engine2);

    Rate atmForward = swapFloatLeg.NPV() / swapFixedLeg.legBPS(0);

    // If we allow for non-zero spreads, more adjustments are needed than below, investigate this later
    QL_REQUIRE(QuantLib::close_enough(swap.spread(), 0.0), "BlackSwaptionEngineDeltaGamma requires zero spread");

    // Volatilities are quoted for zero-spreaded swaps.
    // Therefore, any spread on the floating leg must be removed
    // with a corresponding correction on the fixed leg.
    // if (swap.spread() != 0.0) {
    //     Spread correction = swap.spread() * std::fabs(swap.floatingLegBPS() / swap.fixedLegBPS());
    //     strike -= correction;
    //     atmForward -= correction;
    //     results_.additionalResults["spreadCorrection"] = correction;
    // } else {
    //     results_.additionalResults["spreadCorrection"] = 0.0;
    // }

    results_.additionalResults["strike"] = strike;
    results_.additionalResults["atmForward"] = atmForward;

    swap.setPricingEngine(QuantLib::ext::shared_ptr<PricingEngine>(new DiscountingSwapEngine(discountCurve_, false)));

    // TODO this is for physical settlement only, add pricing + sensitivities for cash settlement
    Real annuity = std::fabs(swap.fixedLegBPS()) / 1.0E-4;
    results_.additionalResults["annuity"] = annuity;

    Time swapLength = vol_->swapLength(swap.floatingSchedule().dates().front(), swap.floatingSchedule().dates().back());
    results_.additionalResults["swapLength"] = swapLength;

    Real variance = vol_->blackVariance(exerciseDate, swapLength, strike);
    Real stdDev = std::sqrt(variance);
    results_.additionalResults["stdDev"] = stdDev;
    Option::Type w = (arguments_.type == VanillaSwap::Payer) ? Option::Call : Option::Put;
    results_.value = Spec().value(w, strike, atmForward, stdDev, annuity, displacement_);

    // sensitivity calculation
    QL_REQUIRE(!computeGamma_ || computeDeltaVega_,
               "BlackSwaptionEngineDeltaGamma, gamma can only be computed if delta is computed as well");
    if (computeDeltaVega_) {
        Time exerciseTime = vol_->timeFromReference(exerciseDate);
        // vega
        int n1 = bucketTimesVegaOpt_.size();
        int n2 = bucketTimesVegaUnd_.size();
        int b1 =
            static_cast<int>(std::upper_bound(bucketTimesVegaOpt_.begin(), bucketTimesVegaOpt_.end(), exerciseTime) -
                             bucketTimesVegaOpt_.begin());
        int b2 = static_cast<int>(std::upper_bound(bucketTimesVegaUnd_.begin(), bucketTimesVegaUnd_.end(), swapLength) -
                                  bucketTimesVegaUnd_.begin());
        Real w1, w2;
        int i1, i2;
        if (b1 == 0) {
            w1 = 1.0;
            i1 = 0;
        } else if (b1 == n1) {
            w1 = 0.0;
            i1 = n1 - 2;
        } else {
            w1 = (bucketTimesVegaOpt_[b1] - exerciseTime) / (bucketTimesVegaOpt_[b1] - bucketTimesVegaOpt_[b1 - 1]);
            i1 = b1 - 1;
        }
        if (b2 == 0) {
            w2 = 1.0;
            i2 = 0;
        } else if (b2 == n2) {
            w2 = 0.0;
            i2 = n2 - 2;
        } else {
            w2 = (bucketTimesVegaUnd_[b2] - swapLength) / (bucketTimesVegaUnd_[b2] - bucketTimesVegaUnd_[b2 - 1]);
            i2 = b2 - 1;
        }
        Real singleVega = Spec().vega(strike, atmForward, stdDev, exerciseTime, annuity, displacement_);
        Matrix vega(n1, n2, 0.0);
        if (i1 >= 0) {
            if (i2 < n2)
                vega[i1][i2 + 1] = w1 * (1.0 - w2) * singleVega;
            if (i2 >= 0) {
                vega[i1][i2] = w1 * w2 * singleVega;
            }
        }
        if (i2 >= 0 && i1 < n1) {
            vega[i1 + 1][i2] = (1.0 - w1) * w2 * singleVega;
        }
        if (i1 < n1 && i2 < n2)
            vega[i1 + 1][i2 + 1] = (1.0 - w1) * (1.0 - w2) * singleVega;
        results_.additionalResults["vega"] = vega;
        // delta
        Real mu = swapFloatLeg.NPV();
        Real black = results_.value / annuity;
        Real blackDelta = Spec().delta(w, strike, atmForward, stdDev, 1.0, displacement_);
        std::vector<Real> A_s_1 = swapFixedLeg.result<std::vector<std::vector<Real>>>("deltaBPS")[0];
        Array A_s(A_s_1.begin(), A_s_1.end());
        std::vector<Real> mu_sd_1 = swapFloatLeg.result<std::vector<Real>>("deltaDiscount");
        std::vector<Real> mu_sf_1 = swapFloatLeg.result<std::vector<Real>>("deltaForward");
        Array mu_sd(mu_sd_1.begin(), mu_sd_1.end());
        Array mu_sf(mu_sf_1.begin(), mu_sf_1.end());
        Array F_sd = 1.0 / annuity * mu_sd - 1.0 / (annuity * annuity) * mu * A_s;
        Array F_sf = 1.0 / annuity * mu_sf;
        Array deltaDiscount = black * A_s + annuity * blackDelta * F_sd;
        Array deltaForward = annuity * blackDelta * F_sf;
        std::vector<Real> deltaDiscount1(deltaDiscount.begin(), deltaDiscount.end());
        std::vector<Real> deltaForward1(deltaForward.begin(), deltaForward.end());
        results_.additionalResults["deltaDiscount"] = deltaDiscount1;
        results_.additionalResults["deltaForward"] = deltaForward1;
        // theta
        results_.additionalResults["theta"] =
            annuity * Spec().theta(strike, atmForward, stdDev, exerciseTime, 1.0, displacement_);
        if (computeGamma_) {
            // gamma
            Size n = bucketTimesDeltaGamma_.size();
            Real blackGamma = Spec().gamma(strike, atmForward, stdDev, 1.0, displacement_);
            Matrix A_ss = swapFixedLeg.result<std::vector<Matrix>>("gammaBPS")[0];
            Matrix mu_ss = swapFloatLeg.result<Matrix>("gamma");
            Matrix result(2 * n, 2 * n, 0.0);
            for (Size i = 0; i < 2 * n; ++i) {
                bool iIsDsc = i < n;
                Real Fi = iIsDsc ? (mu_sd[i] / annuity - mu * A_s[i] / (annuity * annuity)) : (mu_sf[i - n] / annuity);
                for (Size j = 0; j <= i; ++j) {
                    bool jIsDsc = j < n;
                    Real Fj =
                        jIsDsc ? (mu_sd[j] / annuity - mu * A_s[j] / (annuity * annuity)) : (mu_sf[j - n] / annuity);
                    Real Fij = mu_ss[i][j] / annuity;
                    if (iIsDsc) {
                        if (jIsDsc) {
                            Fij += -mu * A_ss[i][j] / (annuity * annuity);
                            result[i][j] += A_ss[i][j] * black;
                        }
                        Fij -= (jIsDsc ? mu_sd[j] : mu_sf[j - n]) * A_s[i] / (annuity * annuity);
                        result[i][j] += A_s[i] * blackDelta * Fj;
                    }
                    if (jIsDsc) {
                        Fij += (iIsDsc ? mu_sd[i] : mu_sf[i - n]) * A_s[j] / (annuity * annuity);
                        Fij -= 2.0 * A_s[j] * (iIsDsc ? mu_sd[i] : mu_sf[i - n]) / (annuity * annuity);
                        if (iIsDsc) {
                            Fij += 2.0 * A_s[j] * mu * A_s[i] / (annuity * annuity * annuity);
                        }
                        result[i][j] += A_s[j] * blackDelta * Fi;
                    }
                    result[i][j] += annuity * (blackGamma * Fi * Fj + blackDelta * Fij);
                }
            }
            // mirror
            for (Size i = 0; i < 2 * n; ++i) {
                for (Size j = i + 1; j < 2 * n; ++j) {
                    result[i][j] = result[j][i];
                }
            }
            results_.additionalResults["gamma"] = result;
        }
    }

} // calculate()

} // namespace detail
} // namespace QuantExt

#endif
