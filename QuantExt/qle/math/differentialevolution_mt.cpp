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

#include <qle/math/differentialevolution_mt.hpp>
//#include <ored/utilities/log.hpp>

#include <boost/make_shared.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <thread>
#include <iostream>

namespace QuantExt {
using namespace QuantLib;
    
    namespace {

        struct sort_by_cost {
            bool operator()(const DifferentialEvolution_MT::Candidate& left,
                            const DifferentialEvolution_MT::Candidate& right) {
                return left.cost < right.cost;
            }
        };

    }

    bool DifferentialEvolution_MT::checkMaxTime() const {
        if(maxTime_ == "")
            return false;
        QL_REQUIRE(maxTime_.length() == 15, "maxTime (" << maxTime_ << ") must have format YYYYMMDDTHHMMSS");
        std::string ts = to_iso_string(boost::posix_time::microsec_clock::local_time()).substr(0, 15);
        return ts > maxTime_;
    }

    EndCriteria::Type DifferentialEvolution_MT::minimize(Problem_MT& p, const EndCriteria& endCriteria) {
        EndCriteria::Type ecType;

        upperBound_ = p.constraint().upperBound(p.currentValue());
        lowerBound_ = p.constraint().lowerBound(p.currentValue());
        currGenSizeWeights_ = Array(configuration().populationMembers,
                                    configuration().stepsizeWeight);
        currGenCrossover_ = Array(configuration().populationMembers,
                                  configuration().crossoverProbability);

        std::vector<Candidate> population(configuration().populationMembers,
                                          Candidate(p.currentValue().size()));
        fillInitialPopulation(population, p);

        std::partial_sort(population.begin(), population.begin() + 1, population.end(),
                          sort_by_cost());
        bestMemberEver_ = population.front();
        Real fxOld = population.front().cost;
        Size iteration = 0, stationaryPointIteration = 0;

        // main loop - calculate consecutive emerging populations
        Size generation = 0;
        while (!endCriteria.checkMaxIterations(iteration++, ecType) && !checkMaxTime()) {
            //std::clog << "calculate generation #" << ++generation << " " << std::flush;
            generation++;
            calculateNextGeneration(population, p.costFunctions());
            std::partial_sort(population.begin(), population.begin() + 1, population.end(),
                              sort_by_cost());
            if (population.front().cost < bestMemberEver_.cost)
                bestMemberEver_ = population.front();
            Real fxNew = population.front().cost;
            //std::clog << " bestMemberEver = " << bestMemberEver_.cost << " " << fxNew << std::endl;
            //Move the class to OREAnalytics if we want to keep logging
            //LOG("Generation " << generation << " best " << bestMemberEver_.cost << " current " << fxNew);
            if (endCriteria.checkStationaryFunctionValue(fxOld, fxNew, stationaryPointIteration,
                                                         ecType))
                break;
            // debug, exit after 10 iterations
            // if(generation >= 10)
            //     break;
            // end debug
            fxOld = fxNew;
        };
        p.setCurrentValue(bestMemberEver_.values);
        p.setFunctionValue(bestMemberEver_.cost);
        if(checkMaxTime())
            ecType = EndCriteria::Type::Unknown;
        return ecType;
    }

