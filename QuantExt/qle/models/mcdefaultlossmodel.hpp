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

#include <functional>
#include <numeric>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/randomnumbers/inversecumulativerng.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <qle/models/defaultlossmodel.hpp>
#include <ored/utilities/log.hpp>

/* Intended to replace LossDistribution in
    ql/experimental/credit/lossdistribution, not sure its covering all the
    functionality (see mthod below)
*/

namespace QuantExt {
using namespace QuantLib;

/*! Model Class for 1 factor gaussian copula loss model
using monte carlo simulation
*/
class GaussianOneFactorMonteCarloLossModel : public DefaultLossModel, public Observer {

public:
    GaussianOneFactorMonteCarloLossModel(Handle<Quote> baseCorrelation, const std::vector<std::vector<double>>& recoveryRates,
                                         const std::vector<double>& recoveryProbabilites, const size_t samples)
        : baseCorrelation_(baseCorrelation), recoveryRates_(recoveryRates),
          recoveryProbabilities_(recoveryProbabilites), cumRecoveryProbabilities_(recoveryProbabilities_.size(), 0.0),
          nSamples_(samples) {

        registerWith(baseCorrelation);

        QL_REQUIRE(recoveryRates_.empty() || recoveryRates_.front().size() == recoveryProbabilities_.size(),
                   "Error mismatch vector size, between recoveryRates and their respective probability");

        std::partial_sum(recoveryProbabilities_.begin(), recoveryProbabilities_.end(),
                         cumRecoveryProbabilities_.begin());

        QL_REQUIRE(QuantLib::close_enough(cumRecoveryProbabilities_.back(), 1.0),
                   "Recovery Rate probs doesnt add up to 1.");

        cumRecoveryProbabilities_.back() = 1.0 + 1e-12;

        for (size_t i = 0; i < recoveryRates_.size(); ++i) {
            QL_REQUIRE(recoveryRates_[i].size() == recoveryProbabilities_.size(),
                       "All recoveryRates should have the same number of probs");
        }
    }

    void update() override {
        resetModel();
        notifyObservers();
    }

protected:
    Real expectedTrancheLoss(const Date& d, Real recoveryRate = Null<Real>()) const override {

        const std::vector<double> pds = basket_->remainingProbabilities(d);
        const std::vector<double> notionals = basket_->notionals();
        const std::vector<std::string> names = basket_->remainingNames();
        TLOG("Compute expectedTrancheLoss with MC for " << d);
        TLOG("Basket Information");
        TLOG("Basket attachment amount " << std::fixed << std::setprecision(2) << basket_->attachmentAmount());
        TLOG("Basket  dettachment Amount " << std::fixed << std::setprecision(2) << basket_->detachmentAmount());
        TLOG("Basket remaining attachment Amount " << std::fixed << std::setprecision(2)
                                                   << basket_->remainingAttachmentAmount(d));
        TLOG("Basket remaining dettachment Amount " << std::fixed << std::setprecision(2)
                                                    << basket_->remainingDetachmentAmount(d));
        TLOG("Constituents");
        TLOG("i,name,notional,pd");
        for (size_t i = 0; i < pds.size(); ++i) {
            TLOG(i << "," << names[i] << "," << notionals[i] << "," << pds[i]);
        }
        auto it = expectedTrancheLoss_.find(d);
        if(it != expectedTrancheLoss_.end()){
            return it->second;
        }
        QuantLib::MersenneTwisterUniformRng urng(42);
        QuantLib::InverseCumulativeRng<QuantLib::MersenneTwisterUniformRng, QuantLib::InverseCumulativeNormal> rng(
            urng);

        
        // Compute determistic LGD case
        // if(recoveryRates_.front().size() == 1){
        
        std::vector<double> thresholds(basket_->size(), 0.0);
        InverseCumulativeNormal ICN;
        for (size_t id = 0; id < pds.size(); id++) {
            thresholds[id] = ICN(pds[id]);
        }
        
        
        const double sqrtRho = std::sqrt(baseCorrelation_->value());
        const double sqrtOneMinusRho = std::sqrt(1.0 - baseCorrelation_->value());
        const double n = static_cast<double>(nSamples_);
        double trancheLoss = 0.0;
        double expectedLossIndex = 0.0;
        const size_t nConstituents = pds.size();
        std::vector<double> xs(nConstituents * nSamples_, 0.0);
        std::vector<double> ys(nConstituents * nSamples_, 0.0);
        for (size_t i = 0; i < nSamples_; i++) {
            const double marketFactor = rng.next().value * sqrtRho;
            for (size_t id = 0; id < nConstituents; id++) {
                xs[i * nConstituents + id] = marketFactor + sqrtOneMinusRho * rng.next().value;
                ys[i * nConstituents + id] = urng.next().value;
            }
        }

            std::vector<double> simPD(pds.size(), 0.0);
            for (size_t i = 0; i < nSamples_; i++) {
                double loss = 0.0;
                for (size_t id = 0; id < pds.size(); id++) {
                    if (xs[i * nConstituents + id]  <= thresholds[id]) {
                        simPD[id] += 1;
                        expectedLossIndex += notionals[id] * 0.6;
                        // TLOG("LGD calculation");
                        // TLOG(cumRecoveryProbabilities_[0] << "," << cumRecoveryProbabilities_[1] << "," <<
                        // cumRecoveryProbabilities_[2]);
                        auto rrIt =
                            std::lower_bound(cumRecoveryProbabilities_.begin(), cumRecoveryProbabilities_.end(), ys[i * nConstituents + id]);
                        // TLOG("rrIt=" << *rrIt);
                        size_t rrId = rrIt - cumRecoveryProbabilities_.begin();
                        // TLOG("index = " << rrId);
                        loss += notionals[id] * (1.0 - recoveryRates_[id][rrId]);
                    }
                }
                trancheLoss += std::max(loss - basket_->attachmentAmount(), 0.0) -
                               std::max(loss - basket_->detachmentAmount(), 0.0);
            }
            TLOG("Valid")
            for (size_t i = 0; i < pds.size(); ++i) {
                TLOG(i << "," << std::fixed << std::setprecision(8) << pds[i] << "," << simPD[i] / n);
            }
            TLOG("Expected Tranche Loss = " << trancheLoss / n);
            TLOG("Expected Index Loss " << expectedLossIndex / n);
            expectedTrancheLoss_[d] = trancheLoss / n;
            return trancheLoss / n;
        }

    QuantLib::Real correlation() const override { return baseCorrelation_->value(); }
    // the call order matters, which is the reason for the parent to be the
    //   sole caller.
    //! Concrete models do now any updates/inits they need on basket reset
private:
    void resetModel() override {
        expectedTrancheLoss_.clear();
    };
    Handle<Quote> baseCorrelation_;
    std::vector<std::vector<double>> recoveryRates_;
    std::vector<double> recoveryProbabilities_;
    std::vector<double> cumRecoveryProbabilities_;
    mutable std::map<QuantLib::Date, double> expectedTrancheLoss_;

    size_t nSamples_;
};

} // namespace QuantExt
