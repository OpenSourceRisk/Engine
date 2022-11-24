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

#include <qle/methods/interpolatedvariatemultipathgenerator.hpp>

namespace QuantExt {

InterpolatedVariateMultiPathGenerator::InterpolatedVariateMultiPathGenerator(
    const boost::shared_ptr<StochasticProcess>& process, const std::vector<Real>& interpolatedVariateTimes,
    const std::vector<Real>& originalVariateTimes,
    const std::vector<std::vector<QuantExt::RandomVariable>>* interpolatedVariates, const Scheme& scheme)
    : process_(process), interpolatedVariateTimes_(interpolatedVariateTimes),
      originalVariateTimes_(originalVariateTimes), interpolatedVariates_(interpolatedVariates), scheme_(scheme),
      next_(MultiPath(process->size(), TimeGrid(interpolatedVariateTimes.begin(), interpolatedVariateTimes.end())),
            1.0) {

    QL_REQUIRE(interpolatedVariates_, "interpolated variates are null");

    QL_REQUIRE(!interpolatedVariateTimes_.empty(), "interpolated variate times are empty");

    QL_REQUIRE(interpolatedVariateTimes_.size() == interpolatedVariates_->size(),
               "interpolated variate times (" << interpolatedVariateTimes_.size() << ") must match variates size ("
                                              << interpolatedVariates_->size());

    for (Size i = 0; i < interpolatedVariateTimes_.size(); ++i) {
        QL_REQUIRE(process->factors() == (*interpolatedVariates_)[i].size(),
                   "process factor size (" << process->factors() << ") must match variates dimension at time step " << i
                                           << " (" << (*interpolatedVariates_)[i].size());
    }

    samples_ = interpolatedVariates_->front().front().size();

    for (Size i = 0; i < interpolatedVariateTimes_.size(); ++i) {
        for (Size j = 0; j < process->factors(); ++j) {
            QL_REQUIRE((*interpolatedVariates_)[i][j].size() == samples_, "inconsistent sample at time step "
                                                                              << i << " factor " << j << ": got "
                                                                              << (*interpolatedVariates_)[i][j].size()
                                                                              << ", expected " << samples_);
        }
    }

    isCoarseTime_ = std::vector<bool>(interpolatedVariateTimes_.size(), false);
    for (Size i = 0; i < originalVariateTimes_.size(); ++i) {
        Real t = originalVariateTimes_[i];
        auto intp = std::find_if(interpolatedVariateTimes_.begin(), interpolatedVariateTimes_.end(),
                                 [&t](Real s) { return QuantLib::close_enough(s, t); });
        QL_REQUIRE(intp != interpolatedVariateTimes_.end(),
                   "could not find original time (" << t << ") in interpolated variate times vector");
        isCoarseTime_[std::distance(interpolatedVariateTimes_.begin(), intp)] = true;
    }

    reset();
}

const Sample<MultiPath>& InterpolatedVariateMultiPathGenerator::next() const {
    QL_REQUIRE(currentPath_ < samples_, "InterpolatedVariateMultiPathGenerator::next(): samples ("
                                            << samples_ << ") exhausted, can not generate path");
    Size dim = process_->factors();
    Size stateSize = process_->size();
    Array dw(dim, 0.0);
    Array state = process_->initialValues(), state0 = state;
    Real lastTime = 0.0;
    for (Size s = 0; s < stateSize; ++s)
        next_.value[s][0] = state[s];
    for (Size j = 0; j < interpolatedVariateTimes_.size(); ++j) {
        if (scheme_ == Scheme::Sequential) {
            // Sequential scheme
            for (Size d = 0; d < dim; ++d)
                dw[d] = (*interpolatedVariates_)[j][d][currentPath_];
            state = process_->evolve(lastTime, state, interpolatedVariateTimes_[j] - lastTime, dw);
            lastTime = interpolatedVariateTimes_[j];
        } else {
            // Cumulative scheme
            for (Size d = 0; d < dim; ++d) {
                dw[d] += (*interpolatedVariates_)[j][d][currentPath_] *
                         std::sqrt(interpolatedVariateTimes_[j] - (j == 0 ? 0.0 : interpolatedVariateTimes_[j - 1]));
            }
            state = process_->evolve(lastTime, state0, interpolatedVariateTimes_[j] - lastTime,
                                     dw / std::sqrt(static_cast<double>(interpolatedVariateTimes_[j] - lastTime)));
            if (isCoarseTime_[j]) {
                std::fill(dw.begin(), dw.end(), 0.0);
                lastTime = interpolatedVariateTimes_[j];
                state0 = state;
            }
        }
        for (Size s = 0; s < stateSize; ++s) {
            next_.value[s][j + 1] = state[s];
        }
    }
    ++currentPath_;
    return next_;
}

void InterpolatedVariateMultiPathGenerator::reset() { currentPath_ = 0; }

} // namespace QuantExt
