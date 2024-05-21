/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/model/crcirbuilder.hpp>

#include <qle/models/cirppconstantfellerparametrization.hpp>
#include <qle/models/cirppconstantparametrization.hpp>

#include <qle/models/cdsoptionhelper.hpp>

#include <ql/math/optimization/levenbergmarquardt.hpp>

#include <ored/model/utilities.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>
#include <ored/utilities/to_string.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

CrCirBuilder::CrCirBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<CrCirData>& data,
                           const std::string& configuration)
    : market_(market), configuration_(configuration), data_(data),
      optimizationMethod_(QuantLib::ext::shared_ptr<OptimizationMethod>(new LevenbergMarquardt(1E-8, 1E-8, 1E-8))),
      endCriteria_(EndCriteria(1000, 500, 1E-8, 1E-8, 1E-8)),
      calibrationErrorType_(BlackCalibrationHelper::RelativePriceError) {

    LOG("CIR CR Calibration for name " << data_->name());

    rateCurve_ = market->discountCurve(data_->currency(), configuration);
    creditCurve_ = market->defaultCurve(data_->name(), configuration)->curve();
    recoveryRate_ = market->recoveryRate(data_->name(), configuration);

    registerWith(rateCurve_);
    registerWith(creditCurve_);
    registerWith(recoveryRate_);

    // shifted CIR model hard coded here
    parametrization_ = QuantLib::ext::make_shared<QuantExt::CrCirppConstantWithFellerParametrization>(
        parseCurrency(data_->currency()), creditCurve_, data_->reversionValue(), data_->longTermValue(),
        data_->volatility(), data_->startValue(), true, data_->relaxedFeller(), data_->fellerFactor(),
        data_->name());

    // alternatively, use unconstrained parametrization (only positivity of all parameters is implied)
    // parametrization_ = QuantLib::ext::make_shared<QuantExt::CrCirppConstantParametrization>(
    //     parseCurrency(data_->currency()), creditCurve_, data_->reversionValue(), data_->longTermValue(),
    //     data_->volatility(), data_->startValue(), false);

    model_ = QuantLib::ext::make_shared<QuantExt::CrCirpp>(parametrization_);
}

} // namespace data
} // namespace ore
