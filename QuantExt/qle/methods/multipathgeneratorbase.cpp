/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/methods/multipathgeneratorbase.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

    MultiPathGeneratorPseudoRandom::MultiPathGeneratorPseudoRandom(
         const boost::shared_ptr<StochasticProcess> &process, const TimeGrid &grid,
         Size dimension, BigNatural seed, bool antitheticSampling)
        : process_(process), grid_(grid), dimension_(dimension), seed_(seed),
          antitheticSampling_(antitheticSampling), antitheticVariate_(true) {
        reset();
    }

    void MultiPathGeneratorPseudoRandom::reset() {
        PseudoRandom::rsg_type rsg =
            PseudoRandom::make_sequence_generator(dimension_, seed_);
        pg_ = boost::make_shared<MultiPathGenerator<PseudoRandom::rsg_type> >(
              process_, grid_, rsg, false);
    }

    MultiPathGeneratorLowDiscrepancy::MultiPathGeneratorLowDiscrepancy(
          const boost::shared_ptr<StochasticProcess> &process, const TimeGrid &grid,
          Size dimension, BigNatural seed, bool brownianBridge)
        : process_(process), grid_(grid), dimension_(dimension), seed_(seed),
          brownianBridge_(brownianBridge) {
        reset();
    }

    void MultiPathGeneratorLowDiscrepancy::reset() {
        LowDiscrepancy::rsg_type rsg =
            LowDiscrepancy::make_sequence_generator(dimension_, seed_);
        pg_ = boost::make_shared<MultiPathGenerator<LowDiscrepancy::rsg_type> >(
              process_, grid_, rsg, brownianBridge_);
    }

} // namesapce QuantExt
