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

/*! \file interpolatedvariatemultipathgenerator.hpp
    \brief multi path generator using interpolated variates
    \ingroup methods
*/

#pragma once

#include <qle/math/randomvariable.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>

namespace QuantExt {

class InterpolatedVariateMultiPathGenerator : public MultiPathGeneratorBase {
public:
    enum class Scheme { Sequential, Cumulative };
    InterpolatedVariateMultiPathGenerator(
        const boost::shared_ptr<StochasticProcess>& process, const std::vector<Real>& interpolatedVariateTimes,
        const std::vector<Real>& originalVariateTimes,
        const std::vector<std::vector<QuantExt::RandomVariable>>* interpolatedVariates,
        const Scheme& scheme = Scheme::Cumulative);
    const Sample<MultiPath>& next() const override;
    void reset() override;

private:
    const boost::shared_ptr<StochasticProcess> process_;
    const std::vector<Real> interpolatedVariateTimes_, originalVariateTimes_;
    const std::vector<std::vector<QuantExt::RandomVariable>>* interpolatedVariates_;
    const Scheme scheme_;

    Size samples_;
    std::vector<bool> isCoarseTime_;
    mutable Size currentPath_;
    mutable Sample<MultiPath> next_;
};

} // namespace QuantExt
