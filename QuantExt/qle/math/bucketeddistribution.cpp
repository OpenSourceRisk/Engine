/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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


/*! \file qle/math/bucketeddistribution.cpp
    \brief Deals with a bucketed distribution
*/

#include <numeric>

#include <functional>
#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>

#include <qle/math/bucketeddistribution.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

BucketedDistribution::BucketedDistribution(Real min, Real max, Size numberBuckets)
    : buckets_(numberBuckets + 1, 0.0), probabilities_(numberBuckets, 0.0), points_(numberBuckets, 0.0) {

    init(min, max, numberBuckets);

    // Initially put all probability in 1st bucket
    probabilities_[0] = 1.0;
    previousProbabilities_ = probabilities_;
}

BucketedDistribution::BucketedDistribution(Real min, Real max, Size numberBuckets, Real initialValue)
    : buckets_(numberBuckets + 1, 0.0), probabilities_(numberBuckets, initialValue), points_(numberBuckets, 0.0),
      previousProbabilities_(probabilities_) {

    init(min, max, numberBuckets);
}

BucketedDistribution::BucketedDistribution(const vector<Real>& buckets, const vector<Real>& initialProbabilities,
                                           const vector<Real>& initialPoints)
    : buckets_(buckets), probabilities_(initialProbabilities), points_(initialPoints),
      previousProbabilities_(initialProbabilities), previousPoints_(initialPoints) {

    // Initial checks
    QL_REQUIRE(buckets_.size() >= 3, "There should be at least two buckets for the distribution");
    QL_REQUIRE(buckets_.size() == probabilities_.size() + 1, "The number of elements in the buckets "
                                                             "vector must exceed the number of probabilities by 1");
    QL_REQUIRE(buckets_.size() == points_.size() + 1, "The number of elements in the buckets "
                                                      "vector must exceed the number of point masses by 1");

    // Check that the buckets are sorted
    vector<Real>::iterator pos = adjacent_find(buckets_.begin(), buckets_.end(), greater<Real>());
    QL_REQUIRE(pos == buckets_.end(), "The vector of buckets must be sorted in ascending order");
}

BucketedDistribution::BucketedDistribution(const BucketedDistribution& other)
    : buckets_(other.buckets_), probabilities_(other.probabilities_), points_(other.points_),
      previousProbabilities_(other.previousProbabilities_), previousPoints_(other.previousPoints_) {}

const Real BucketedDistribution::minProbability_ = 0.00000001;

void BucketedDistribution::add(const DiscreteDistribution& distribution) {

    // Update the distribution with the values provided
    previousProbabilities_ = probabilities_;
    previousPoints_ = points_;
    vector<Real> tempPoints(points_.size(), 0.0);
    vector<Real> tempProbabilities = previousProbabilities_;
    vector<bool> bucketsChanged(points_.size(), false);

    for (Size i = 0; i < buckets_.size() - 1; i++) {
        // Skip buckets that have no probability of being occupied
        if (previousProbabilities_[i] >= minProbability_) {
            // Update buckets reachable from ith bucket
            for (Size j = 0; j < distribution.size(); j++) {
                Distributionpair pair = distribution.get(j);
                Real transitionPoint = previousPoints_[i] + pair.x_;
                Real transitionProbability = previousProbabilities_[i] * pair.y_;

                // Find in which bucket transitionPoint lies
                QL_REQUIRE(buckets_[0] <= transitionPoint && transitionPoint <= buckets_.back(),
                           "Value, " << transitionPoint << ", is out of range of buckets: (" << buckets_[0] << ", "
                                     << buckets_.back() << ")");

                // Either transition to a higher bucket or stay in the same bucket
                if (transitionPoint >= buckets_[i + 1]) {
                    Size bucketIndex = 0;
                    vector<Real>::const_iterator it =
                        upper_bound(buckets_.begin() + i + 1, buckets_.end(), transitionPoint);
                    if (it == buckets_.end()) {
                        // If here, must be equal to upper end of last bucket
                        bucketIndex = buckets_.size() - 2;
                    } else {
                        bucketIndex = it - buckets_.begin() - 1;
                    }

                    // Update probability if moved to different bucket
                    probabilities_[i] -= transitionProbability;
                    probabilities_[bucketIndex] += transitionProbability;
                    // Update temp points (divide again in separate loop below)
                    tempPoints[bucketIndex] += transitionPoint * transitionProbability;
                    tempProbabilities[bucketIndex] += transitionProbability;
                    bucketsChanged[bucketIndex] = true;
                } else {
                    // If bucket does not change, do not update probability but shift
                    // the conditional expectation i.e. point in bucket
                    points_[i] += pair.x_ * pair.y_;
                }
            }
        }
    }

    // If the probability of being in bucket is non-zero and the bucket has been hit by
    // the addition of the distribution above, update the conditional expectation (i.e. points_)
    for (Size i = 0; i < buckets_.size() - 1; i++) {
        if (tempProbabilities[i] > minProbability_ && bucketsChanged[i]) {
            points_[i] = (previousProbabilities_[i] * points_[i] + tempPoints[i]) / tempProbabilities[i];
        }
    }
}

