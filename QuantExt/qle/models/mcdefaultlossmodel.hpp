/*
 Copyright (C) 2024 AcadiaSoft Inc.
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

#pragma once

#include <qle/models/defaultlossmodel.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/math/randomnumbers/inversecumulativerng.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
/* Intended to replace LossDistribution in
    ql/experimental/credit/lossdistribution, not sure its covering all the
    functionality (see mthod below)
*/

namespace QuantExt {
using namespace QuantLib;

/*! Model Class for 1 factor gaussian copula loss model 
using monte carlo simulation
*/
class GaussianOneFactorMonteCarloLossModel : public DefaultLossModel  {

public:
    GaussianOneFactorMonteCarloLossModel(double baseCorrelation, const std::vector<std::vector<double>>& recoveryRates,
                                         const std::vector<double>& recoveryProbabilites, )
        : baseCorrelation_(baseCorrelation), recoveryRates_(recoveryRates),
          recoveryProbabilities_(recoveryProbabilites), rng_() {}

protected:
    Real expectedTrancheLoss(const Date& d, Real recoveryRate = Null<Real>()) const override{
        // Check cache first
        auto it = expectedTrancheLoss_.find(d);
        if(it != expectedTrancheLoss_.end()){
            return it->second;
        }
        QuantLib::MersenneTwisterUniformRng urng(42);
        QuantLib::InverseCumulativeRng<QuantLib::MersenneTwisterUniformRng, QuantLib::InverseCumulativeNormal> rng(
            urng);
        
        double expectedLoss = 0.0;
        // Compute determistic LGD case
        //if(recoveryRates_.front().size() == 1){
            const std::vector<double> pds = basket_->remainingProbabilities(d);
            const std::vector<double> notionals = basket_->notionals();
            std::vector<double> thresholds(basket_->size(), 0.0);
            InverseCumulativeNormal ICN;
            for (size_t id = 0; id < pds.size(); id++) {
                thresholds[id] = ICN(pds[id]);
            }
            const double sqrtRho = std::sqrt(baseCorrelation_);
            const double sqrtOneMinusRho = std::sqrt(1.0 - baseCorrelation_);
            for (size_t i = 0; i < nSamples_; i++) {
                const double marketFactor = rng.next().value() * sqrtRho;
                for (size_t id = 0; id < pds.size(); id++) {
                    if (marketFactor + sqrtOneMinusRho * rng.next().value() <= thresholds[i]) {
                        expectedLoss += notionals[id] * (1.0 - recoveryRates_[id].front());
                    }
                }
            }
        //}
        return expectedLoss;
    }

    QuantLib::Real correlation() const override { return baseCorrelation_; }
    // the call order matters, which is the reason for the parent to be the
    //   sole caller.
    //! Concrete models do now any updates/inits they need on basket reset
private:
    void resetModel() override {};
    double baseCorrelation_;
    std::vector<std::vector<double>> recoveryRates_;
    std::vector<std::vector<double>> recoveryProbabilities_;
    std::map<QuantLib::Date, double> expectedTrancheLoss_;
    
    size_t nSamples_;
};

} // namespace QuantExt

