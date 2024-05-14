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
    const QuantLib::ext::shared_ptr<StochasticProcess>& process, const TimeGrid& grid, BigNatural seed, bool antitheticSampling)
    : process_(process), grid_(grid), seed_(seed), antitheticSampling_(antitheticSampling), antitheticVariate_(true),
      next_(MultiPath(process->size(), grid), 1.0) {
    MultiPathGeneratorMersenneTwister::reset();
}

void MultiPathGeneratorMersenneTwister::reset() {
    PseudoRandom::rsg_type rsg = PseudoRandom::make_sequence_generator(process_->factors() * (grid_.size() - 1), seed_);
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<StochasticProcess1D>(process_)) {
        pg1D_ = QuantLib::ext::make_shared<PathGenerator<PseudoRandom::rsg_type>>(tmp, grid_, rsg, false);
    } else {
        pg_ = QuantLib::ext::make_shared<MultiPathGenerator<PseudoRandom::rsg_type>>(process_, grid_, rsg, false);
    }
    antitheticVariate_ = true;
}

const Sample<MultiPath>& MultiPathGeneratorMersenneTwister::next() const {
    if (antitheticSampling_) {
        antitheticVariate_ = !antitheticVariate_;
        if (pg_) {
            return antitheticVariate_ ? pg_->antithetic() : pg_->next();
        } else {
            next_.value.at(0) = antitheticVariate_ ? pg1D_->antithetic().value : pg1D_->next().value;
            return next_;
        }
    } else {
        if (pg_)
            return pg_->next();
        else {
            next_.value.at(0) = pg1D_->next().value;
            return next_;
        }
    }
}

MultiPathGeneratorSobol::MultiPathGeneratorSobol(const QuantLib::ext::shared_ptr<StochasticProcess>& process,
                                                 const TimeGrid& grid, BigNatural seed,
                                                 SobolRsg::DirectionIntegers directionIntegers)
    : process_(process), grid_(grid), seed_(seed), directionIntegers_(directionIntegers),
      next_(MultiPath(process->size(), grid), 1.0) {
    MultiPathGeneratorSobol::reset();
}

void MultiPathGeneratorSobol::reset() {
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<StochasticProcess1D>(process_)) {
        pg1D_ = QuantLib::ext::make_shared<PathGenerator<InverseCumulativeRsg<SobolRsg, InverseCumulativeNormal>>>(
            tmp, grid_,
            InverseCumulativeRsg<SobolRsg, InverseCumulativeNormal>(
                SobolRsg(process_->factors() * (grid_.size() - 1), seed_, directionIntegers_)),
            false);

    } else {
        pg_ = QuantLib::ext::make_shared<MultiPathGenerator<InverseCumulativeRsg<SobolRsg, InverseCumulativeNormal>>>(
            process_, grid_,
            InverseCumulativeRsg<SobolRsg, InverseCumulativeNormal>(
                SobolRsg(process_->factors() * (grid_.size() - 1), seed_, directionIntegers_)));
    }
}

const Sample<MultiPath>& MultiPathGeneratorSobol::next() const {
    if (pg_)
        return pg_->next();
    else {
        next_.value.at(0) = pg1D_->next().value;
        return next_;
    }
}

MultiPathGeneratorBurley2020Sobol::MultiPathGeneratorBurley2020Sobol(const QuantLib::ext::shared_ptr<StochasticProcess>& process,
                                                           const TimeGrid& grid, BigNatural seed,
                                                           SobolRsg::DirectionIntegers directionIntegers,
                                                           BigNatural scrambleSeed)
    : process_(process), grid_(grid), seed_(seed), directionIntegers_(directionIntegers), scrambleSeed_(scrambleSeed),
      next_(MultiPath(process->size(), grid), 1.0) {
    MultiPathGeneratorBurley2020Sobol::reset();
}

void MultiPathGeneratorBurley2020Sobol::reset() {
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<StochasticProcess1D>(process_)) {
        pg1D_ = QuantLib::ext::make_shared<PathGenerator<InverseCumulativeRsg<Burley2020SobolRsg, InverseCumulativeNormal>>>(
            tmp, grid_,
            InverseCumulativeRsg<Burley2020SobolRsg, InverseCumulativeNormal>(
                Burley2020SobolRsg(process_->factors() * (grid_.size() - 1), seed_, directionIntegers_, scrambleSeed_)),
            false);

    } else {
        pg_ = QuantLib::ext::make_shared<MultiPathGenerator<InverseCumulativeRsg<Burley2020SobolRsg, InverseCumulativeNormal>>>(
            process_, grid_,
            InverseCumulativeRsg<Burley2020SobolRsg, InverseCumulativeNormal>(Burley2020SobolRsg(
                process_->factors() * (grid_.size() - 1), seed_, directionIntegers_, scrambleSeed_)));
    }
}

const Sample<MultiPath>& MultiPathGeneratorBurley2020Sobol::next() const {
    if (pg_)
        return pg_->next();
    else {
        next_.value.at(0) = pg1D_->next().value;
        return next_;
    }
}

