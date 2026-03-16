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

/*! \file qle/pricingengines/discountingswapenginedeltagamma.hpp
    \brief Swap engine providing analytical deltas and gammas for vanilla swaps

    \ingroup engines
*/

#ifndef quantext_discounting_swap_engine_deltagamma_hpp
#define quantext_discounting_swap_engine_deltagamma_hpp

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/handle.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/math/matrix.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>

#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Discounting swap engine providing analytical deltas and gammas
/*! The provided deltas and gammas are (continuously compounded) zero yield deltas assuming linear in zero or
  loglinear in discount factor interpolation on the discounting and forwarding curves with flat extrapolation
  of the zero rate before the first and last bucket time. The deltas are provided as additional results

  deltaDiscount    (vector<Real>           ): Delta on discount curve, rebucketed on time grid
  deltaForward     (vector<Real>           ): Delta on forward curve, rebucketed on time grid
  deltaBPS         (vector<vector<Real>>   ): Delta of BPS (on discount curve, per leg)

  The gammas, likewise,

  gamma            (Matrix                    ): Gamma matrix with blocks | dsc-dsc dsc-fwd |
                                                                          | dsc-fwd fwd-fwd |
  gammaBps         (vector<Matrix>            ): Gamma of BPS (on dsc, per leg)

  bucketTimes      (vector<Real>   ): Bucketing grid for deltas and gammas

  \warning Deltas and gammas are produced only for fixed and Ibor coupons without caps or floors,
  for Ibor coupons they are ignoring convexity adjustments (like in arrears adjustments). It is possible
  to have different Ibor coupons (with different forward curves) on a leg, but the computed deltas would
  be aggregated over all underlying curves then.

  \warning Derivatives are not w.r.t. basispoints, but w.r.t. the usual unit
  \warning BPS is the value of one unit (not one basispoint actually), it has to be divided by 10000.0 to get QL's BPS
*/

class DiscountingSwapEngineDeltaGamma : public QuantLib::Swap::engine {
public:
    DiscountingSwapEngineDeltaGamma(const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
                                    const std::vector<Time>& bucketTimes = std::vector<Time>(),
                                    const bool computeDelta = false, const bool computeGamma = false,
                                    const bool computeBPS = false, const bool linearInZero = true);
    void calculate() const override;
    Handle<YieldTermStructure> discountCurve() const { return discountCurve_; }

private:
    Handle<YieldTermStructure> discountCurve_;
    const std::vector<Real> bucketTimes_;
    const bool computeDelta_, computeGamma_, computeBPS_, linearInZero_;
};

namespace detail {

class NpvDeltaGammaCalculator : public AcyclicVisitor,
                                public Visitor<CashFlow>,
                                public Visitor<SimpleCashFlow>,
                                public Visitor<FixedRateCoupon>,
                                public Visitor<IborCoupon>,
                                public Visitor<QuantExt::OvernightIndexedCoupon>,
                                public Visitor<FloatingRateFXLinkedNotionalCoupon>,
                                public Visitor<FXLinkedCashFlow> {
public:
    // if excludeSimpleCashFlowsFromSensis = true, SimpleCashFlow's are excluded from all results, and their npv
    // is collected in simpleCashFlowNpv instead
    NpvDeltaGammaCalculator(Handle<YieldTermStructure> discountCurve, const Real payer, Real& npv, Real& bps,
                            const bool computeDelta, const bool computeGamma, const bool computeBPS,
                            std::map<Date, Real>& deltaDiscount, std::map<Date, Real>& deltaForward,
                            std::map<Date, Real>& deltaBPS, std::map<Date, Real>& gammaDiscount,
                            std::map<std::pair<Date, Date>, Real>& gammaForward,
                            std::map<std::pair<Date, Date>, Real>& gammaDscFwd, std::map<Date, Real>& gammaBPS,
                            Real& fxLinkedForeignNpv, const bool excludeSimpleCashFlowsFromSensis,
                            Real& simpleCashFlowNpv);
    void visit(CashFlow& c) override;
    void visit(SimpleCashFlow& c) override;
    void visit(FixedRateCoupon& c) override;
    void visit(IborCoupon& c) override;
    void visit(FloatingRateFXLinkedNotionalCoupon& c) override;
    void visit(FXLinkedCashFlow& c) override;
    void visit(QuantExt::OvernightIndexedCoupon& c) override;

private:
    void processIborCoupon(FloatingRateCoupon& c);

    Handle<YieldTermStructure> discountCurve_;
    const Real payer_;
    Real &npv_, &bps_;
    const bool computeDelta_, computeGamma_, computeBPS_;
    std::map<Date, Real>&deltaDiscount_, &deltaForward_, &deltaBPS_, &gammaDiscount_;
    std::map<std::pair<Date, Date>, Real>&gammaForward_, &gammaDscFwd_;
    std::map<Date, Real>& gammaBPS_;
    Real& fxLinkedForeignNpv_;
    const bool excludeSimpleCashFlowsFromSensis_;
    Real& simpleCashFlowNpv_;
};

std::vector<Real> rebucketDeltas(const std::vector<Time>& deltaTimes, const std::map<Date, Real>& deltaRaw,
				 const Date& referenceDate, const DayCounter& dc, const bool linearInZero);

Matrix rebucketGammas(const std::vector<Time>& gammaTimes, const std::map<Date, Real>& gammaDscRaw,
		      std::map<std::pair<Date, Date>, Real>& gammaForward,
		      std::map<std::pair<Date, Date>, Real>& gammaDscFwd, const bool forceFullMatrix,
		      const Date& referenceDate, const DayCounter& dc, const bool linearInZero);

} // namespace detail

} // namespace QuantExt

#endif