Size BucketedDistribution::bucket(Real value) const {

    QL_REQUIRE(buckets_[0] <= value && value <= buckets_.back(), "Value, " << value << ", is out of range of buckets: ("
                                                                           << buckets_[0] << ", " << buckets_.back()
                                                                           << ")");

    vector<Real>::const_iterator it = upper_bound(buckets_.begin(), buckets_.end(), value);
    if (it == buckets_.end()) {
        // If here, must be equal to upper end of last bucket
        return buckets_.size() - 2;
    }
    return it - buckets_.begin() - 1;
}

void BucketedDistribution::init(Real min, Real max, Size numberBuckets) {
    // Initial checks
    QL_REQUIRE(buckets_.size() >= 3, "There should be at least two buckets for the distribution");
    QL_REQUIRE(max > min, "Max should be strictly greater than min");

    // Divide [min, max] into equally sized buckets
    Real bucketSize = (max - min) / numberBuckets;
    for (Size i = 0; i < buckets_.size(); ++i) {
        buckets_[i] = min + i * bucketSize;
    }

    // Initially set points to lower end of buckets
    copy(buckets_.begin(), buckets_.end() - 1, points_.begin());
    previousPoints_ = points_;
}

vector<Real> BucketedDistribution::cumulativeProbabilities() const {
    vector<Real> cumulativeProbabilities(buckets_.size(), 0.0);
    cumulativeProbabilities[0] = 0.0;
    // Calculate running sum i.e. probability that <= endpoint of each bucket
    transform(probabilities_.begin(), probabilities_.end(), cumulativeProbabilities.begin(),
              cumulativeProbabilities.begin() + 1, plus<Real>());
    return cumulativeProbabilities;
}

vector<Real> BucketedDistribution::complementaryProbabilities() const {
    vector<Real> probabilities = cumulativeProbabilities();
    // transform(probabilities.begin(), probabilities.end(), probabilities.begin(), std::bind1st(minus<Real>(), 1.0));
    transform(probabilities.begin(), probabilities.end(), probabilities.begin(), [](const Real x) { return x - 1.0; });
    return probabilities;
}

void BucketedDistribution::applyShift(Real shift) {
    // transform(buckets_.begin(), buckets_.end(), buckets_.begin(), std::bind1st(plus<Real>(), shift));
    transform(buckets_.begin(), buckets_.end(), buckets_.begin(), [shift](const Real x) { return x + shift; });
    // transform(points_.begin(), points_.end(), points_.begin(), std::bind1st(plus<Real>(), shift));
    transform(points_.begin(), points_.end(), points_.begin(), [shift](const Real x) { return x + shift; });
    // transform(previousPoints_.begin(), previousPoints_.end(), previousPoints_.begin(), std::bind1st(plus<Real>(),
    // shift));
    transform(previousPoints_.begin(), previousPoints_.end(), previousPoints_.begin(),
              [shift](const Real x) { return x + shift; });
}

void BucketedDistribution::applyFactor(Real factor) {
    // If factor is negative, reverse everything before scaling so buckets are still ascending
    if (factor < 0.0) {
        reverse(buckets_.begin(), buckets_.end());
        reverse(points_.begin(), points_.end());
        reverse(previousPoints_.begin(), previousPoints_.end());
        reverse(probabilities_.begin(), probabilities_.end());
        reverse(previousProbabilities_.begin(), previousProbabilities_.end());
    }
    // transform(buckets_.begin(), buckets_.end(), buckets_.begin(), std::bind1st(multiplies<Real>(), factor));
    transform(buckets_.begin(), buckets_.end(), buckets_.begin(), [factor](const Real x) { return x * factor; });
    // transform(points_.begin(), points_.end(), points_.begin(), std::bind1st(multiplies<Real>(), factor));
    transform(points_.begin(), points_.end(), points_.begin(), [factor](const Real x) { return x * factor; });
    // transform(previousPoints_.begin(), previousPoints_.end(), previousPoints_.begin(),
    // std::bind1st(multiplies<Real>(), factor));
    transform(previousPoints_.begin(), previousPoints_.end(), previousPoints_.begin(),
              [factor](const Real x) { return x * factor; });
}

