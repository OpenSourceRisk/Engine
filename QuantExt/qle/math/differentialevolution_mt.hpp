/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 Ralph Schreyer
 Copyright (C) 2012 Mateusz Kapturski

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
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
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.*/

/*! \file differentialevolution_mt.hpp
    \brief Multithreaded version of QL class
*/

#pragma once

#include <qle/math/method_mt.hpp>
#include <qle/math/problem_mt.hpp>

#include <ql/math/optimization/constraint.hpp>
#include <ql/math/optimization/differentialevolution.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>

namespace QuantExt {
using namespace QuantLib;

class DifferentialEvolution_MT : public OptimizationMethod_MT {
public:
    using Strategy = DifferentialEvolution::Strategy;
    using CrossoverType = DifferentialEvolution::CrossoverType;
    using Candidate = DifferentialEvolution::Candidate;
    using Configuration = DifferentialEvolution::Configuration;

    DifferentialEvolution_MT(Configuration configuration = Configuration(), std::string maxTime = "")
        : configuration_(configuration), maxTime_(maxTime), rng_(configuration.seed) {}

    virtual EndCriteria::Type minimize(Problem_MT& p, const EndCriteria& endCriteria) override;

    const Configuration& configuration() const { return configuration_; }

private:
    void updateCost(std::vector<Candidate>& population, Problem_MT& p) const;

    Configuration configuration_;
    std::string maxTime_;
    Array upperBound_, lowerBound_;
    mutable Array currGenSizeWeights_, currGenCrossover_;
    Candidate bestMemberEver_;
    MersenneTwisterUniformRng rng_;

    bool checkMaxTime() const;

    void fillInitialPopulation(std::vector<Candidate>& population, const Problem_MT& p) const;

    void getCrossoverMask(std::vector<Array>& crossoverMask, std::vector<Array>& invCrossoverMask,
                          const Array& mutationProbabilities) const;

    Array getMutationProbabilities(const std::vector<Candidate>& population) const;

    void adaptSizeWeights() const;

    void adaptCrossover() const;

    void calculateNextGeneration(std::vector<Candidate>& population, Problem_MT& p) const;

    Array rotateArray(Array inputArray) const;

    void crossover(const std::vector<Candidate>& oldPopulation, std::vector<Candidate>& population,
                   const std::vector<Candidate>& mutantPopulation, const std::vector<Candidate>& mirrorPopulation,
                   Problem_MT& p) const;
};

} // namespace QuantExt
