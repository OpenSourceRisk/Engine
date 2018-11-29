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

/*! \file model/utilities.hpp
    \brief Shared utilities for model building and calibration
    \ingroup models
*/

#pragma once

#include <vector>

#include <ql/models/calibrationhelper.hpp>
#include <qle/models/eqbsparametrization.hpp>
#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/infdkparametrization.hpp>
#include <qle/models/irlgm1fparametrization.hpp>


namespace ore {
namespace data {
using namespace QuantExt;
using namespace QuantLib;

Real logCalibrationErrors(
    const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& basket,
    const boost::shared_ptr<IrLgm1fParametrization>& parametrization = boost::shared_ptr<IrLgm1fParametrization>());

Real logCalibrationErrors(
    const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& basket,
    const boost::shared_ptr<FxBsParametrization>& parametrization = boost::shared_ptr<FxBsParametrization>(),
    const boost::shared_ptr<IrLgm1fParametrization>& domesticLgm = boost::shared_ptr<IrLgm1fParametrization>());

Real logCalibrationErrors(
    const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& basket,
    const boost::shared_ptr<EqBsParametrization>& parametrization = boost::shared_ptr<EqBsParametrization>(),
    const boost::shared_ptr<IrLgm1fParametrization>& domesticLgm = boost::shared_ptr<IrLgm1fParametrization>());

Real logCalibrationErrors(
    const std::vector<boost::shared_ptr<BlackCalibrationHelper>>& basket,
    const boost::shared_ptr<InfDkParametrization>& parametrization = boost::shared_ptr<InfDkParametrization>(),
    const boost::shared_ptr<IrLgm1fParametrization>& domesticLgm = boost::shared_ptr<IrLgm1fParametrization>());
} // namespace data
} // namespace ore
