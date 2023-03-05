/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file midpointindexcdsengine.hpp
    \brief Mid-point engine for credit default swaps
*/

#ifndef quantext_mid_point_index_cds_engine_hpp
#define quantext_mid_point_index_cds_engine_hpp

#include <qle/instruments/indexcreditdefaultswap.hpp>

#include <ql/pricingengines/credit/midpointcdsengine.hpp>

namespace QuantExt {
using namespace QuantLib;

class MidPointIndexCdsEngine : public IndexCreditDefaultSwap::engine, public MidPointCdsEngineBase {
public:
    // use index curve
    MidPointIndexCdsEngine(const Handle<DefaultProbabilityTermStructure>&, Real recoveryRate,
                           const Handle<YieldTermStructure>& discountCurve,
                           boost::optional<bool> includeSettlementDateFlows = boost::none);
    // use underlying curves
    MidPointIndexCdsEngine(const std::vector<Handle<DefaultProbabilityTermStructure>>&,
                           const std::vector<Real>& underlyingRecoveryRate,
                           const Handle<YieldTermStructure>& discountCurve,
                           boost::optional<bool> includeSettlementDateFlows = boost::none);
    void calculate() const override;

private:
    Real survivalProbability(const Date& d) const override;
    Real defaultProbability(const Date& d1, const Date& d2) const override;
    Real expectedLoss(const Date& defaultDate, const Date& d1, const Date& d2, const Real notional) const override;

    Handle<DefaultProbabilityTermStructure> probability_;
    Real recoveryRate_;

    const std::vector<Handle<DefaultProbabilityTermStructure>> underlyingProbability_;
    const std::vector<Real> underlyingRecoveryRate_;

    const bool useUnderlyingCurves_;
};
} // namespace QuantExt

#endif
