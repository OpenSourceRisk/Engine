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

#ifndef quantext_homogenous_pool_default_model_hpp
#define quantext_homogenous_pool_default_model_hpp

#include <ql/experimental/credit/lossdistribution.hpp>
//#include <ql/experimental/credit/basket.hpp>
#include <qle/models/basket.hpp>
//#include <ql/experimental/credit/constantlosslatentmodel.hpp>
#include <qle/models/constantlosslatentmodel.hpp>
//#include <ql/experimental/credit/defaultlossmodel.hpp>
#include <qle/models/defaultlossmodel.hpp>

// Intended to replace HomogeneousPoolCDOEngine in syntheticcdoengines.hpp

namespace QuantExt {
using namespace QuantLib;

//-------------------------------------------------------------------------
//! Default loss distribution convolution for finite homogeneous pool
/* A note on the number of buckets: As it is now the code goes splitting
losses into buckets from loses equal to zero to losses up to the value of
the underlying basket. This is in view of a stochastic loss given default
but in a constant LGD situation this is a waste and it is more efficient to
go up to the attainable losses.
\todo Extend to the multifactor case for a generic LM
*/
template <class copulaPolicy> class HomogeneousPoolLossModel : public QuantExt::DefaultLossModel {
private:
    void resetModel();

public:
    HomogeneousPoolLossModel(const QuantLib::ext::shared_ptr<ConstantLossLatentmodel<copulaPolicy>>& copula, Size nBuckets,
                             Real max = 5., Real min = -5., Real nSteps = 50)
        : copula_(copula), nBuckets_(nBuckets), max_(max), min_(min), nSteps_(nSteps), delta_((max - min) / nSteps) {
        QL_REQUIRE(copula->numFactors() == 1, "Inhomogeneous model not implemented for multifactor");
    }

protected:
    // RL: additional flag
    Distribution lossDistrib(const Date& d, bool zeroRecovery = false) const;

public:
    // RL: additional flag
    Real expectedTrancheLoss(const Date& d, bool zeroRecovery = false) const {
        Distribution dist = lossDistrib(d, zeroRecovery);
        // RL: dist.trancheExpectedValue() using x = dist.average(i)
        // FIXME: some remaining inaccuracy in dist.cumulativeDensity(detachAmount_)
        Real expectedLoss = 0;
        dist.normalize();
        for (Size i = 0; i < dist.size(); i++) {
            // Real x = dist.x(i) + dist.dx(i)/2; // in QL distribution.cpp
            Real x = dist.average(i);
            if (x < attachAmount_)
                continue;
            if (x > detachAmount_)
                break;
            expectedLoss += (x - attachAmount_) * dist.dx(i) * dist.density(i);
        }
        expectedLoss += (detachAmount_ - attachAmount_) * (1.0 - dist.cumulativeDensity(detachAmount_));
        return expectedLoss;

        return lossDistrib(d, zeroRecovery).cumulativeExcessProbability(attachAmount_, detachAmount_);
        // This one if the distribution is over the whole loss structure:
        // but it becomes very expensive
        /*
        return lossDistrib(d).trancheExpectedValue(attach_ * notional_,
            detach_ * notional_);
        */
    }
    Real percentile(const Date& d, Real percentile) const {
        Real portfLoss = lossDistrib(d).confidenceLevel(percentile);
        return std::min(std::max(portfLoss - attachAmount_, 0.), detachAmount_ - attachAmount_);
    }
    Real expectedShortfall(const Date& d, Probability percentile) const {
        Distribution dist = lossDistrib(d);
        dist.tranche(attachAmount_, detachAmount_);
        return dist.expectedShortfall(percentile);
    }

protected:
    const QuantLib::ext::shared_ptr<ConstantLossLatentmodel<copulaPolicy>> copula_;
    Size nBuckets_;
    mutable Real attach_, detach_, notional_, attachAmount_, detachAmount_;
    mutable std::vector<Real> notionals_;

private:
    // integration:
    //  \todo move integration to latent model types when moving to a
    //  multifactor version
    const Real max_; // redundant?
    const Real min_;
    const Real nSteps_;
    const Real delta_;
};
// \todo Add other loss distribution statistics
typedef HomogeneousPoolLossModel<GaussianCopulaPolicy> HomogGaussPoolLossModel;
typedef HomogeneousPoolLossModel<TCopulaPolicy> HomogTPoolLossModel;

//-----------------------------------------------------------------------

template <class CP> void HomogeneousPoolLossModel<CP>::resetModel() {
    // need to be capped now since the limit amounts might be over the
    //  remaining notional (think amortizing)
    attach_ = std::min(basket_->remainingAttachmentAmount() / basket_->remainingNotional(), 1.);
    detach_ = std::min(basket_->remainingDetachmentAmount() / basket_->remainingNotional(), 1.);
    notional_ = basket_->remainingNotional();
    notionals_ = basket_->remainingNotionals();
    attachAmount_ = basket_->remainingAttachmentAmount();
    detachAmount_ = basket_->remainingDetachmentAmount();

    copula_->resetBasket(basket_.currentLink());
}

// RL: additional flag
template <class CP> Distribution HomogeneousPoolLossModel<CP>::lossDistrib(const Date& d, bool zeroRecovery) const {
    LossDistHomogeneous bucktLDistBuff(nBuckets_, detachAmount_);

    std::vector<Real> lgd; // switch to a mutable cache member
    // RL: additional option used to set set recovery rates to zero
    // std::vector<Real> recoveries = copula_->recoveries();
    std::vector<Real> recoveries =
        zeroRecovery ? std::vector<Real>(copula_->recoveries().size(), 0.0) : copula_->recoveries();
    std::transform(recoveries.begin(), recoveries.end(), std::back_inserter(lgd), [](Real x) { return 1.0 - x; });
    std::transform(lgd.begin(), lgd.end(), notionals_.begin(), lgd.begin(), std::multiplies<Real>());
    std::vector<Real> prob = basket_->remainingProbabilities(d);
    for (Size iName = 0; iName < prob.size(); iName++)
        prob[iName] = copula_->inverseCumulativeY(prob[iName], iName);

    // integrate locally (1 factor).
    // use explicitly a 1D latent model object?
    Distribution dist(nBuckets_, 0.0, detachAmount_);
    // notional_);
    std::vector<Real> mkft(1, min_ + delta_ / 2.);
    for (Size i = 0; i < nSteps_; i++) {
        std::vector<Real> conditionalProbs;
        for (Size iName = 0; iName < notionals_.size(); iName++)
            conditionalProbs.push_back(copula_->conditionalDefaultProbabilityInvP(prob[iName], iName, mkft));
        Distribution d = bucktLDistBuff(lgd, conditionalProbs);
        Real densitydm = delta_ * copula_->density(mkft);
        // also, instead of calling the static method it could be wrapped
        // through an inlined call in the latent model
        for (Size j = 0; j < nBuckets_; j++) {
            dist.addDensity(j, d.density(j) * densitydm);
            dist.addAverage(j, d.average(j) * densitydm); // RL
        }
        mkft[0] += delta_;
    }
    return dist;
}

} // namespace QuantExt

#endif
