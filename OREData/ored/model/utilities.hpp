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

#include <ored/marketdata/strike.hpp>

#include <qle/models/eqbsparametrization.hpp>
#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/infdkparametrization.hpp>
#include <qle/models/infjyparameterization.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/commodityschwartzparametrization.hpp>
#include <qle/models/lgmcalibrationinfo.hpp>

#include <ql/models/calibrationhelper.hpp>

#include <boost/variant.hpp>

#include <vector>

namespace ore {
namespace data {
using namespace QuantExt;
using namespace QuantLib;

template <typename Helper> Real getCalibrationError(const std::vector<QuantLib::ext::shared_ptr<Helper>>& basket) {
    Real rmse = 0.0;
    for (auto const& h : basket) {
        Real tmp = h->calibrationError();
        rmse += tmp * tmp;
    }
    return std::sqrt(rmse / static_cast<Real>(basket.size()));
}

std::string getCalibrationDetails(
    LgmCalibrationInfo& info, const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
    const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& parametrization = QuantLib::ext::shared_ptr<IrLgm1fParametrization>());

std::string getCalibrationDetails(
    const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
    const QuantLib::ext::shared_ptr<FxBsParametrization>& parametrization = QuantLib::ext::shared_ptr<FxBsParametrization>(),
    const QuantLib::ext::shared_ptr<Parametrization>& domesticLgm = QuantLib::ext::shared_ptr<IrLgm1fParametrization>());

std::string getCalibrationDetails(
    const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
    const QuantLib::ext::shared_ptr<FxBsParametrization>& parametrization = QuantLib::ext::shared_ptr<FxBsParametrization>(),
    const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& domesticLgm = QuantLib::ext::shared_ptr<IrLgm1fParametrization>());

std::string getCalibrationDetails(
    const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
    const QuantLib::ext::shared_ptr<EqBsParametrization>& parametrization = QuantLib::ext::shared_ptr<EqBsParametrization>(),
    const QuantLib::ext::shared_ptr<Parametrization>& domesticLgm = QuantLib::ext::shared_ptr<IrLgm1fParametrization>());

std::string getCalibrationDetails(
    const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
    const QuantLib::ext::shared_ptr<EqBsParametrization>& parametrization = QuantLib::ext::shared_ptr<EqBsParametrization>(),
    const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& domesticLgm = QuantLib::ext::shared_ptr<IrLgm1fParametrization>());

std::string getCalibrationDetails(
    const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
    const QuantLib::ext::shared_ptr<InfDkParametrization>& parametrization = QuantLib::ext::shared_ptr<InfDkParametrization>(),
    bool indexIsInterpolated = true);

std::string getCalibrationDetails(
    const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& basket,
    const QuantLib::ext::shared_ptr<CommoditySchwartzParametrization>& parametrization = QuantLib::ext::shared_ptr<CommoditySchwartzParametrization>());

std::string getCalibrationDetails(const std::vector<QuantLib::ext::shared_ptr<CalibrationHelper>>& realRateBasket,
                                  const std::vector<QuantLib::ext::shared_ptr<CalibrationHelper>>& indexBasket,
                                  const QuantLib::ext::shared_ptr<InfJyParameterization>& parameterization,
                                  bool calibrateRealRateVol = false);

std::string getCalibrationDetails(const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& parametrization);

//! Return an option's maturity date, given an explicit date or a period.
QuantLib::Date optionMaturity(const boost::variant<QuantLib::Date, QuantLib::Period>& maturity,
                              const QuantLib::Calendar& calendar,
                              const QuantLib::Date& referenceDate = Settings::instance().evaluationDate());

//! Return a cpi cap/floor strike value, the input strike can be of type absolute or atm forward
Real cpiCapFloorStrikeValue(const QuantLib::ext::shared_ptr<BaseStrike>& strike,
                            const QuantLib::ext::shared_ptr<ZeroInflationTermStructure>& curve,
                            const QuantLib::Date& optionMaturityDate);

//! Return a yoy cap/floor strike value, the input strike can be of type absolute or atm forward
Real yoyCapFloorStrikeValue(const QuantLib::ext::shared_ptr<BaseStrike>& strike,
                            const QuantLib::ext::shared_ptr<YoYInflationTermStructure>& curve,
                            const QuantLib::Date& optionMaturityDate);

//! helper function that computes the atm forward
Real atmForward(const Real s0, const Handle<YieldTermStructure>& r, const Handle<YieldTermStructure>& q, const Real t);

} // namespace data
} // namespace ore