    void DifferentialEvolution_MT::calculateNextGeneration(
                                     std::vector<Candidate>& population,
                                     const std::vector<boost::shared_ptr<CostFunction>>& costFunctions) const {

        std::vector<Candidate> mirrorPopulation;
        std::vector<Candidate> oldPopulation = population;

        switch (configuration().strategy) {

          case DifferentialEvolution::Rand1Standard: {
              std::random_shuffle(population.begin(), population.end());
              std::vector<Candidate> shuffledPop1 = population;
              std::random_shuffle(population.begin(), population.end());
              std::vector<Candidate> shuffledPop2 = population;
              std::random_shuffle(population.begin(), population.end());
              mirrorPopulation = shuffledPop1;

              for (Size popIter = 0; popIter < population.size(); popIter++) {
                  population[popIter].values = population[popIter].values
                      + configuration().stepsizeWeight
                      * (shuffledPop1[popIter].values - shuffledPop2[popIter].values);
              }
          }
            break;

          case DifferentialEvolution::BestMemberWithJitter: {
              std::random_shuffle(population.begin(), population.end());
              std::vector<Candidate> shuffledPop1 = population;
              std::random_shuffle(population.begin(), population.end());
              Array jitter(population[0].values.size(), 0.0);

              for (Size popIter = 0; popIter < population.size(); popIter++) {
                  for (Size jitterIter = 0; jitterIter < jitter.size(); jitterIter++) {
                      jitter[jitterIter] = rng_.nextReal();
                  }
                  population[popIter].values = bestMemberEver_.values
                      + (shuffledPop1[popIter].values - population[popIter].values)
                      * (0.0001 * jitter + configuration().stepsizeWeight);
              }
              mirrorPopulation = std::vector<Candidate>(population.size(),
                                                        bestMemberEver_);
          }
            break;

          case DifferentialEvolution::CurrentToBest2Diffs: {
              std::random_shuffle(population.begin(), population.end());
              std::vector<Candidate> shuffledPop1 = population;
              std::random_shuffle(population.begin(), population.end());

              for (Size popIter = 0; popIter < population.size(); popIter++) {
                  population[popIter].values = oldPopulation[popIter].values
                      + configuration().stepsizeWeight
                      * (bestMemberEver_.values - oldPopulation[popIter].values)
                      + configuration().stepsizeWeight
                      * (population[popIter].values - shuffledPop1[popIter].values);
              }
              mirrorPopulation = shuffledPop1;
          }
            break;

          case DifferentialEvolution::Rand1DiffWithPerVectorDither: {
              std::random_shuffle(population.begin(), population.end());
              std::vector<Candidate> shuffledPop1 = population;
              std::random_shuffle(population.begin(), population.end());
              std::vector<Candidate> shuffledPop2 = population;
              std::random_shuffle(population.begin(), population.end());
              mirrorPopulation = shuffledPop1;
              Array FWeight = Array(population.front().values.size(), 0.0);
              for (Size fwIter = 0; fwIter < FWeight.size(); fwIter++)
                  FWeight[fwIter] = (1.0 - configuration().stepsizeWeight)
                      * rng_.nextReal() + configuration().stepsizeWeight;
              for (Size popIter = 0; popIter < population.size(); popIter++) {
                  population[popIter].values = population[popIter].values
                      + FWeight * (shuffledPop1[popIter].values - shuffledPop2[popIter].values);
              }
          }
            break;

          case DifferentialEvolution::Rand1DiffWithDither: {
              std::random_shuffle(population.begin(), population.end());
              std::vector<Candidate> shuffledPop1 = population;
              std::random_shuffle(population.begin(), population.end());
              std::vector<Candidate> shuffledPop2 = population;
              std::random_shuffle(population.begin(), population.end());
              mirrorPopulation = shuffledPop1;
              Real FWeight = (1.0 - configuration().stepsizeWeight) * rng_.nextReal()
                  + configuration().stepsizeWeight;
              for (Size popIter = 0; popIter < population.size(); popIter++) {
                  population[popIter].values = population[popIter].values
                      + FWeight * (shuffledPop1[popIter].values - shuffledPop2[popIter].values);
              }
          }
            break;

          case DifferentialEvolution::EitherOrWithOptimalRecombination: {
              std::random_shuffle(population.begin(), population.end());
              std::vector<Candidate> shuffledPop1 = population;
              std::random_shuffle(population.begin(), population.end());
              std::vector<Candidate> shuffledPop2 = population;
              std::random_shuffle(population.begin(), population.end());
              mirrorPopulation = shuffledPop1;
              Real probFWeight = 0.5;
              if (rng_.nextReal() < probFWeight) {
                  for (Size popIter = 0; popIter < population.size(); popIter++) {
                      population[popIter].values = oldPopulation[popIter].values
                          + configuration().stepsizeWeight
                          * (shuffledPop1[popIter].values - shuffledPop2[popIter].values);
                  }
              } else {
                  Real K = 0.5 * (configuration().stepsizeWeight + 1); // invariant with respect to probFWeight used
                  for (Size popIter = 0; popIter < population.size(); popIter++) {
                      population[popIter].values = oldPopulation[popIter].values
                          + K
                          * (shuffledPop1[popIter].values - shuffledPop2[popIter].values
                             - 2.0 * population[popIter].values);
                  }
              }
          }
            break;

          case DifferentialEvolution::Rand1SelfadaptiveWithRotation: {
              std::random_shuffle(population.begin(), population.end());
              std::vector<Candidate> shuffledPop1 = population;
              std::random_shuffle(population.begin(), population.end());
              std::vector<Candidate> shuffledPop2 = population;
              std::random_shuffle(population.begin(), population.end());
              mirrorPopulation = shuffledPop1;

              adaptSizeWeights();

              for (Size popIter = 0; popIter < population.size(); popIter++) {
                  if (rng_.nextReal() < 0.1){
                      population[popIter].values = rotateArray(bestMemberEver_.values);
                  }else {
                      population[popIter].values = bestMemberEver_.values
                          + currGenSizeWeights_[popIter]
                          * (shuffledPop1[popIter].values - shuffledPop2[popIter].values);
                  }
              }
          }
            break;

          default:
            QL_FAIL("Unknown strategy ("
                    << Integer(configuration().strategy) << ")");
        }
        // in order to avoid unnecessary copying we use the same population object for mutants
        crossover(oldPopulation, population, population, mirrorPopulation,
                  costFunctions);
    }

