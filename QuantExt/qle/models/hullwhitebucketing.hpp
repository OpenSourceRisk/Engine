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

/*! \file qle/models/hullwhitebucketing.hpp
    \brief probability bucketing as in Valuation of a CDO and an nth to Default CDS
           without Monte Carlo Simulation, Appdx. B
    \ingroup models
*/

#pragma once

#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>

namespace QuantExt {
using namespace QuantLib;

class Bucketing {
public:
    /*! buckets are (-QL_MAX_REAL, b1), [b1, b2), [b2,b3), ... , [b_{n-1}, b_n), [b_n, +QL_MAX_REAL) */
    template <class I> Bucketing(I bucketsBegin, I bucketsEnd);
    /*! there are n+2 buckets constructed, lb = lower bound, ub = upper bound, h = (ub - lb) / n
      buckets are (-QL_MAX_REAL, lb), [lb, lb+h), [lb+h, lb+2h), ... , [lb+(n-1)h, ub), [ub,+QL_MAX_REAL) */
    Bucketing(const Real lowerBound, const Real upperBound, const Size n);
    const std::vector<Real>& upperBucketBound() const { return buckets_; }
    Size index(const Real x) const;
    Size buckets() const { return buckets_.size(); }

protected:
    void initBuckets();
    std::vector<Real> buckets_;
    bool uniformBuckets_ = false;
    Real lowerBound_, upperBound_, h_;
}; // Bucketing

class HullWhiteBucketing : public Bucketing {
public:
    template <class I> HullWhiteBucketing(I bucketsBegin, I bucketsEnd) : Bucketing(bucketsBegin, bucketsEnd) {}
    HullWhiteBucketing(const Real lowerBound, const Real upperBound, const Size n)
        : Bucketing(lowerBound, upperBound, n) {}

    /* losses might be negative, warning: pd and losses container sizes must match, this is not checked */
    template <class I1, class I2> void compute(I1 pdBegin, I1 pdEnd, I2 lossesBegin);
    /* Each p is itself a vector p[0], p[1], ... , p[n], with a corresponding
       vector of losses          l[0], l[1], ... , l[n]; now a loss is realised
       with probability P = p[0] + ... + p[n] and conditional on this, l[0] is realised
       with probability p[0] / P, l[1] with probability p[1] / P, ..., l[n] with probability p[n] / P,
       Warning: container sizes and p, l vector sizes must match, this is not checked */
    template <class I1, class I2> void computeMultiState(I1 pBegin, I1 pEnd, I2 lossesBegin);

    const Array& probability() const { return p_; }
    const Array& averageLoss() const { return A_; }

private:
    void init_p_A();
    void finalize_p_A();
    Array p_, A_;
}; // HullWhiteBucketing

// definitions

template <class I> Bucketing::Bucketing(I bucketsBegin, I bucketsEnd) : buckets_(bucketsBegin, bucketsEnd) {
    initBuckets();
}

template <class I1, class I2> void HullWhiteBucketing::compute(I1 pdBegin, I1 pdEnd, I2 lossesBegin) {
    init_p_A();
    Array A2(A_.size()), p2(p_.size());

    auto it2 = lossesBegin;
    for (auto it = pdBegin; it != pdEnd; ++it, ++it2) {
        if (QuantLib::close_enough(*it, 0.0) || QuantLib::close_enough(*it2, 0.0))
            continue;
        std::fill(A2.begin(), A2.end(), 0.0);
        std::fill(p2.begin(), p2.end(), 0.0);
        for (Size k = 0; k < buckets_.size(); ++k) {
            if (QuantLib::close_enough(p_[k], 0.0))
                continue;
            Size t = index(A_[k] / p_[k] + *it2);
            p2[k] += p_[k] * (1.0 - (*it));
            A2[k] += A_[k] * (1.0 - (*it));
            p2[t] += p_[k] * (*it);
            A2[t] += (*it) * (A_[k] + p_[k] * (*it2));
        }
        std::copy(A2.begin(), A2.end(), A_.begin());
        std::copy(p2.begin(), p2.end(), p_.begin());
    }

    finalize_p_A();

} // compute

template <class I1, class I2> void HullWhiteBucketing::computeMultiState(I1 pBegin, I1 pEnd, I2 lossesBegin) {
    init_p_A();
    Array A2(A_.size()), p2(p_.size());

    auto it2 = lossesBegin;
    for (auto it = pBegin; it != pEnd; ++it, ++it2) {
        std::fill(A2.begin(), A2.end(), 0.0);
        std::fill(p2.begin(), p2.end(), 0.0);
        for (Size k = 0; k < buckets_.size(); ++k) {
            Real q = 0.0;
            auto it2_i = (*it2).begin();
            for (auto it_i = (*it).begin(), itend = (*it).end(); it_i != itend; ++it_i, ++it2_i) {
                if (QuantLib::close_enough(*it_i, 0.0) || QuantLib::close_enough(*it2_i, 0.0))
                    continue;
                Size t = index(A_[k] / p_[k] + *it2_i);
                p2[t] += p_[k] * (*it_i);
                A2[t] += (*it_i) * (A_[k] + p_[k] * (*it2_i));
                q += *it_i;
            }
            p2[k] += p_[k] * (1.0 - q);
            A2[k] += A_[k] * (1.0 - q);
        }
        std::copy(A2.begin(), A2.end(), A_.begin());
        std::copy(p2.begin(), p2.end(), p_.begin());
    } // for it

    finalize_p_A();

} // computeMultiState

} // namespace QuantExt
