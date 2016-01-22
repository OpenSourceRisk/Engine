/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/methods/multipathgeneratorbase.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

MultiPathGeneratorPseudoRandom::MultiPathGeneratorPseudoRandom(
    const boost::shared_ptr<StochasticProcess> &process, const TimeGrid &grid,
    Size dimension, BigNatural seed, bool brownianBridge) {
    PseudoRandom::rsg_type rsg =
        PseudoRandom::make_sequence_generator(dimension, seed);
    pg_ = boost::make_shared<MultiPathGenerator<PseudoRandom::rsg_type> >(
        process, grid, rsg, brownianBridge);
}

MultiPathGeneratorLowDiscrepancy::MultiPathGeneratorLowDiscrepancy(
    const boost::shared_ptr<StochasticProcess> &process, const TimeGrid &grid,
    Size dimension, BigNatural seed, bool brownianBridge) {
    LowDiscrepancy::rsg_type rsg =
        LowDiscrepancy::make_sequence_generator(dimension, seed);
    pg_ = boost::make_shared<MultiPathGenerator<LowDiscrepancy::rsg_type> >(
        process, grid, rsg, brownianBridge);
}

} // namesapce QuantExt
