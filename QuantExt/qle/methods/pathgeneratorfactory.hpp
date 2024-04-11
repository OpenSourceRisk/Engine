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

/*! \file pathgeneratorfactory.hpp
    \brief base class and standard implementation for path generator factories
    \ingroup methods
*/

#pragma once

#include <qle/methods/multipathgeneratorbase.hpp>

namespace QuantExt {

//! Base class for path generator factories
class PathGeneratorFactory {
public:
    virtual ~PathGeneratorFactory() {}
    virtual QuantLib::ext::shared_ptr<MultiPathGeneratorBase> build(const SequenceType s,
                                                            const QuantLib::ext::shared_ptr<StochasticProcess>& process,
                                                            const TimeGrid& timeGrid, const BigNatural seed,
                                                            const SobolBrownianGenerator::Ordering ordering,
                                                            const SobolRsg::DirectionIntegers directionIntegers) = 0;
};

//! Standard implementation for path generator factory
class MultiPathGeneratorFactory : public PathGeneratorFactory {
public:
    QuantLib::ext::shared_ptr<MultiPathGeneratorBase> build(const SequenceType s,
                                                    const QuantLib::ext::shared_ptr<StochasticProcess>& process,
                                                    const TimeGrid& timeGrid, const BigNatural seed,
                                                    const SobolBrownianGenerator::Ordering ordering,
                                                    const SobolRsg::DirectionIntegers directionIntegers) override {
        return makeMultiPathGenerator(s, process, timeGrid, seed, ordering, directionIntegers);
    }
};

} // namespace QuantExt
