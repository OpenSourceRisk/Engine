/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
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
