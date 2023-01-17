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

#include <qle/models/hullwhitebucketing.hpp>
#include <ql/math/array.hpp>

#include <algorithm>

using namespace QuantLib;

namespace QuantExt {

Bucketing::Bucketing(const Real lowerBound, const Real upperBound, const Size n) {
    buckets_.resize(n + 1);
    Real h = (upperBound - lowerBound) / static_cast<Real>(n);
    for (Size i = 0; i <= n; ++i)
        buckets_[i] = lowerBound + static_cast<Real>(i) * h;
    initBuckets();
    uniformBuckets_ = true;
    lowerBound_ = lowerBound;
    upperBound_ = upperBound;
    h_ = h;
}

Size Bucketing::index(const Real x) const {
    if (uniformBuckets_) {
        return std::min<Size>(buckets_.size() - 1,
                              std::max(0, static_cast<int>(std::floor((x - lowerBound_) / h_) + 1)));
    } else {
        return std::upper_bound(buckets_.begin(), buckets_.end(), x) - buckets_.begin();
    }
}

void Bucketing::initBuckets() {
    QL_REQUIRE(!buckets_.empty(), "Bucketing::initBuckets() no buckets given");
    QL_REQUIRE(std::is_sorted(buckets_.begin(), buckets_.end()), "buckets must be sorted");
    if (!close_enough(buckets_.back(), QL_MAX_REAL)) {
        buckets_.insert(buckets_.end(), QL_MAX_REAL);
    }
}

void HullWhiteBucketing::init_p_A() {
    Size N = buckets_.size();
    p_ = Array(N, 0);
    A_ = Array(N, 0);
    Size zeroIdx = index(0.0);
    p_[zeroIdx] = 1.0;
    A_[zeroIdx] = 0.0;
}

void HullWhiteBucketing::finalize_p_A() {
    Size N = buckets_.size();
    for (Size i = 0; i < N; ++i) {
        if (QuantLib::close_enough(p_[i], 0.0)) {
            // the probability for a bucket is zero => fill average with midpoint (endpoints)
            if (i == 0) {
                A_[i] = buckets_.front();
            } else if (i == N - 1) {
                A_[i] = buckets_[N - 2];
            } else {
                A_[i] = 0.5 * (buckets_[i - 1] + buckets_[i]);
            }
        } else {
            // otherwise normalize the bucket to represent the conditional aveage for this bucket
            A_[i] /= p_[i];
        }
    }
}

} // namespace QuantExt
