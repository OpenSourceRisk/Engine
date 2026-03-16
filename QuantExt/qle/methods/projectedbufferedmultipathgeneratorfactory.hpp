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

/*! \file projectedbufferedmultipathgeneratorfactory.hpp
    \brief path generator factory that builds a projected path generator
    \ingroup methods
*/

#pragma once

#include <qle/methods/multipathvariategenerator.hpp>
#include <qle/methods/projectedbufferedmultipathgenerator.hpp>

#include <qle/methods/pathgeneratorfactory.hpp>

namespace QuantExt {

class ProjectedBufferedMultiPathGeneratorFactory : public PathGeneratorFactory {
public:
    ProjectedBufferedMultiPathGeneratorFactory(
        const std::vector<Size>& stateProcessProjection,
        const QuantLib::ext::shared_ptr<std::vector<std::vector<QuantLib::Path>>>& bufferedPaths)
        : stateProcessProjection_(stateProcessProjection), bufferedPaths_(bufferedPaths) {}
    QuantLib::ext::shared_ptr<MultiPathGeneratorBase> build(const SequenceType s,
                                                    const QuantLib::ext::shared_ptr<StochasticProcess>& process,
                                                    const TimeGrid& timeGrid, const BigNatural seed,
                                                    const SobolBrownianGenerator::Ordering ordering,
                                                    const SobolRsg::DirectionIntegers directionIntegers) override {
        return QuantLib::ext::make_shared<ProjectedBufferedMultiPathGenerator>(stateProcessProjection_, bufferedPaths_);
    }

private:
    const std::vector<Size> stateProcessProjection_;
    const QuantLib::ext::shared_ptr<std::vector<std::vector<QuantLib::Path>>> bufferedPaths_;
};

} // namespace QuantExt
