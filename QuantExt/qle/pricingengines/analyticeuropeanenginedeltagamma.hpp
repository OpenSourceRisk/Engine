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

/*! \file qle/pricingengines/analyticeuropeanenginedeltagamma.hpp
    \brief Analytic European engine providing sensitivities
*/

#ifndef quantext_analytic_european_engine_deltagamma_hpp
#define quantext_analytic_european_engine_deltagamma_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Pricing engine for European vanilla options using analytical formulae
/*! The additional results of this engine are

    deltaSpot     (Real          ): Delta w.r.t. spot
    gammaSpot     (Real          ): Gamma w.r.t. spot
    vega          (vector<Real>  ): Bucketed vega

    deltaRate     (vector<Real>  ): Bucketed delta on risk free curve
    deltaDividend (vector<Real>  ): Bucketed delta on dividend curve
    gamma         (Matrix        ): Gamma matrix with blocks | rate-rate rate-div |
                                                             | rate-dic  div-div  |
    gammaSpotRate (vecor<Real>   ): Mixed derivatives w.r.t. spot and rate
    gammaSpotDiv  (vecor<Real>   ): Mixed derivatives w.r.t. spot and div

    theta         (Real          ): Theta (TODO...)

    bucketTimesDeltaGamma (vector<Real>  ): Bucketing grid for rate and dividend deltas and gammas
    bucketTimesVega       (vector<Real>  ): Bucketing grid for vega
*/

class AnalyticEuropeanEngineDeltaGamma : public VanillaOption::engine {
public:
    AnalyticEuropeanEngineDeltaGamma(const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
                                     const std::vector<Time>& bucketTimeDeltaGamma = std::vector<Time>(),
                                     const std::vector<Time>& bucketTimesVega = std::vector<Time>(),
                                     const bool computeDeltaVega = false, const bool computeGamma = false,
                                     const bool linearInZero = true);
    void calculate() const override;

private:
    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
    const std::vector<Time> bucketTimesDeltaGamma_, bucketTimesVega_;
    const bool computeDeltaVega_, computeGamma_, linearInZero_;
};
} // namespace QuantExt

#endif
