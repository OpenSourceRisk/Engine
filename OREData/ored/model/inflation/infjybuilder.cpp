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

#include <ored/model/inflation/infjybuilder.hpp>
#include <ored/utilities/log.hpp>
#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/infjyparameterization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>

using std::string;

namespace ore {
namespace data {

InfJyBuilder::InfJyBuilder(
    const boost::shared_ptr<Market>& market,
    const boost::shared_ptr<InfJyData>& data,
    const string& configuration,
    const string& referenceCalibrationGrid)
    : market_(market),
      configuration_(configuration),
      data_(data),
      referenceCalibrationGrid_(referenceCalibrationGrid),
      marketObserver_(boost::make_shared<MarketObserver>()),
      inflationIndex_(*market_->zeroInflationIndex(data_->index(), configuration_)),
      inflationVolatility_(market_->cpiInflationCapFloorVolatilitySurface(data_->index(), configuration_)) {

    LOG("InfJyBuilder: building model for inflation index " << data_->index());

    if (!data_->calibrationBaskets().empty()) {
        QL_FAIL("InfJyBuilder: need to do work with the calibration baskets.");
    }

    // Register with market observables except volatilities
    marketObserver_->registerWith(inflationIndex_);

    // Register the model builder with the market observer
    registerWith(marketObserver_);

    // Notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    // Need to build calibration instruments if any of the parameters need calibration
    const ReversionParameter& rrReversion = data_->realRateReversion();
    const VolatilityParameter& rrVolatility = data_->realRateVolatility();
    const VolatilityParameter& idxVolatility = data_->indexVolatility();
    if (rrVolatility.calibrate() || rrReversion.calibrate() || idxVolatility.calibrate()) {
        QL_FAIL("InfJyBuilder: need to do work with the calibration baskets.");
    }

    // In the event of calibration, need to restructure the real rate and reversion parameters.
    Array rrVolatilityTimes(rrVolatility.times().begin(), rrVolatility.times().end());
    Array rrVolatilityValues(rrVolatility.values().begin(), rrVolatility.values().end());
    Array rrReversionTimes(rrReversion.times().begin(), rrReversion.times().end());
    Array rrReversionValues(rrReversion.values().begin(), rrReversion.values().end());
    Array idxVolatilityTimes(idxVolatility.times().begin(), idxVolatility.times().end());
    Array idxVolatilityValues(idxVolatility.values().begin(), idxVolatility.values().end());

    // Create the JY parameterization.
    using RT = LgmData::ReversionType;
    using VT = LgmData::VolatilityType;

    // 1) Create the real rate portion of the parameterization
    using QuantLib::ZeroInflationTermStructure;
    boost::shared_ptr<QuantExt::Lgm1fParametrization<ZeroInflationTermStructure>> realRateParam;
    if (rrReversion.reversionType() == RT::HullWhite && rrVolatility.volatilityType() == VT::HullWhite) {
        using QuantExt::Lgm1fPiecewiseConstantHullWhiteAdaptor;
        DLOG("InfJyBuilder: real rate parameterization is Lgm1fPiecewiseConstantHullWhiteAdaptor");
        realRateParam = boost::make_shared<Lgm1fPiecewiseConstantHullWhiteAdaptor<ZeroInflationTermStructure>>(
            inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), rrVolatilityTimes,
            rrVolatilityValues, rrReversionTimes, rrReversionValues, data_->index());
    } else if (rrReversion.reversionType() == RT::HullWhite) {
        using QuantExt::Lgm1fPiecewiseConstantParametrization;
        DLOG("InfJyBuilder: real rate parameterization is Lgm1fPiecewiseConstantParametrization");
        realRateParam = boost::make_shared<Lgm1fPiecewiseConstantParametrization<ZeroInflationTermStructure>>(
            inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), rrVolatilityTimes,
            rrVolatilityValues, rrReversionTimes, rrReversionValues, data_->index());
    } else {
        using QuantExt::Lgm1fPiecewiseLinearParametrization;
        DLOG("InfJyBuilder: real rate parameterization is Lgm1fPiecewiseLinearParametrization");
        realRateParam = boost::make_shared<Lgm1fPiecewiseLinearParametrization<ZeroInflationTermStructure>>(
            inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), rrVolatilityTimes,
            rrVolatilityValues, rrReversionTimes, rrReversionValues, data_->index());
    }

    Time horizon = data_->reversionTransformation().horizon();
    if (horizon >= 0.0) {
        DLOG("InfJyBuilder: apply shift horizon " << horizon << " to the JY real rate parameterisation for index " <<
            data_->index() << ".");
        realRateParam->shift() = horizon;
    } else {
        WLOG("InfJyBuilder: ignoring negative horizon, " << horizon <<
            ", passed to the JY real rate parameterisation for index " << data_->index() << ".");
    }

    Real scaling = data_->reversionTransformation().scaling();
    if (scaling > 0.0) {
        DLOG("InfJyBuilder: apply scaling " << scaling << " to the JY real rate parameterisation for index " <<
            data_->index() << ".");
        realRateParam->scaling() = scaling;
    } else {
        WLOG("Ignoring non-positive scaling, " << scaling <<
            ", passed to the JY real rate parameterisation for index " << data_->index() << ".");
    }

    // 2) Create the index portion of the parameterization
    boost::shared_ptr<QuantExt::FxBsParametrization> indexParam;
    
    Handle<Quote> baseCpiQuote(boost::make_shared<SimpleQuote>(
        inflationIndex_->fixing(inflationIndex_->zeroInflationTermStructure()->baseDate())));

    if (idxVolatility.type() == ParamType::Piecewise) {
        using QuantExt::FxBsPiecewiseConstantParametrization;
        DLOG("InfJyBuilder: index volatility parameterization is FxBsPiecewiseConstantParametrization");
        indexParam = boost::make_shared<FxBsPiecewiseConstantParametrization>(
            inflationIndex_->currency(), baseCpiQuote, idxVolatilityTimes, idxVolatilityValues);
    } else if (idxVolatility.type() == ParamType::Constant) {
        using QuantExt::FxBsConstantParametrization;
        DLOG("InfJyBuilder: index volatility parameterization is FxBsConstantParametrization");
        indexParam = boost::make_shared<FxBsConstantParametrization>(
            inflationIndex_->currency(), baseCpiQuote, idxVolatilityValues[0]);
    } else {
        QL_FAIL("InfJyBuilder: index volatility parameterization needs to be Piecewise or Constant.");
    }

    // 3) The final parameterisation is a combination of the real rate and index parameterisation.
    parameterization_ = boost::make_shared<QuantExt::InfJyParameterization>(realRateParam, indexParam);

}

string InfJyBuilder::inflationIndex() const {
    return data_->index();
}

boost::shared_ptr<QuantExt::InfJyParameterization> InfJyBuilder::parameterization() const {
    calculate();
    return parameterization_;
}

bool InfJyBuilder::requiresRecalibration() const {
    return (data_->realRateVolatility().calibrate() ||
            data_->realRateReversion().calibrate() ||
            data_->indexVolatility().calibrate()) &&
           (marketObserver_->hasUpdated(false) ||
            forceCalibration_);
}

void InfJyBuilder::performCalculations() const {
    if (requiresRecalibration()) {
        marketObserver_->hasUpdated(true);
    }
}

void InfJyBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

}
}
