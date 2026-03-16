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

#ifndef quantext_pool_loss_model_hpp
#define quantext_pool_loss_model_hpp

#include <ql/experimental/credit/lossdistribution.hpp>
#include <qle/models/basket.hpp>
#include <qle/models/extendedconstantlosslatentmodel.hpp>
#include <qle/models/defaultlossmodel.hpp>
#include <qle/models/hullwhitebucketing.hpp>
#include <functional>
#include <iostream>

// clang-format off
namespace QuantExt {

/*! Default loss distribution convolution for finite homogeneous or non-homogeneous pool

    \todo Extend to the multifactor case for a generic LM
*/
template <class CopulaPolicy>
class PoolLossModel : public DefaultLossModel {

public:
    PoolLossModel(
        bool homogeneous,
        const QuantLib::ext::shared_ptr<ExtendedConstantLossLatentModel<CopulaPolicy>>& copula,
        QuantLib::Size nBuckets,
        QuantLib::Real max = 5.0,
        QuantLib::Real min = -5.0,
        QuantLib::Size nSteps = 50,
        bool useQuadrature = false,
        bool useStochasticRecovery = false);

    QuantLib::Real expectedTrancheLoss(const QuantLib::Date& d, Real recoveryRate = Null<Real>()) const override;

    QuantLib::Real percentile(const QuantLib::Date& d, QuantLib::Real percentile) const override;

    QuantLib::Real expectedShortfall(const QuantLib::Date& d, QuantLib::Probability percentile) const override;

    QuantLib::Real correlation() const override;

    std::vector<std::vector<Real>> marginalProbabilitiesVV(Date d, Real recoveryRate = Null<Real>()) const;
    
private:
    bool homogeneous_;
    QuantLib::ext::shared_ptr<ExtendedConstantLossLatentModel<CopulaPolicy>> copula_;
    QuantLib::Size nBuckets_;
    QuantLib::Real max_;
    QuantLib::Real min_;
    QuantLib::Size nSteps_;
    bool useQuadrature_;
    bool useStochasticRecovery_;

    QuantLib::Real delta_;
    mutable QuantLib::Real attach_;
    mutable QuantLib::Real detach_;
    mutable QuantLib::Real notional_;
    mutable QuantLib::Real attachAmount_;
    mutable QuantLib::Real detachAmount_;
    mutable std::vector<QuantLib::Real> notionals_;

    // deterministic LGD vector by entity
    mutable std::vector<QuantLib::Real> lgd_;
    // marginal probabilities, by entity and recovery rate, second dimension size 1 when deterministic
    mutable std::vector<std::vector<QuantLib::Real>> q_;
    // Copula model thresholds, with same dimension as q_
    mutable std::vector<std::vector<QuantLib::Real>> c_;
    // LGD by entity and (stochastic) recovery rate dimension 
    mutable std::vector<std::vector<QuantLib::Real>> lgdVV_;
    // conditional probability of default with recovery rate, same dimension as lgdVV_
    mutable std::vector<std::vector<QuantLib::Real>> cprVV_;
    
    QuantLib::Distribution lossDistrib(const QuantLib::Date& d, Real recoveryRate = Null<Real>()) const;
    // update lgdVV_
    void updateLGDs(Real recoveryRate = Null<Real>()) const;
    // update q_and c_
    void updateThresholds(QuantLib::Date d, Real recoveryRate = Null<Real>()) const;
    // update cprVV_
    std::vector<Real> updateCPRs(std::vector<QuantLib::Real> factor, Real recoveryRate = Null<Real>()) const;

    void resetModel() override;

    // for checking consistency between Distribution and Arrays
    QuantLib::Real expectedTrancheLoss1(const QuantLib::Date& d, Distribution& dist) const;
    QuantLib::Real expectedTrancheLoss2(const QuantLib::Date& d, const Array& p, const Array& A) const;
};

typedef PoolLossModel<GaussianCopulaPolicy> GaussPoolLossModel;
typedef PoolLossModel<TCopulaPolicy> StudentPoolLossModel;

// Helper class used to support Quadrature integration.
template <class CopulaPolicy>
class LossModelConditionalDist {
public:
    LossModelConditionalDist(const QuantLib::ext::shared_ptr<ExtendedConstantLossLatentModel<CopulaPolicy>>& copula,
        const QuantLib::ext::shared_ptr<QuantLib::LossDist>& bucketing,
        const std::vector<QuantLib::Real>& marginalDps,
        const std::vector<QuantLib::Real>& lgds);