    void DifferentialEvolution_MT::crossover(
                               const std::vector<Candidate>& oldPopulation,
                               std::vector<Candidate>& population,
                               const std::vector<Candidate>& mutantPopulation,
                               const std::vector<Candidate>& mirrorPopulation,
                               const std::vector<boost::shared_ptr<CostFunction>>& costFunctions) const {

        if (configuration().crossoverIsAdaptive) {
            adaptCrossover();
        }

        Array mutationProbabilities = getMutationProbabilities(population);

        std::vector<Array> crossoverMask(population.size(),
                                         Array(population.front().values.size(), 1.0));
        std::vector<Array> invCrossoverMask = crossoverMask;
        getCrossoverMask(crossoverMask, invCrossoverMask, mutationProbabilities);

        // crossover of the old and mutant population

        for (Size popIter = 0; popIter < population.size(); popIter++) {
            population[popIter].values = oldPopulation[popIter].values * invCrossoverMask[popIter]
                + mutantPopulation[popIter].values * crossoverMask[popIter];
            // immediately apply bounds if specified
            if (configuration().applyBounds) {
                for (Size memIter = 0; memIter < population[popIter].values.size(); memIter++) {
                    if (population[popIter].values[memIter] > upperBound_[memIter])
                        population[popIter].values[memIter] = upperBound_[memIter]
                            + rng_.nextReal()
                            * (mirrorPopulation[popIter].values[memIter]
                               - upperBound_[memIter]);
                    if (population[popIter].values[memIter] < lowerBound_[memIter])
                        population[popIter].values[memIter] = lowerBound_[memIter]
                            + rng_.nextReal()
                            * (mirrorPopulation[popIter].values[memIter]
                               - lowerBound_[memIter]);
                }
            }
        }

        Size threads = costFunctions.size(); // std::min<Size>(std::thread::hardware_concurrency(), costFunctions.size());
        QL_REQUIRE(threads > 0, "DifferentialEvolution_MT: number of available threads is zero");

        std::vector<Size> chunks(threads, std::max<Size>(population.size() / threads, 1));
        int rest = population.size() - threads * chunks[0];
        do {
            Size i=0;
            while(i<chunks.size() && rest > 0) {
                chunks[i]++;
                rest--;
                ++i;
            }
        } while(rest > 0);

        std::vector<boost::shared_ptr<std::thread>> workers(threads);

        Size start=0, end, prevChunkSize, countChunkSize=0;
        for(Size thread = 0; thread < threads; ++thread) {
            end = std::min(start + chunks[thread], population.size());
            if(thread == 0 || (end-start == prevChunkSize)) {
                countChunkSize++;
            }
            else {
                //std::clog << countChunkSize << "x" << prevChunkSize << " ";
                countChunkSize = 1;
            }
            prevChunkSize = end - start;
            Worker worker(population, start, end, costFunctions[thread]);
            workers[thread] = boost::make_shared<std::thread>(worker);
            start = end;
        }
        // if(countChunkSize > 0)
        //     std::clog << countChunkSize << "x" << prevChunkSize;
        // std::clog << std::flush;

        for(Size thread = 0;thread < threads;++thread) {
            workers[thread]->join();
        }
    }

