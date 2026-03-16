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

#include <qle/methods/projectedbufferedmultipathgenerator.hpp>

namespace QuantExt {

ProjectedBufferedMultiPathGenerator::ProjectedBufferedMultiPathGenerator(
    const std::vector<Size>& stateProcessProjection,
    const QuantLib::ext::shared_ptr<std::vector<std::vector<QuantLib::Path>>>& bufferedPaths)
    : stateProcessProjection_(stateProcessProjection), bufferedPaths_(bufferedPaths), next_(MultiPath(), 1.0) {

    QL_REQUIRE(bufferedPaths_, "ProjectedBufferedMultiPathGenerator: no buffered paths given (null)");
    QL_REQUIRE(!bufferedPaths_->empty(), "ProjectedBufferedMultiPathGenerator: at least one buffered path required");

    QL_REQUIRE(!stateProcessProjection.empty(),
               "ProjectedBufferedMultiPathGenerator: state process projection is empty");

    maxTargetIndex_ = *std::max_element(stateProcessProjection.begin(), stateProcessProjection.end());

    reset();
}

const Sample<MultiPath>& ProjectedBufferedMultiPathGenerator::next() const {
    QL_REQUIRE(currentPath_ < bufferedPaths_->size(),
               "ProjectedBufferedMultiPathGenerator: run out of paths (" << bufferedPaths_->size() << ")");

    QL_REQUIRE((*bufferedPaths_)[currentPath_].size() > maxTargetIndex_,
               "ProjectedBufferedMultiPathGenerator: buffered path at sample "
                   << currentPath_ << " has insufficient dimension (" << (*bufferedPaths_)[currentPath_].size()
                   << "), need " << (maxTargetIndex_ + 1));

    std::vector<Path> tmp;
    for (Size d = 0; d < stateProcessProjection_.size(); ++d) {
        tmp.push_back((*bufferedPaths_)[currentPath_][stateProcessProjection_[d]]);
    }

    ++currentPath_;
    next_.value = MultiPath(tmp);
    return next_;
}

void ProjectedBufferedMultiPathGenerator::reset() { currentPath_ = 0; }

} // namespace QuantExt
