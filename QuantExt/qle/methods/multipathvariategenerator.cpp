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

#include <qle/methods/multipathvariategenerator.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

MultiPathVariateGeneratorBase::MultiPathVariateGeneratorBase(const Size dimension, const Size timeSteps)
    : dimension_(dimension), timeSteps_(timeSteps) {}

Sample<std::vector<Array>> MultiPathVariateGeneratorBase::next() const {

    Sample<std::vector<Real>> sequence = nextSequence();
    Sample<std::vector<Array>> result(std::vector<Array>(timeSteps_, Array(dimension_)), sequence.weight);

    for (Size i = 0; i < timeSteps_; ++i) {
        Size offset = i * dimension_;
        std::copy(sequence.value.begin() + offset, sequence.value.begin() + offset + dimension_,
                  result.value[i].begin());
    }

    return result;
}

MultiPathVariateGeneratorMersenneTwister::MultiPathVariateGeneratorMersenneTwister(const Size dimension,
                                                                                   const Size timeSteps,
                                                                                   BigNatural seed,
                                                                                   bool antitheticSampling)
    : MultiPathVariateGeneratorBase(dimension, timeSteps), seed_(seed), antitheticSampling_(antitheticSampling),
      antitheticVariate_(true) {
    reset();
}

void MultiPathVariateGeneratorMersenneTwister::reset() {
    rsg_ = boost::make_shared<
        InverseCumulativeRsg<RandomSequenceGenerator<MersenneTwisterUniformRng>, InverseCumulativeNormal>>(
        RandomSequenceGenerator<MersenneTwisterUniformRng>(dimension_ * timeSteps_, MersenneTwisterUniformRng(seed_)),
        InverseCumulativeNormal());
    antitheticVariate_ = true;
}

Sample<std::vector<Real>> MultiPathVariateGeneratorMersenneTwister::nextSequence() const {
    if (antitheticSampling_) {
        antitheticVariate_ = !antitheticVariate_;
        if (antitheticVariate_) {
            auto tmp = rsg_->lastSequence();
            std::transform(tmp.value.begin(), tmp.value.end(), tmp.value.begin(), std::negate<Real>());
            return tmp;
        } else {
            return rsg_->nextSequence();
        }
    }
    return rsg_->nextSequence();
}

MultiPathVariateGeneratorSobol::MultiPathVariateGeneratorSobol(const Size dimension, const Size timeSteps,
                                                               BigNatural seed,
                                                               SobolRsg::DirectionIntegers directionIntegers)
    : MultiPathVariateGeneratorBase(dimension, timeSteps), seed_(seed), directionIntegers_(directionIntegers) {
    reset();
}

void MultiPathVariateGeneratorSobol::reset() {
    rsg_ = boost::make_shared<InverseCumulativeRsg<SobolRsg, InverseCumulativeNormal>>(
        SobolRsg(dimension_ * timeSteps_, seed_, directionIntegers_), InverseCumulativeNormal());
}

Sample<std::vector<Real>> MultiPathVariateGeneratorSobol::nextSequence() const { return rsg_->nextSequence(); }

MultiPathVariateGeneratorSobolBrownianBridge::MultiPathVariateGeneratorSobolBrownianBridge(
    const Size dimension, const Size timeSteps, SobolBrownianGenerator::Ordering ordering, BigNatural seed,
    SobolRsg::DirectionIntegers directionIntegers)
    : MultiPathVariateGeneratorBase(dimension, timeSteps), ordering_(ordering), seed_(seed),
      directionIntegers_(directionIntegers) {
    reset();
}

void MultiPathVariateGeneratorSobolBrownianBridge::reset() {
    gen_ = boost::make_shared<SobolBrownianGenerator>(dimension_, timeSteps_, ordering_, seed_, directionIntegers_);
}

Sample<std::vector<Real>> MultiPathVariateGeneratorSobolBrownianBridge::nextSequence() const {
    // not needed, we directly overwrite next()
    return Sample<std::vector<Real>>(std::vector<Real>(), 0.0);
}

Sample<std::vector<Array>> MultiPathVariateGeneratorSobolBrownianBridge::next() const {
    Real weight = gen_->nextPath();
    std::vector<Array> output(timeSteps_, Array(dimension_));
    std::vector<Real> tmp(dimension_);
    for (Size i = 0; i < timeSteps_; ++i) {
        gen_->nextStep(tmp);
        std::copy(tmp.begin(), tmp.end(), output[i].begin());
    }
    return Sample<std::vector<Array>>(output, weight);
}

boost::shared_ptr<MultiPathVariateGeneratorBase>
makeMultiPathVariateGenerator(const SequenceType s, const Size dimension, const Size timeSteps, const BigNatural seed,
                              const SobolBrownianGenerator::Ordering ordering,
                              const SobolRsg::DirectionIntegers directionIntegers) {
    switch (s) {
    case MersenneTwister:
        return boost::make_shared<QuantExt::MultiPathVariateGeneratorMersenneTwister>(dimension, timeSteps, seed,
                                                                                      false);
    case MersenneTwisterAntithetic:
        return boost::make_shared<QuantExt::MultiPathVariateGeneratorMersenneTwister>(dimension, timeSteps, seed, true);
    case Sobol:
        return boost::make_shared<QuantExt::MultiPathVariateGeneratorSobol>(dimension, timeSteps, seed,
                                                                            directionIntegers);
    case SobolBrownianBridge:
        return boost::make_shared<QuantExt::MultiPathVariateGeneratorSobolBrownianBridge>(
            dimension, timeSteps, ordering, seed, directionIntegers);
    default:
        QL_FAIL("Unknown sequence type");
    }
}

} // namespace QuantExt