MultiPathGeneratorSobolBrownianBridgeBase::MultiPathGeneratorSobolBrownianBridgeBase(
    const QuantLib::ext::shared_ptr<StochasticProcess>& process, const TimeGrid& grid,
    SobolBrownianGenerator::Ordering ordering, BigNatural seed, SobolRsg::DirectionIntegers directionIntegers)
    : process_(process), grid_(grid), ordering_(ordering), seed_(seed), directionIntegers_(directionIntegers),
      next_(MultiPath(process->size(), grid), 1.0) {
    process1D_ = QuantLib::ext::dynamic_pointer_cast<StochasticProcess1D>(process);
}

const Sample<MultiPath>& MultiPathGeneratorSobolBrownianBridgeBase::next() const {
    Array asset = process_->initialValues();
    MultiPath& path = next_.value;
    for (Size j = 0; j < asset.size(); ++j) {
        path[j].front() = asset[j];
    }
    next_.weight = gen_->nextPath();
    std::vector<Real> output(process_->factors());
    for (Size i = 1; i < grid_.size(); ++i) {
        Real t = grid_[i - 1];
        Real dt = grid_.dt(i - 1);
        gen_->nextStep(output);
        if (process1D_) {
            path[0][i] = asset[0] = process1D_->evolve(t, asset[0], dt, output[0]);
        } else {
            Array tmp(output.begin(), output.end());
            asset = process_->evolve(t, asset, dt, tmp);
            for (Size j = 0; j < asset.size(); ++j) {
                path[j][i] = asset[j];
            }
        }
    }
    return next_;
}

MultiPathGeneratorSobolBrownianBridge::MultiPathGeneratorSobolBrownianBridge(
    const QuantLib::ext::shared_ptr<StochasticProcess>& process, const TimeGrid& grid,
    SobolBrownianGenerator::Ordering ordering, BigNatural seed, SobolRsg::DirectionIntegers directionIntegers)
    : MultiPathGeneratorSobolBrownianBridgeBase(process, grid, ordering, seed, directionIntegers) {
    MultiPathGeneratorSobolBrownianBridge::reset();
}

void MultiPathGeneratorSobolBrownianBridge::reset() {
    gen_ = QuantLib::ext::make_shared<SobolBrownianGenerator>(process_->factors(), grid_.size() - 1, ordering_, seed_,
                                                      directionIntegers_);
}

MultiPathGeneratorBurley2020SobolBrownianBridge::MultiPathGeneratorBurley2020SobolBrownianBridge(
    const QuantLib::ext::shared_ptr<StochasticProcess>& process, const TimeGrid& grid,
    Burley2020SobolBrownianGenerator::Ordering ordering, BigNatural seed, SobolRsg::DirectionIntegers directionIntegers,
    BigNatural scrambleSeed)
    : MultiPathGeneratorSobolBrownianBridgeBase(process, grid, ordering, seed, directionIntegers),
      scrambleSeed_(scrambleSeed) {
    MultiPathGeneratorBurley2020SobolBrownianBridge::reset();
}

void MultiPathGeneratorBurley2020SobolBrownianBridge::reset() {
    gen_ = QuantLib::ext::make_shared<Burley2020SobolBrownianGenerator>(process_->factors(), grid_.size() - 1, ordering_, seed_,
                                                                directionIntegers_, scrambleSeed_);
}

QuantLib::ext::shared_ptr<MultiPathGeneratorBase>
makeMultiPathGenerator(const SequenceType s, const QuantLib::ext::shared_ptr<StochasticProcess>& process,
                       const TimeGrid& timeGrid, const BigNatural seed, const SobolBrownianGenerator::Ordering ordering,
                       const SobolRsg::DirectionIntegers directionIntegers) {
    switch (s) {
    case MersenneTwister:
        return QuantLib::ext::make_shared<QuantExt::MultiPathGeneratorMersenneTwister>(process, timeGrid, seed, false);
    case MersenneTwisterAntithetic:
        return QuantLib::ext::make_shared<QuantExt::MultiPathGeneratorMersenneTwister>(process, timeGrid, seed, true);
    case Sobol:
        return QuantLib::ext::make_shared<QuantExt::MultiPathGeneratorSobol>(process, timeGrid, seed, directionIntegers);
    case Burley2020Sobol:
        return QuantLib::ext::make_shared<QuantExt::MultiPathGeneratorBurley2020Sobol>(
            process, timeGrid, seed, directionIntegers, seed == 0 ? 0 : seed + 1);
    case SobolBrownianBridge:
        return QuantLib::ext::make_shared<QuantExt::MultiPathGeneratorSobolBrownianBridge>(process, timeGrid, ordering, seed,
                                                                                   directionIntegers);
    case Burley2020SobolBrownianBridge:
        return QuantLib::ext::make_shared<QuantExt::MultiPathGeneratorBurley2020SobolBrownianBridge>(
            process, timeGrid, ordering, seed, directionIntegers, seed == 0 ? 0 : seed + 1);
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
    case Burley2020Sobol:
        return out << "Burley2020Sobol";
    case SobolBrownianBridge:
        return out << "SobolBrownianBridge";
    case Burley2020SobolBrownianBridge:
        return out << "Burley2020SobolBrownianBridge";
    default:
        return out << "Unknown sequence type";
    }
}

} // namespace QuantExt