    QuantLib::Real conditionalDensity(QuantLib::Real factor, QuantLib::Size bucket);
    QuantLib::Real conditionalAverage(QuantLib::Real factor, QuantLib::Size bucket);

private:
    const QuantLib::ext::shared_ptr<ExtendedConstantLossLatentModel<CopulaPolicy>> copula_;
    QuantLib::ext::shared_ptr<QuantLib::LossDist> bucketing_;
    std::vector<QuantLib::Real> inverseMarginalDps_;
    std::vector<QuantLib::Real> lgds_;

    // Return the conditional distribution and factor density for the given factor.
    std::pair<Distribution, QuantLib::Real> distribution(QuantLib::Real factor);

    // Store the conditional distribution and factor density keyed on the market factor.
    struct keyCmp {
        bool operator()(QuantLib::Real x_1, QuantLib::Real x_2) const {
            return !close(x_1, x_2) && x_1 < x_2;
        }
    };
    std::map<QuantLib::Real, std::pair<Distribution, QuantLib::Real>, keyCmp> conditionalDists_;
};

template <class CopulaPolicy>
PoolLossModel<CopulaPolicy>::PoolLossModel(
    bool homogeneous,
    const QuantLib::ext::shared_ptr<ExtendedConstantLossLatentModel<CopulaPolicy>>& copula,
    QuantLib::Size nBuckets,
    QuantLib::Real max,
    QuantLib::Real min,
    QuantLib::Size nSteps,
    bool useQuadrature,
    bool useStochasticRecovery)
    : homogeneous_(homogeneous),
      copula_(copula),
      nBuckets_(nBuckets),
      max_(max),
      min_(min),
      nSteps_(nSteps),
      useQuadrature_(useQuadrature),
      useStochasticRecovery_(useStochasticRecovery),
      delta_((max - min) / nSteps),
      attach_(0.0),
      detach_(0.0),
      notional_(0.0),
      attachAmount_(0.0),
      detachAmount_(0.0) {

    QL_REQUIRE(copula->numFactors() == 1, "Multifactor PoolLossModel not yet implemented.");
}

template <class CopulaPolicy>
QuantLib::Real PoolLossModel<CopulaPolicy>::expectedTrancheLoss(const QuantLib::Date& d, Real recoveryRate) const {

    QuantLib::Distribution dist = lossDistrib(d, recoveryRate);

    // RL: dist.trancheExpectedValue() using x = dist.average(i)
    // FIXME: some remaining inaccuracy in dist.cumulativeDensity(detachAmount_)
    QuantLib::Real expectedLoss = 0;
    dist.normalize();
    for (QuantLib::Size i = 0; i < dist.size(); i++) {
        // Real x = dist.x(i) + dist.dx(i)/2; // in QL distribution.cpp
        QuantLib::Real x = dist.average(i);
        if (x < attachAmount_)
            continue;
        if (x > detachAmount_)
            break;
        expectedLoss += (x - attachAmount_) * dist.dx(i) * dist.density(i);
    }
    expectedLoss += (detachAmount_ - attachAmount_) * (1.0 - dist.cumulativeDensity(detachAmount_));

    return expectedLoss;
}
    
template <class CopulaPolicy>
QuantLib::Real PoolLossModel<CopulaPolicy>::expectedTrancheLoss1(const QuantLib::Date& d, Distribution& dist) const {

    // RL: dist.trancheExpectedValue() using x = dist.average(i)
    // FIXME: some remaining inaccuracy in dist.cumulativeDensity(detachAmount_)
    QuantLib::Real expectedLoss = 0;
    for (QuantLib::Size i = 0; i < dist.size(); i++) {
        // Real x = dist.x(i) + dist.dx(i)/2; // in QL distribution.cpp
        QuantLib::Real x = dist.average(i);
        if (x < attachAmount_)
            continue;
        if (x > detachAmount_)
            break;
        expectedLoss += (x - attachAmount_) * dist.dx(i) * dist.density(i);
    }
    expectedLoss += (detachAmount_ - attachAmount_) * (1.0 - dist.cumulativeDensity(detachAmount_));

    return expectedLoss;
}

template <class CopulaPolicy>
QuantLib::Real PoolLossModel<CopulaPolicy>::expectedTrancheLoss2(const QuantLib::Date& d, const Array& p, const Array& A) const {
    // QL_REQUIRE(A.back() > detachAmount_, "A grid (max " << A.back() << ") does not cover the tranche with D " << detachAmount_);
    QuantLib::Real expectedLoss = 0;
    QuantLib::Real cumulative = 0.0;
    for (QuantLib::Size i = 0; i < p.size(); i++) {
        QuantLib::Real x = A[i];
        if (x < attachAmount_)
            continue;
        if (x > detachAmount_)
            break;
        expectedLoss += (x - attachAmount_) * p[i];
        cumulative += p[i];
    }
    expectedLoss += (detachAmount_ - attachAmount_) * (1.0 - cumulative);

    return expectedLoss;
}

template <class CopulaPolicy>
QuantLib::Real PoolLossModel<CopulaPolicy>::percentile(const QuantLib::Date& d, QuantLib::Real percentile) const {
    QuantLib::Real portfLoss = lossDistrib(d).confidenceLevel(percentile);
    return std::min(std::max(portfLoss - attachAmount_, 0.), detachAmount_ - attachAmount_);
}

template <class CopulaPolicy>
QuantLib::Real PoolLossModel<CopulaPolicy>::expectedShortfall(
    const QuantLib::Date& d, QuantLib::Probability percentile) const {

    QuantLib::Distribution dist = lossDistrib(d);
    dist.tranche(attachAmount_, detachAmount_);
    return dist.expectedShortfall(percentile);
}

template <class CopulaPolicy>
QuantLib::Real PoolLossModel<CopulaPolicy>::correlation() const {

    const std::vector<std::vector<QuantLib::Real> >& weights = copula_->factorWeights();

    if (weights.empty() || weights[0].size() != 1) {
        return QuantLib::Null<QuantLib::Real>();
    }

    QuantLib::Real res = weights[0][0];
    for (QuantLib::Size i = 1; i < weights.size(); ++i) {
        if (weights[i].size() != 1 || !close(res, weights[i][0])) {
            return QuantLib::Null<QuantLib::Real>();
        }
    }

    return res * res;
}

template <class CopulaPolicy>
void PoolLossModel<CopulaPolicy>::updateLGDs(Real recoveryRate) const {

    // one LGD per entity, notional modified with deterministic recovery rate
    lgd_ = notionals_;
    const std::vector<Real>& recoveries = copula_->recoveries();
    for (QuantLib::Size i = 0; i < notionals_.size(); ++i) {
        if (recoveryRate != Null<Real>())
            lgd_[i] *= (1 - recoveryRate);
        else
            lgd_[i] *= (1 - recoveries[i]);
    }
    
    // Initialize the LGD matrix for stochastic recovery
    lgdVV_.clear();    
    lgdVV_.resize(notionals_.size(), std::vector<Real>());
    for (Size i = 0; i < notionals_.size(); ++i) {
        if (recoveryRate != Null<Real>())
            lgdVV_[i].push_back(notionals_[i] * (1.0 - recoveryRate));
        else if (!useStochasticRecovery_)
            lgdVV_[i].push_back(notionals_[i] * (1.0 - recoveries[i]));
        else {
            const std::vector<Real>& rrGrid = copula_->recoveryRateGrids()[i];
            for (Size j = 0; j < rrGrid.size(); ++j) {                
                QL_REQUIRE(j == 0 || rrGrid[j] <= rrGrid[j-1], "recovery rates need to be sorted in decreasing order");
                // j == 0 corresponds to the lowest recovery or largest loss
                lgdVV_[i].push_back(notionals_[i] * (1.0 - rrGrid[j]));
            }
            // std::cout << std::endl;
        }
    }
}

template <class CopulaPolicy>
void PoolLossModel<CopulaPolicy>::updateThresholds(QuantLib::Date d, Real recoveryRate) const {
    // Initialize probability of default function Q and thresholds C according to spec

    // Marginal probabilities of default for each remaining entity in basket
    std::vector<QuantLib::Real> prob = basket_->remainingProbabilities(d);

    q_.resize(notionals_.size());
    c_.resize(notionals_.size());

    if (useStochasticRecovery_ && recoveryRate == Null<Real>()) {
        Real tiny = 1.0e-10;
        for (Size i = 0; i < notionals_.size(); ++i) {
            QL_REQUIRE(copula_->recoveryProbabilities().size() == notionals_.size(),
                       "number of rec rate probability vectors does not match number of notionals"); 
            std::vector<Real> rrProbs = copula_->recoveryProbabilities()[i];
            q_[i] = std::vector<Real>(rrProbs.size() + 1, prob[i]);
            c_[i] = std::vector<Real>(rrProbs.size() + 1, copula_->inverseCumulativeY(q_[i][0], i));
            Real sum = 0.0;
            for (Size j = 0; j < rrProbs.size(); ++j) {
                sum += rrProbs[j];
                q_[i][j+1] = q_[i][0] * (1.0 - sum);
                if (QuantLib::close_enough(q_[i][j+1], 0.0))
                    c_[i][j+1] = QL_MIN_REAL;
                else
                    c_[i][j+1] = copula_->inverseCumulativeY(q_[i][j+1], i); 
            }
            QL_REQUIRE(fabs(q_[i].back()) < tiny, "expected zero qij, but found " << q_[i].back() << " for i=" << i); 
        }
    }
    else {
        for (QuantLib::Size i = 0; i < prob.size(); i++) {
            q_[i] = std::vector<Real>(1, prob[i]);
            c_[i] = std::vector<Real>(1, copula_->inverseCumulativeY(prob[i], i));
        }
    }
}

template <class CopulaPolicy>
std::vector<std::vector<Real>> PoolLossModel<CopulaPolicy>::marginalProbabilitiesVV(Date d, Real recoveryRate) const {

    std::vector<QuantLib::Real> prob = basket_->remainingProbabilities(d);
    std::vector<std::vector<QuantLib::Real>> probVV(notionals_.size(), std::vector<Real>());
    if (!useStochasticRecovery_ || recoveryRate == Null<Real>()) {
        for (Size i = 0; i < notionals_.size(); ++i)
            probVV[i].resize(1, prob[i]);
        return probVV;
    } else {
        QL_REQUIRE(copula_->recoveryProbabilities().size() == notionals_.size(),
                   "number of rec rate probability vectors does not match number of notionals"); 
        for (Size i = 0; i < notionals_.size(); ++i) {
            Real pd = prob[i];
            std::vector<Real> rrProbs = copula_->recoveryProbabilities()[i];
            probVV[i].resize(rrProbs.size(), 0.0);
            for (Size j = 0; j < rrProbs.size(); ++j)
                probVV[i][j] = pd * rrProbs[j];
        }
        return probVV;
    }
}

template <class CopulaPolicy>
std::vector<Real> PoolLossModel<CopulaPolicy>::updateCPRs(std::vector<QuantLib::Real> factor, Real recoveryRate) const {
    cprVV_.clear();

    Real tiny = 1.0e-10;
    if (useStochasticRecovery_ && recoveryRate == Null<Real>()) {
        cprVV_.resize(notionals_.size(), std::vector<Real>());
        for (Size i = 0; i < c_.size(); ++i) {
            cprVV_[i].resize(c_[i].size() - 1, 0.0);
            Real pd = copula_->conditionalDefaultProbabilityInvP(c_[i][0], i, factor);
            Real sum = 0.0;
            for (Size j = 1; j < c_[i].size(); ++j) {
                // probability of recovery j conditional on default of i
                cprVV_[i][j-1] = copula_->conditionalDefaultProbabilityInvP(c_[i][j-1], i, factor)
                    - copula_->conditionalDefaultProbabilityInvP(c_[i][j], i, factor);
                sum += cprVV_[i][j-1];
            }
            QL_REQUIRE(fabs(sum - pd) < tiny, "probability check failed for factor0 " << factor[0]);
        }
    }
    else {
        cprVV_.resize(notionals_.size(), std::vector<Real>(1, 0));
        for (Size i = 0; i < c_.size(); ++i) {
            cprVV_[i][0] = copula_->conditionalDefaultProbabilityInvP(c_[i][0], i, factor);
        }
    }
    
    // Vector of default probabilities conditional on the common market factor M: P(\tau_i < t | M = m).
    std::vector<Real> probs;
    for (Size i = 0; i < c_.size(); i++)
        probs.push_back(copula_->conditionalDefaultProbabilityInvP(c_[i][0], i, factor));

    return probs;
}

template <class CopulaPolicy>
void PoolLossModel<CopulaPolicy>::resetModel() {
    // need to be capped now since the limit amounts might be over the remaining notional (think amortizing)
    attach_ = std::min(basket_->remainingAttachmentAmount() / basket_->remainingNotional(), 1.);
    detach_ = std::min(basket_->remainingDetachmentAmount() / basket_->remainingNotional(), 1.);
    notional_ = basket_->remainingNotional();
    notionals_ = basket_->remainingNotionals();
    attachAmount_ = basket_->remainingAttachmentAmount();
    detachAmount_ = basket_->remainingDetachmentAmount();
    copula_->resetBasket(basket_.currentLink());
}
    
template <class CopulaPolicy>
QuantLib::Distribution PoolLossModel<CopulaPolicy>::lossDistrib(const QuantLib::Date& d, Real recoveryRate) const {

    bool check = false;
    
    Real maximum = detachAmount_; 
    Real minimum = 0.0;
    
    // Update the LGD vector, could be moved to resetModel() if we can disregard the zeroRecovery flag
    updateLGDs(recoveryRate);

    // Update probabilities qij and thresholds cij, needs to stay here because date dependent
    updateThresholds(d, recoveryRate);

    // Init bucketing class
    HullWhiteBucketing hwb(minimum, maximum, nBuckets_);

    // Container for the final (unconditional) loss distribution up to date d.
    Array p(hwb.buckets()-1, 0.0), A(hwb.buckets()-1, 0.0);
    QuantLib::Distribution dist(nBuckets_, minimum, maximum);
    
    // Use the relevant loss bucketing algorithm
    QuantLib::ext::shared_ptr<QuantLib::LossDist> bucketing;
    if (homogeneous_)
        bucketing = QuantLib::ext::make_shared<QuantLib::LossDistHomogeneous>(nBuckets_, maximum);
    else
        bucketing = QuantLib::ext::make_shared<QuantLib::LossDistBucketing>(nBuckets_, maximum);

    // Is the ql bucketing used?
    bool useQlBucketing = false;

    // Switch between quadrature integration and basic segment type scheme.
    if (useQuadrature_ && !useStochasticRecovery_) {

        // FIXME: Ensure quadrature works with stochastic recovery

        QuantLib::GaussHermiteIntegration Integrator(nSteps_);
        // Marginal probabilities for each remaining entity in basket, P(\tau_i < t).
        std::vector<QuantLib::Real> prob = basket_->remainingProbabilities(d);
        LossModelConditionalDist<CopulaPolicy> lmcd(copula_, bucketing, prob, lgd_);

        for (QuantLib::Size j = 0; j < nBuckets_; j++) {
            std::function<QuantLib::Real(QuantLib::Real)> densityFunc = std::bind(
                &LossModelConditionalDist<CopulaPolicy>::conditionalDensity, &lmcd, std::placeholders::_1, j);
            std::function<QuantLib::Real(QuantLib::Real)> averageFunc = std::bind(
                &LossModelConditionalDist<CopulaPolicy>::conditionalAverage, &lmcd, std::placeholders::_1, j);
            dist.addDensity(j, Integrator(densityFunc));
            dist.addAverage(j, Integrator(averageFunc));
        }

        useQlBucketing = true;

    } else {

        std::vector<QuantLib::Real> factor{ min_ + delta_ / 2.0 };      

        for (QuantLib::Size k = 0; k < nSteps_; k++) {
            
            std::vector<Real> cpr = updateCPRs(factor, recoveryRate);

            // Loss distribution up to date d conditional on common factor M = m.
            Distribution conditionalDist;
            if (useStochasticRecovery_) {
                // HW bucketing with multi-state extension for stochastic recovery
                // With stochastic recovery the portfolios is in general not homogeneous any more, so that
                // bucketing is the only choice. We could use this call in all cases (including deterministic
                // recovery, homogeneous pool), but this can cause small regression errors (using bucketing
                // instead of homogeneoius pool algorithm) and calculation time increase (QuantLib::LossDist's
                // bucketing is faster).
                hwb.computeMultiState(cprVV_.begin(), cprVV_.end(), lgdVV_.begin());                
            }
            else if (homogeneous_) {
                // Original QuantLib::LossDist (homogeneous), works with deterministic recovery only.
                // If possible we use the homogeneous algorithm in QuantLib::LossDist. This also avoids small
                // regression errors from switching to any of the bucketing algorithms in the homogeneous case. 
                conditionalDist = (*bucketing)(lgd_, cpr);
                useQlBucketing = true;
            }
            else {
                // We could use hwb.computeMultiState here as well, yields the same result,
                // but compute is slightly faster than computeMultiState.
                hwb.compute(cpr.begin(), cpr.end(), lgd_.begin());                
            }

            // Update final distribution with contribution from common factor M = m.
            Real densitydm = delta_ * copula_->density(factor);

            if (useQlBucketing) {
                for (Size j = 0; j < nBuckets_; j++) {
                    dist.addDensity(j, conditionalDist.density(j) * densitydm);
                    dist.addAverage(j, conditionalDist.average(j) * densitydm);
                }
            }
            else {
                // Bucket 0 contains losses up to lowerBound, and bucket 1 from [lowerBound, lowerBound + dx),
                // together both buckets contains (-inf, lowerBound + dx)
                // Since we dont have any negative losses in a CDO its [0, dx)
                double p0 = (hwb.probability()[0] + hwb.probability()[1]);
                double A0 = 0;
                if (!QuantLib::close_enough(p0, 0.0)) {
                    A0 = (hwb.averageLoss()[0] * hwb.probability()[0] + hwb.averageLoss()[1] * hwb.probability()[1]) / p0; 
                }
                p[0] += p0 * densitydm;
                A[0] += A0 * densitydm;
                 
                for (Size j = 2; j < hwb.buckets(); j++) {
                    p[j-1] += hwb.probability()[j] * densitydm;
                    A[j-1] += hwb.averageLoss()[j] * densitydm;
                }
            }
            
            // Move to next value of common factor in [min,max] region.
            factor[0] += delta_;
        }

        if (!useQlBucketing) {
            // Copy results to distribution, skip the right-most bucket (maximum, infty)
            for (Size j = 0; j < nBuckets_; j++) {
                dist.addDensity(j, p[j] / dist.dx(j));
                dist.addAverage(j, A[j]);
            }
        }
    }

    if (check && !useQlBucketing) {
        // This checks the consistency between the Distribution object and the "raw" p and A vectors
        // by way of expectedTrancheLoss calculations.
        // Can be deactivated because of its performance impact.
        Real etl1 = expectedTrancheLoss1(d, dist);
        Real etl2 = expectedTrancheLoss2(d, p, A);
        Real tiny = 1e-3;
        QL_REQUIRE(fabs((etl1 - etl2)/etl2) < tiny, "expected tranche loss failed, " << etl1 << " vs " << etl2);
    }

    return dist;
}

template <class CopulaPolicy>
LossModelConditionalDist<CopulaPolicy>::LossModelConditionalDist(
    const QuantLib::ext::shared_ptr<ExtendedConstantLossLatentModel<CopulaPolicy>>& copula,
    const QuantLib::ext::shared_ptr<QuantLib::LossDist>& bucketing,
    const std::vector<QuantLib::Real>& marginalDps,
    const std::vector<QuantLib::Real>& lgds)
    : copula_(copula), bucketing_(bucketing),
      inverseMarginalDps_(marginalDps.size()), lgds_(lgds) {

    for (Size i = 0; i < marginalDps.size(); i++) {
        inverseMarginalDps_[i] = copula_->inverseCumulativeY(marginalDps[i], i);
    }
}

template <class CopulaPolicy>
QuantLib::Real LossModelConditionalDist<CopulaPolicy>::conditionalDensity(
    QuantLib::Real factor, QuantLib::Size bucket) {
    auto p = distribution(factor);
    return p.first.density(bucket) * p.second;
}

template <class CopulaPolicy>
QuantLib::Real LossModelConditionalDist<CopulaPolicy>::conditionalAverage(
    QuantLib::Real factor, QuantLib::Size bucket) {
    auto p = distribution(factor);
    return p.first.average(bucket) * p.second;
}

template <class CopulaPolicy>
std::pair<Distribution, QuantLib::Real> LossModelConditionalDist<CopulaPolicy>::distribution(QuantLib::Real factor) {

    auto it = conditionalDists_.find(factor);
    if (it != conditionalDists_.end())
        return it->second;

    // If we do not already have the distribution for the given factor, we create it.
    // Vector of default probabilities conditional on the common market factor M: P(\tau_i < t | M = m)
    std::vector<QuantLib::Real> cps(inverseMarginalDps_.size());
    std::vector<QuantLib::Real> vFactor{ factor };
    for (Size i = 0; i < cps.size(); i++) {
        cps[i] = copula_->conditionalDefaultProbabilityInvP(inverseMarginalDps_[i], i, vFactor);
    }

    it = conditionalDists_.emplace(factor, std::make_pair((*bucketing_)(lgds_, cps), copula_->density(vFactor))).first;
    return it->second;
}

}

// clang-format on

#endif
