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


/*! \file qle/math/bucketeddistribution.hpp
    \brief Deals with a bucketed distribution
*/

#pragma once

#include <vector>

#include <ql/types.hpp>

#include <qle/math/discretedistribution.hpp>

namespace QuantExt {

//! Represents a bucketed probability distibution
class BucketedDistribution {
public:
    //! Default constructor
    BucketedDistribution() {}
    //! Build a default initial distribution from \p min and \p max and \p numberBuckets
    BucketedDistribution(QuantLib::Real min, QuantLib::Real max, QuantLib::Size numberBuckets);
    /*! Build a default initial distribution from \p min and \p max and \p numberBuckets and
     *  set all probabilities to \p initialValue
     */
    BucketedDistribution(QuantLib::Real min, QuantLib::Real max, QuantLib::Size numberBuckets,
                         QuantLib::Real initialValue);
    //! Explicitly specify the initial distribution
    BucketedDistribution(const std::vector<QuantLib::Real>& buckets,
                         const std::vector<QuantLib::Real>& initialProbabilities,
                         const std::vector<QuantLib::Real>& initialPoints);
    //! Copy constructor
    BucketedDistribution(const BucketedDistribution& other);
    //! Update the bucketed distribution by adding a discrete distribution
    void add(const DiscreteDistribution& distribution);
    //! Return the buckets of the distribution
    const std::vector<QuantLib::Real>& buckets() const { return buckets_; }
    //! Get the probabilities
    const std::vector<QuantLib::Real>& probabilities() const { return probabilities_; }
    //! Set the probabilities
    std::vector<QuantLib::Real>& probabilities() { return probabilities_; }
    //! Return the points of the distribution
    const std::vector<QuantLib::Real>& points() const { return points_; }
    //! Return the number of buckets in the distribution
    Size numberBuckets() const { return buckets_.size() - 1; }
    //! Return the cumulative probabilities of the distribution
    std::vector<QuantLib::Real> cumulativeProbabilities() const;
    //! Return 1.0 minus the cumulative probabilities of the distribution
    std::vector<QuantLib::Real> complementaryProbabilities() const;
    //! Shift all buckets and points by an additive \p shift
    void applyShift(QuantLib::Real shift);
    //! Shift all buckets and points by a multiplicative \p factor
    void applyFactor(QuantLib::Real factor);
    //! Return cumulative probability at point \p x using linear interpolation
    Real cumulativeProbability(QuantLib::Real x) const;
    //! Return inverse cumulative probability given a probability \p p using linear interpolation
    Real inverseCumulativeProbability(QuantLib::Real p) const;
    //! Utility functions
    BucketedDistribution& operator+=(const BucketedDistribution& other);
    //! Create a DiscreteDistribution from a BucketedDistribution with discrete points at midpoints of the buckets
    DiscreteDistribution createDiscrete() const;
    //! Erase the first \p n buckets from the distribution
    /*! \warning This may invalidate the distribution if the buckets erased do not have negligible probability.
     */
    void erase(QuantLib::Size n);
    //! Returns the index of the bucket containing value
    QuantLib::Size bucket(QuantLib::Real value) const;

private:
    //! Common code used in the constructor
    void init(QuantLib::Real min, QuantLib::Real max, QuantLib::Size numberBuckets);
    //! Vector of numbers denoting buckets of distribution
    std::vector<QuantLib::Real> buckets_;
    //! Vector of numbers denoting probabilities in each bucket
    std::vector<QuantLib::Real> probabilities_;
    std::vector<QuantLib::Real> points_;
    std::vector<QuantLib::Real> previousProbabilities_;
    std::vector<QuantLib::Real> previousPoints_;
    // Probabilities in a bucket less than this value are considered negligible
    // FIXME: need to find a better way to deal with very small probabilities
    const static QuantLib::Real minProbability_;
};

//! Sum probabilities in two bucketed distributions with equal buckets
BucketedDistribution operator+(const BucketedDistribution& lhs, const BucketedDistribution& rhs);

//! Multiply probabilities in bucketed distribution by \p factor
BucketedDistribution operator*(QuantLib::Real factor, const BucketedDistribution& rhs);
BucketedDistribution operator*(const BucketedDistribution& lhs, QuantLib::Real factor);
} // namespace QuantExt

