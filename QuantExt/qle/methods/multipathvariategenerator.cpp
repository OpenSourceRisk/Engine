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
    MultiPathVariateGeneratorMersenneTwister::reset();
}

void MultiPathVariateGeneratorMersenneTwister::reset() {
    rsg_ = QuantLib::ext::make_shared<
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
    MultiPathVariateGeneratorSobol::reset();
}

void MultiPathVariateGeneratorSobol::reset() {
    rsg_ = QuantLib::ext::make_shared<InverseCumulativeRsg<SobolRsg, InverseCumulativeNormal>>(
        SobolRsg(dimension_ * timeSteps_, seed_, directionIntegers_), InverseCumulativeNormal());
}

Sample<std::vector<Real>> MultiPathVariateGeneratorSobol::nextSequence() const { return rsg_->nextSequence(); }

MultiPathVariateGeneratorBurley2020Sobol::MultiPathVariateGeneratorBurley2020Sobol(
    const Size dimension, const Size timeSteps, BigNatural seed, SobolRsg::DirectionIntegers directionIntegers,
    BigNatural scrambleSeed)
    : MultiPathVariateGeneratorBase(dimension, timeSteps), seed_(seed), directionIntegers_(directionIntegers),
      scrambleSeed_(scrambleSeed) {
    MultiPathVariateGeneratorBurley2020Sobol::reset();
}

void MultiPathVariateGeneratorBurley2020Sobol::reset() {
    rsg_ = QuantLib::ext::make_shared<InverseCumulativeRsg<Burley2020SobolRsg, InverseCumulativeNormal>>(
        Burley2020SobolRsg(dimension_ * timeSteps_, seed_, directionIntegers_, scrambleSeed_), InverseCumulativeNormal());
}

Sample<std::vector<Real>> MultiPathVariateGeneratorBurley2020Sobol::nextSequence() const {
    return rsg_->nextSequence();
}

MultiPathVariateGeneratorSobolBrownianBridgeBase::MultiPathVariateGeneratorSobolBrownianBridgeBase(
    const Size dimension, const Size timeSteps, SobolBrownianGenerator::Ordering ordering, BigNatural seed,
    SobolRsg::DirectionIntegers directionIntegers)
    : MultiPathVariateGeneratorBase(dimension, timeSteps), ordering_(ordering), seed_(seed),
      directionIntegers_(directionIntegers) {}

Sample<std::vector<Real>> MultiPathVariateGeneratorSobolBrownianBridgeBase::nextSequence() const {
    // not needed, we directly overwrite next()
    return Sample<std::vector<Real>>(std::vector<Real>(), 0.0);
}

Sample<std::vector<Array>> MultiPathVariateGeneratorSobolBrownianBridgeBase::next() const {
    Real weight = gen_->nextPath();
    std::vector<Array> output(timeSteps_, Array(dimension_));
    std::vector<Real> tmp(dimension_);
    for (Size i = 0; i < timeSteps_; ++i) {
        gen_->nextStep(tmp);
        std::copy(tmp.begin(), tmp.end(), output[i].begin());
    }
    return Sample<std::vector<Array>>(output, weight);
}

MultiPathVariateGeneratorSobolBrownianBridge::MultiPathVariateGeneratorSobolBrownianBridge(
    const Size dimension, const Size timeSteps, SobolBrownianGenerator::Ordering ordering, BigNatural seed,
    SobolRsg::DirectionIntegers directionIntegers)
    : MultiPathVariateGeneratorSobolBrownianBridgeBase(dimension, timeSteps, ordering, seed, directionIntegers) {
    MultiPathVariateGeneratorSobolBrownianBridge::reset();
}

void MultiPathVariateGeneratorSobolBrownianBridge::reset() {
    gen_ = QuantLib::ext::make_shared<SobolBrownianGenerator>(dimension_, timeSteps_, ordering_, seed_, directionIntegers_);
}

MultiPathVariateGeneratorBurley2020SobolBrownianBridge::MultiPathVariateGeneratorBurley2020SobolBrownianBridge(
    const Size dimension, const Size timeSteps, SobolBrownianGenerator::Ordering ordering, BigNatural seed,
    SobolRsg::DirectionIntegers directionIntegers, BigNatural scrambleSeed)
    : MultiPathVariateGeneratorSobolBrownianBridgeBase(dimension, timeSteps, ordering, seed, directionIntegers),
      scrambleSeed_(scrambleSeed) {
    MultiPathVariateGeneratorBurley2020SobolBrownianBridge::reset();
}

void MultiPathVariateGeneratorBurley2020SobolBrownianBridge::reset() {
    gen_ = QuantLib::ext::make_shared<Burley2020SobolBrownianGenerator>(dimension_, timeSteps_, ordering_, seed_,
                                                                directionIntegers_, scrambleSeed_);
}

QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>
makeMultiPathVariateGenerator(const SequenceType s, const Size dimension, const Size timeSteps, const BigNatural seed,
                              const SobolBrownianGenerator::Ordering ordering,
                              const SobolRsg::DirectionIntegers directionIntegers) {
    switch (s) {
    case MersenneTwister:
        return QuantLib::ext::make_shared<QuantExt::MultiPathVariateGeneratorMersenneTwister>(dimension, timeSteps, seed,
                                                                                      false);
    case MersenneTwisterAntithetic:
        return QuantLib::ext::make_shared<QuantExt::MultiPathVariateGeneratorMersenneTwister>(dimension, timeSteps, seed, true);
    case Sobol:
        return QuantLib::ext::make_shared<QuantExt::MultiPathVariateGeneratorSobol>(dimension, timeSteps, seed,
                                                                            directionIntegers);
    case Burley2020Sobol:
        return QuantLib::ext::make_shared<QuantExt::MultiPathVariateGeneratorBurley2020Sobol>(dimension, timeSteps, seed,
                                                                                      directionIntegers, seed + 1);
    case SobolBrownianBridge:
        return QuantLib::ext::make_shared<QuantExt::MultiPathVariateGeneratorSobolBrownianBridge>(
            dimension, timeSteps, ordering, seed, directionIntegers);
    case Burley2020SobolBrownianBridge:
        return QuantLib::ext::make_shared<QuantExt::MultiPathVariateGeneratorBurley2020SobolBrownianBridge>(
            dimension, timeSteps, ordering, seed, directionIntegers, seed + 1);
    default:
        QL_FAIL("Unknown sequence type");
    }
}

} // namespace QuantExt
