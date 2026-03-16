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

#include <qle/methods/projectedvariatemultipathgenerator.hpp>

namespace QuantExt {

ProjectedVariateMultiPathGenerator::ProjectedVariateMultiPathGenerator(
    const QuantLib::ext::shared_ptr<StochasticProcess>& process, const TimeGrid& timeGrid,
    const std::vector<Size>& stateProcessProjection,
    const QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>& variateGenerator)
    : process_(process), timeGrid_(timeGrid), stateProcessProjection_(stateProcessProjection),
      variateGenerator_(variateGenerator), next_(MultiPath(process->size(), timeGrid), 1.0) {

    for (Size s = 0; s < process_->size(); ++s)
        next_.value[s][0] = process_->initialValues()[s];

    QL_REQUIRE(stateProcessProjection.size() == process->factors(),
               "ProjectedVariateMultiPathGenerator: state process projection source size ("
                   << stateProcessProjection.size() << ") does not match process factors (" << process->factors());
    QL_REQUIRE(!stateProcessProjection.empty(),
               "ProjectedVariateMultiPathGenerator: state process projection is empty");

    maxTargetIndex_ = *std::max_element(stateProcessProjection.begin(), stateProcessProjection.end());
}

const Sample<MultiPath>& ProjectedVariateMultiPathGenerator::next() const {

    Sample<std::vector<Array>> v = variateGenerator_->next();
    next_.weight = v.weight;

    QL_REQUIRE(v.value.size() == timeGrid_.size() - 1,
               "ProjectedVariateMultiPathGenerator::next(): variate generator returns "
                   << v.value.size() << " variates for " << (timeGrid_.size() - 1) << " time steps to evolve");

    QL_REQUIRE(v.value.size() == 0 || maxTargetIndex_ < v.value.front().size(),
               "ProjectedVariateMultiPathGenerator::next(): variate generator returns variate of size "
                   << v.value.front().size() << ", this is required to be > max target index (" << maxTargetIndex_
                   << ")");

    Size dim = process_->factors();
    Size stateSize = process_->size();

    Array dw(dim, 0.0);
    Array state = process_->initialValues();
    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        for (Size d = 0; d < dim; ++d)
            dw[d] = v.value[i][stateProcessProjection_[d]];
        state = process_->evolve(timeGrid_[i], state, timeGrid_.dt(i), dw);
        for (Size s = 0; s < stateSize; ++s)
            next_.value[s][i + 1] = state[s];
    }
    return next_;
}

void ProjectedVariateMultiPathGenerator::reset() { variateGenerator_->reset(); }

} // namespace QuantExt