Real BucketedDistribution::cumulativeProbability(Real x) const {
    vector<Real> probabilities = cumulativeProbabilities();
    vector<Real>::const_iterator it = lower_bound(buckets_.begin(), buckets_.end(), x);
    // If x > upper end of last bucket
    if (it == buckets_.end())
        return 1.0;
    // If x <= lower end of first bucket
    if (it == buckets_.begin())
        return 0.0;
    // If x is in range of buckets
    Size index = it - buckets_.begin();
    return probabilities[index - 1] + (x - buckets_[index - 1]) * (probabilities[index] - probabilities[index - 1]) /
                                          (buckets_[index] - buckets_[index - 1]);
}

Real BucketedDistribution::inverseCumulativeProbability(Real p) const {
    QL_REQUIRE(0 <= p && p <= 1.0, "Probability must be between 0 and 1");
    vector<Real> probabilities = cumulativeProbabilities();
    vector<Real>::const_iterator it = lower_bound(probabilities.begin(), probabilities.end(), p);
    // If p > last cumulative probability (shouldn't happen since p <= 1.0)
    if (it == probabilities.end())
        return buckets_.back();
    // If p <= first cumulative probability (shouldn't happen since p >= 0.0)
    if (it == probabilities.begin())
        return buckets_.front();
    // If p is in the range of probabilities
    Size index = it - probabilities.begin();
    return buckets_[index - 1] + (p - probabilities[index - 1]) * (buckets_[index] - buckets_[index - 1]) /
                                     (probabilities[index] - probabilities[index - 1]);
}

BucketedDistribution& BucketedDistribution::operator+=(const BucketedDistribution& other) {

    QL_REQUIRE(buckets_.size() - 1 == other.numberBuckets(), "Distributions must have same number of buckets to sum");

    bool bucketsEqual = equal(buckets_.begin(), buckets_.end(), other.buckets().begin(),
                              static_cast<bool (*)(Real, Real)>(&close_enough));
    QL_REQUIRE(bucketsEqual, "Distributions must have the same buckets to sum");

    transform(probabilities_.begin(), probabilities_.end(), other.probabilities().begin(), probabilities_.begin(),
              plus<Real>());

    return *this;
}

DiscreteDistribution BucketedDistribution::createDiscrete() const {
    // Create a vector containing midpoint of each bucket
    vector<Real> midpoints(probabilities_.size(), 0.0);
    transform(buckets_.begin(), buckets_.end() - 1, buckets_.begin() + 1, midpoints.begin(), plus<Real>());
    // transform(midpoints.begin(), midpoints.end(), midpoints.begin(), std::bind2nd(divides<Real>(), 2.0));
    transform(midpoints.begin(), midpoints.end(), midpoints.begin(),
              [](const Real x) { return x / 2.0; }); // IS THIS CORRECT?!?

    return DiscreteDistribution(midpoints, probabilities_);
}

void BucketedDistribution::erase(Size n) {
    QL_REQUIRE(n < numberBuckets(), "There are not enough buckets to erase");
    buckets_.erase(buckets_.begin(), buckets_.begin() + n);
    probabilities_.erase(probabilities_.begin(), probabilities_.begin() + n);
    points_.erase(points_.begin(), points_.begin() + n);
    previousProbabilities_.erase(previousProbabilities_.begin(), previousProbabilities_.begin() + n);
    previousPoints_.erase(previousPoints_.begin(), previousPoints_.begin() + n);
}

BucketedDistribution operator+(const BucketedDistribution& lhs, const BucketedDistribution& rhs) {

    BucketedDistribution result = lhs;
    result += rhs;
    return result;
}

BucketedDistribution operator*(Real factor, const BucketedDistribution& rhs) {
    vector<Real> probabilities(rhs.numberBuckets(), 0.0);
    // transform(rhs.probabilities().begin(), rhs.probabilities().end(), probabilities.begin(),
    // std::bind1st(multiplies<Real>(), factor));
    transform(rhs.probabilities().begin(), rhs.probabilities().end(), probabilities.begin(),
              [factor](const Real x) { return x * factor; });
    return BucketedDistribution(rhs.buckets(), probabilities, rhs.points());
}

BucketedDistribution operator*(const BucketedDistribution& lhs, QuantLib::Real factor) { return factor * lhs; }

} // namespace QuantExt
