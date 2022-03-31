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
*/

/*! \file differentialevolution_mt.hpp
    \brief Multithreaded Differential Evolution optimization method
*/

#ifndef quantext_optimization_differential_evolution_mt_hpp
#define quantext_optimization_differential_evolution_mt_hpp

#include <qle/math/problem_mt.hpp>
#include <qle/math/method_mt.hpp>

#include <ql/math/optimization/differentialevolution.hpp>
#include <ql/math/optimization/constraint.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>

namespace QuantExt {
using namespace QuantLib;

    //! Differential Evolution configuration object
    /*! The algorithm and strategy names are taken from here:

        Price, K., Storn, R., 1997. Differential Evolution -
        A Simple and Efficient Heuristic for Global Optimization
        over Continuous Spaces.
        Journal of Global Optimization, Kluwer Academic Publishers,
        1997, Vol. 11, pp. 341 - 359.

        There are seven basic strategies for creating mutant population
        currently implemented. Three basic crossover types are also
        available.

        Future development:
        1) base element type to be extracted
        2) L differences to be used instead of fixed number
        3) various weights distributions for the differences (dither etc.)
        4) printFullInfo parameter usage to track the algorithm

        \warning This was reported to fail tests on Mac OS X 10.8.4.
    */


    //! %OptimizationMethod using Differential Evolution algorithm
    /*! \ingroup optimizers */
    class DifferentialEvolution_MT: public OptimizationMethod_MT {
      public:
        typedef DifferentialEvolution::Strategy Strategy;
        typedef DifferentialEvolution::CrossoverType CrossoverType;

        typedef DifferentialEvolution::Candidate Candidate;
        typedef DifferentialEvolution::Configuration Configuration;

        DifferentialEvolution_MT(Configuration configuration = Configuration(), std::string maxTime = "")
        : configuration_(configuration), maxTime_(maxTime), rng_(configuration.seed) {}

        virtual EndCriteria::Type minimize(Problem_MT& p,
                                           const EndCriteria& endCriteria) override;

        const Configuration& configuration() const {
            return configuration_;
        }

      private:
        // MT worker class
        struct Worker {
            Worker(std::vector<Candidate>& p, const Size s, const Size e,
                   const boost::shared_ptr<CostFunction> c)
                : population(p), start(s), end(e), costFunction(c) {}
            void operator()();
            std::vector<Candidate>& population;
            const Size start, end;
            const boost::shared_ptr<CostFunction> costFunction;
        };

        Configuration configuration_;
        std::string maxTime_;
        Array upperBound_, lowerBound_;
        mutable Array currGenSizeWeights_, currGenCrossover_;
        Candidate bestMemberEver_;
        MersenneTwisterUniformRng rng_;

        bool checkMaxTime() const;

        void fillInitialPopulation(std::vector<Candidate>& population,
                                   const Problem_MT& p) const;

        void getCrossoverMask(std::vector<Array>& crossoverMask,
                              std::vector<Array>& invCrossoverMask,
                              const Array& mutationProbabilities) const;

        Array getMutationProbabilities(
                              const std::vector<Candidate>& population) const;

        void adaptSizeWeights() const;

        void adaptCrossover() const;

        void calculateNextGeneration(std::vector<Candidate>& population,
                                     const std::vector<boost::shared_ptr<CostFunction>>& costFunctions) const;

        Array rotateArray(Array inputArray) const;

        void crossover(const std::vector<Candidate>& oldPopulation,
                       std::vector<Candidate> & population,
                       const std::vector<Candidate>& mutantPopulation,
                       const std::vector<Candidate>& mirrorPopulation,
                       const std::vector<boost::shared_ptr<CostFunction>>& costFunctions) const;
    };

}

#endif
