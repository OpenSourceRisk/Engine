/*
 Copyright (C) 2020 Quaternion Risk Management Ltd.
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

#include <qle/models/crlgm1fparametrization.hpp>

#include <ored/model/crlgmbuilder.hpp>
#include <ored/utilities/log.hpp>

#include <ql/currencies/america.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

CrLgmBuilder::CrLgmBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<CrLgmData>& data,
                           const std::string& configuration)
    : market_(market), configuration_(configuration), data_(data) {

    string name = data->name();
    LOG("LgmCalibration for name " << name << ", configuration is " << configuration);

    modelDefaultCurve_ = RelinkableHandle<DefaultProbabilityTermStructure>(*market_->defaultCurve(name, configuration)->curve());

    QL_REQUIRE(!data_->calibrateA() && !data_->calibrateH(), "CrLgmBuilder does not support calibration currently");

    QL_REQUIRE(data_->aParamType() == ParamType::Constant, "CrLgmBuilder only supports constant volatility currently");
    QL_REQUIRE(data_->hParamType() == ParamType::Constant, "CrLgmBuilder only supports constant reversion currently");

    Array aTimes(data_->aTimes().begin(), data_->aTimes().end());
    Array hTimes(data_->hTimes().begin(), data_->hTimes().end());
    Array alpha(data_->aValues().begin(), data_->aValues().end());
    Array h(data_->hValues().begin(), data_->hValues().end());

    // the currency does not matter here
    parametrization_ =
        QuantLib::ext::make_shared<QuantExt::CrLgm1fConstantParametrization>(USDCurrency(), modelDefaultCurve_, alpha[0], h[0], name);

    LOG("Apply shift horizon and scale");

    QL_REQUIRE(data_->shiftHorizon() >= 0.0, "shift horizon must be non negative");
    QL_REQUIRE(data_->scaling() > 0.0, "scaling must be positive");

    if (data_->shiftHorizon() > 0.0) {
        LOG("Apply shift horizon " << data_->shiftHorizon() << " to the " << data_->qualifier() << " CR-LGM model");
        parametrization_->shift() = data_->shiftHorizon();
    }

    if (data_->scaling() != 1.0) {
        LOG("Apply scaling " << data_->scaling() << " to the " << data_->qualifier() << " CR-LGM model");
        parametrization_->scaling() = data_->scaling();
    }
}
} // namespace data
} // namespace ore