    void DifferentialEvolution_MT::Worker::operator()() {
        for (Size popIter = start; popIter < end; popIter++) {
            // evaluate objective function as soon as possible to avoid unnecessary loops
            try {
                population[popIter].cost = costFunction->value(population[popIter].values);
            } catch (Error&) {
                population[popIter].cost = QL_MAX_REAL;
            }
        }
    }

    void DifferentialEvolution_MT::getCrossoverMask(
                                  std::vector<Array> & crossoverMask,
                                  std::vector<Array> & invCrossoverMask,
                                  const Array & mutationProbabilities) const {
        for (Size cmIter = 0; cmIter < crossoverMask.size(); cmIter++) {
            for (Size memIter = 0; memIter < crossoverMask[cmIter].size(); memIter++) {
                if (rng_.nextReal() < mutationProbabilities[cmIter]) {
                    invCrossoverMask[cmIter][memIter] = 0.0;
                } else {
                    crossoverMask[cmIter][memIter] = 0.0;
                }
            }
        }
    }

    Array DifferentialEvolution_MT::getMutationProbabilities(
                            const std::vector<Candidate> & population) const {
        Array mutationProbabilities = currGenCrossover_;
        switch (configuration().crossoverType) {
          case DifferentialEvolution::Normal:
            break;
          case DifferentialEvolution::Binomial:
            mutationProbabilities = currGenCrossover_
                * (1.0 - 1.0 / population.front().values.size())
                + 1.0 / population.front().values.size();
            break;
          case DifferentialEvolution::Exponential:
            for (Size coIter = 0;coIter< currGenCrossover_.size(); coIter++){
                mutationProbabilities[coIter] =
                    (1.0 - std::pow(currGenCrossover_[coIter],
                                    (int) population.front().values.size()))
                    / (population.front().values.size()
                       * (1.0 - currGenCrossover_[coIter]));
            }
            break;
          default:
            QL_FAIL("Unknown crossover type ("
                    << Integer(configuration().crossoverType) << ")");
            break;
        }
        return mutationProbabilities;
    }

    Array DifferentialEvolution_MT::rotateArray(Array a) const {
        std::random_shuffle(a.begin(), a.end());
        return a;
    }

    void DifferentialEvolution_MT::adaptSizeWeights() const {
        // [=Fl & =Fu] respectively see Brest, J. et al., 2006,
        // "Self-Adapting Control Parameters in Differential
        // Evolution"
        Real sizeWeightLowerBound = 0.1, sizeWeightUpperBound = 0.9;
         // [=tau1] A Comparative Study on Numerical Benchmark
         // Problems." page 649 for reference
        Real sizeWeightChangeProb = 0.1;
        for (Size coIter = 0;coIter < currGenSizeWeights_.size(); coIter++){
            if (rng_.nextReal() < sizeWeightChangeProb)
                currGenSizeWeights_[coIter] = sizeWeightLowerBound + rng_.nextReal() * sizeWeightUpperBound;
        }
    }

    void DifferentialEvolution_MT::adaptCrossover() const {
        Real crossoverChangeProb = 0.1; // [=tau2]
        for (Size coIter = 0;coIter < currGenCrossover_.size(); coIter++){
            if (rng_.nextReal() < crossoverChangeProb)
                currGenCrossover_[coIter] = rng_.nextReal();
        }
    }

    void DifferentialEvolution_MT::fillInitialPopulation(
                                          std::vector<Candidate> & population,
                                          const Problem_MT& p) const {

        // use initial values provided by the user
        population.front().values = p.currentValue();
        population.front().cost = p.costFunction(0)->value(population.front().values);
        // rest of the initial population is random
        for (Size j = 1; j < population.size(); ++j) {
            for (Size i = 0; i < p.currentValue().size(); ++i) {
                Real l = lowerBound_[i], u = upperBound_[i];
                population[j].values[i] = l + (u-l)*rng_.nextReal();
            }
            population[j].cost = p.costFunction(0)->value(population[j].values);
        }
    }

}

