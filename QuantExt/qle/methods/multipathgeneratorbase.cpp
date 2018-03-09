/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <qle/methods/multipathgeneratorbase.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

MultiPathGeneratorMersenneTwister::MultiPathGeneratorMersenneTwister(
    const boost::shared_ptr<StochasticProcess>& process, const TimeGrid& grid, BigNatural seed, bool antitheticSampling)
    : process_(process), grid_(grid), seed_(seed), antitheticSampling_(antitheticSampling), antitheticVariate_(true) {
    reset();
}

void MultiPathGeneratorMersenneTwister::reset() {
    PseudoRandom::rsg_type rsg = PseudoRandom::make_sequence_generator(process_->size() * (grid_.size() - 1), seed_);
    pg_ = boost::make_shared<MultiPathGenerator<PseudoRandom::rsg_type> >(process_, grid_, rsg, false);
}

MultiPathGeneratorSobol::MultiPathGeneratorSobol(const boost::shared_ptr<StochasticProcess>& process,
                                                 const TimeGrid& grid, BigNatural seed)
    : process_(process), grid_(grid), seed_(seed) {
    reset();
}

void MultiPathGeneratorSobol::reset() {
    LowDiscrepancy::rsg_type rsg =
        LowDiscrepancy::make_sequence_generator(process_->size() * (grid_.size() - 1), seed_);
    pg_ = boost::make_shared<MultiPathGenerator<LowDiscrepancy::rsg_type> >(process_, grid_, rsg);
}

MultiPathGeneratorSobolBrownianBridge::MultiPathGeneratorSobolBrownianBridge(
    const boost::shared_ptr<StochasticProcess>& process, const TimeGrid& grid,
    SobolBrownianGenerator::Ordering ordering, BigNatural seed, SobolRsg::DirectionIntegers directionIntegers)
    : process_(process), grid_(grid), ordering_(ordering), seed_(seed), directionIntegers_(directionIntegers),
      next_(MultiPath(process->size(), grid), 1.0) {
    reset();
}

void MultiPathGeneratorSobolBrownianBridge::reset() {
    gen_ = boost::make_shared<SobolBrownianGenerator>(process_->size(), grid_.size() - 1, ordering_, seed_,
                                                      directionIntegers_);
}

const Sample<MultiPath>& MultiPathGeneratorSobolBrownianBridge::next() const {
    Array asset = process_->initialValues();
    MultiPath& path = next_.value;
    for (Size j = 0; j < asset.size(); ++j) {
        path[j].front() = asset[j];
    }
    next_.weight = gen_->nextPath();
    std::vector<Real> output(asset.size());
    for (Size i = 1; i < grid_.size(); ++i) {
        Real t = grid_[i - 1];
        Real dt = grid_.dt(i - 1);
        gen_->nextStep(output);
        Array tmp(output.begin(), output.end());
        asset = process_->evolve(t, asset, dt, tmp);
        for (Size j = 0; j < asset.size(); ++j) {
            path[j][i] = asset[j];
        }
    }
    return next_;
}

boost::shared_ptr<MultiPathGeneratorBase> makeMultiPathGenerator(const SequenceType s,
                                                                 const boost::shared_ptr<StochasticProcess>& process,
                                                                 const TimeGrid& timeGrid, const BigNatural seed,
                                                                 const SobolBrownianGenerator::Ordering ordering,
                                                                 const SobolRsg::DirectionIntegers directionIntegers) {
    switch (s) {
    case MersenneTwister:
        return boost::make_shared<QuantExt::MultiPathGeneratorMersenneTwister>(process, timeGrid, seed, false);
    case MersenneTwisterAntithetic:
        return boost::make_shared<QuantExt::MultiPathGeneratorMersenneTwister>(process, timeGrid, seed, true);
    case Sobol:
        return boost::make_shared<QuantExt::MultiPathGeneratorSobol>(process, timeGrid, seed);
    case SobolBrownianBridge:
        return boost::make_shared<QuantExt::MultiPathGeneratorSobolBrownianBridge>(process, timeGrid, ordering, seed,
                                                                                   directionIntegers);
    default:
        QL_FAIL("Unknown sequence type");
    }
}

std::ostream& operator<<(std::ostream& out, const SequenceType s) {
    switch (s) {
    case MersenneTwister:
        return out << "MersenneTwister";
    case MersenneTwisterAntithetic:
        return out << "MersenneTwisterAntithetic";
    case Sobol:
        return out << "Sobol";
    case SobolBrownianBridge:
        return out << "SobolBrownianBridge";
    default:
        return out << "Unknown sequence type";
    }
}

} // namespace QuantExt
