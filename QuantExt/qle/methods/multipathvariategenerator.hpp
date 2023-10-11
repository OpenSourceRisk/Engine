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

/*! \file multipathvariategenerator.hpp
    \brief multi path generators returning the generating N(0,1) variates, this is very much in parallel to the
           MultiPathGeneratorBase interface and derived classes, including the make function
    \ingroup models
*/

#pragma once

#include <qle/methods/multipathgeneratorbase.hpp>

namespace QuantExt {

class MultiPathVariateGeneratorBase {
public:
    MultiPathVariateGeneratorBase(const Size dimension, const Size timeSteps);
    virtual ~MultiPathVariateGeneratorBase() {}
    virtual Sample<std::vector<Array>> next() const;
    virtual void reset() = 0;

protected:
    virtual Sample<std::vector<Real>> nextSequence() const = 0;

    Size dimension_;
    Size  timeSteps_;
};

class MultiPathVariateGeneratorMersenneTwister : public MultiPathVariateGeneratorBase {
public:
    MultiPathVariateGeneratorMersenneTwister(const Size dimension, const Size timeSteps, BigNatural seed = 0,
                                             bool antitheticSampling = false);
    void reset() override;

private:
    Sample<std::vector<Real>> nextSequence() const override;

    BigNatural seed_;
    bool antitheticSampling_;
    mutable bool antitheticVariate_;

    boost::shared_ptr<PseudoRandom::rsg_type> rsg_;
};

class MultiPathVariateGeneratorMersenneTwisterAntithetic : public MultiPathVariateGeneratorMersenneTwister {
public:
    MultiPathVariateGeneratorMersenneTwisterAntithetic(const Size dimension, const Size timeSteps, BigNatural seed = 0)
        : MultiPathVariateGeneratorMersenneTwister(dimension, timeSteps, seed, true){};
};

class MultiPathVariateGeneratorSobol : public MultiPathVariateGeneratorBase {
public:
    MultiPathVariateGeneratorSobol(const Size dimension, const Size timeSteps, BigNatural seed = 0,
                                   SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7);
    void reset() override;

private:
    Sample<std::vector<Real>> nextSequence() const override;

    BigNatural seed_;
    SobolRsg::DirectionIntegers directionIntegers_;

    boost::shared_ptr<LowDiscrepancy::rsg_type> rsg_;
};

class MultiPathVariateGeneratorSobolBrownianBridge : public MultiPathVariateGeneratorBase {
public:
    MultiPathVariateGeneratorSobolBrownianBridge(
        const Size dimension, const Size timeSteps,
        SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps, BigNatural seed = 0,
        SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7);
    Sample<std::vector<Array>> next() const override;
    void reset() override;

private:
    Sample<std::vector<Real>> nextSequence() const override;

    SobolBrownianGenerator::Ordering ordering_;
    BigNatural seed_;
    SobolRsg::DirectionIntegers directionIntegers_;

    boost::shared_ptr<SobolBrownianGenerator> gen_;
};

boost::shared_ptr<MultiPathVariateGeneratorBase>
makeMultiPathVariateGenerator(const SequenceType s, const Size dimension, const Size timeSteps, const BigNatural seed,
                              const SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
                              const SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7);

} // namespace QuantExt
