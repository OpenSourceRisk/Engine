/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

#include <ored/marketdata/capfloorvolcurve.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/instruments/makeoiscapfloor.hpp>
#include <qle/interpolators/optioninterpolator2d.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/models/carrmadanarbitragecheck.hpp>
#include <qle/termstructures/capfloortermvolsurface.hpp>
#include <qle/termstructures/capfloortermvolsurfacesparse.hpp>
#include <qle/termstructures/datedstrippedoptionlet.hpp>
#include <qle/termstructures/datedstrippedoptionletadapter.hpp>
#include <qle/termstructures/optionletstripper1.hpp>
#include <qle/termstructures/optionletstripper2.hpp>
#include <qle/termstructures/optionletstripperwithatm.hpp>
#include <qle/termstructures/piecewiseatmoptionletcurve.hpp>
#include <qle/termstructures/piecewiseoptionletstripper.hpp>
#include <qle/termstructures/proxyoptionletvolatility.hpp>
#include <qle/termstructures/sabrstrippedoptionletadapter.hpp>
#include <qle/termstructures/strippedoptionletadapter.hpp>
#include <qle/utilities/cashflows.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolcurve.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletadapter.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

typedef ore::data::CapFloorVolatilityCurveConfig::VolatilityType CfgVolType;
typedef ore::data::CapFloorVolatilityCurveConfig::Type CfgType;
typedef QuantExt::CapFloorTermVolSurfaceExact::InterpolationMethod CftvsInterp;

namespace ore {
namespace data {

// Currently, only two possibilities for variable InterpolateOn: TermVolatilities and OptionletVolatilities
// Here, we convert the value to a bool for use in building the structures. May need to broaden if we add more.
// If InputType is OptionletVolatilities, InterpolateOn node will be ignored.
bool interpOnOpt(CapFloorVolatilityCurveConfig& config) {
    QL_REQUIRE(config.interpolateOn() == "TermVolatilities" || config.interpolateOn() == "OptionletVolatilities",
               "Expected InterpolateOn to be one of TermVolatilities or OptionletVolatilities");
    return config.interpolateOn() == "OptionletVolatilities";
}

CapFloorVolCurve::CapFloorVolCurve(
    const Date& asof, const CapFloorVolatilityCurveSpec& spec, const Loader& loader,
    const CurveConfigurations& curveConfigs, QuantLib::ext::shared_ptr<IborIndex> iborIndex,
    Handle<YieldTermStructure> discountCurve, const QuantLib::ext::shared_ptr<IborIndex> sourceIndex,
    const QuantLib::ext::shared_ptr<IborIndex> targetIndex,
    const std::map<std::string,
                   std::pair<QuantLib::ext::shared_ptr<ore::data::CapFloorVolCurve>, std::pair<std::string, QuantLib::Period>>>&
        requiredCapFloorVolCurves,
    const bool buildCalibrationInfo)
    : spec_(spec) {

    try {
        // The configuration
        const QuantLib::ext::shared_ptr<CapFloorVolatilityCurveConfig>& config =
            curveConfigs.capFloorVolCurveConfig(spec_.curveConfigID());

        if (!config->proxySourceCurveId().empty()) {
            // handle proxy vol surfaces
            buildProxyCurve(*config, sourceIndex, targetIndex, requiredCapFloorVolCurves);
        } else {
            QL_REQUIRE(QuantLib::ext::dynamic_pointer_cast<BMAIndexWrapper>(iborIndex) == nullptr,
                       "CapFloorVolCurve: BMA/SIFMA index in '"
                           << spec_.curveConfigID()
                           << " not allowed  - vol surfaces for SIFMA can only be proxied from Ibor / OIS");

            // Read the shift early if the configured volatility type is shifted lognormal
            Real shift = 0.0;
            if (config->volatilityType() == CfgVolType::ShiftedLognormal) {
                shift = shiftQuote(asof, *config, loader);
            }

            // There are three possible cap floor configurations
            if (config->type() == CfgType::TermAtm) {
                termAtmOptCurve(asof, *config, loader, iborIndex, discountCurve, shift);
            } else if (config->type() == CfgType::TermSurface || config->type() == CfgType::TermSurfaceWithAtm) {
                termOptSurface(asof, *config, loader, iborIndex, discountCurve, shift);
            } else if (config->type() == CfgType::OptionletSurface ||
                       config->type() == CfgType::OptionletSurfaceWithAtm) {
                optOptSurface(asof, *config, loader, iborIndex, discountCurve, shift);
            } else if (config->type() == CfgType::OptionletAtm) {
                optAtmOptCurve(asof, *config, loader, iborIndex, discountCurve, shift);
            } else {
                QL_FAIL("Unexpected type (" << static_cast<int>(config->type()) << ") for cap floor config "
                                            << config->curveID());
            }
            // Turn on or off extrapolation
            capletVol_->enableExtrapolation(config->extrapolate());
        }

        // Build calibration info
        if (buildCalibrationInfo) {
            this->buildCalibrationInfo(asof, curveConfigs, config, iborIndex);
        }

    } catch (exception& e) {
        QL_FAIL("cap/floor vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("cap/floor vol curve building failed: unknown error");
    }

    // force bootstrap so that errors are thrown during the build, not later
    capletVol_->volatility(QL_EPSILON, capletVol_->minStrike());
}

void CapFloorVolCurve::buildProxyCurve(
    const CapFloorVolatilityCurveConfig& config, const QuantLib::ext::shared_ptr<IborIndex>& sourceIndex,
    const QuantLib::ext::shared_ptr<IborIndex>& targetIndex,
    const std::map<std::string,
                   std::pair<QuantLib::ext::shared_ptr<ore::data::CapFloorVolCurve>, std::pair<std::string, QuantLib::Period>>>&
        requiredCapFloorVolCurves) {

    auto sourceVol = requiredCapFloorVolCurves.find(config.proxySourceCurveId());
    QL_REQUIRE(sourceVol != requiredCapFloorVolCurves.end(),
               "CapFloorVolCurve::buildProxyCurve(): required source cap vol curve '" << config.proxySourceCurveId()
                                                                                      << "' not found.");

    capletVol_ = QuantLib::ext::make_shared<ProxyOptionletVolatility>(
        Handle<OptionletVolatilityStructure>(sourceVol->second.first->capletVolStructure()), sourceIndex, targetIndex,
        config.proxySourceRateComputationPeriod(), config.proxyTargetRateComputationPeriod());
}

void CapFloorVolCurve::termAtmOptCurve(const Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader,
                                   QuantLib::ext::shared_ptr<IborIndex> index, Handle<YieldTermStructure> discountCurve,
                                   Real shift) {

    // Get the ATM cap floor term vol curve
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolCurve> cftvc = atmCurve(asof, config, loader);

    // Hardcode some values. Can add them to the CapFloorVolatilityCurveConfig later if needed.
    bool flatFirstPeriod = true;
    VolatilityType optVolType = Normal;
    Real optDisplacement = 0.0;

    // Get configuration values for bootstrap
    Real accuracy = config.bootstrapConfig().accuracy();
    Real globalAccuracy = config.bootstrapConfig().globalAccuracy();
    bool dontThrow = config.bootstrapConfig().dontThrow();
    Size maxAttempts = config.bootstrapConfig().maxAttempts();
    Real maxFactor = config.bootstrapConfig().maxFactor();
    Real minFactor = config.bootstrapConfig().minFactor();
    Size dontThrowSteps = config.bootstrapConfig().dontThrowSteps();

    // On optionlets is the newly added interpolation approach whereas on term volatilities is legacy
    bool onOpt = interpOnOpt(config);
    if (onOpt) {
        // This is not pretty but can't think of a better way (with template functions and or classes)
        // Note: second template argument in StrippedOptionletAdapter doesn't matter so just use Linear here.
        if (config.timeInterpolation() == "Linear") {
            QuantLib::ext::shared_ptr<PiecewiseAtmOptionletCurve<Linear>> tmp =
                QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<Linear>>(
                    config.settleDays(), cftvc, index, discountCurve, flatFirstPeriod,
                    volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, Linear(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseAtmOptionletCurve<Linear, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Linear>>(
                asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                tmp->calendar(), tmp->businessDayConvention(), index, tmp->dayCounter(),
                                tmp->volatilityType(), tmp->displacement()));
        } else if (config.timeInterpolation() == "LinearFlat") {
            QuantLib::ext::shared_ptr<PiecewiseAtmOptionletCurve<LinearFlat>> tmp =
                QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<LinearFlat>>(
                    config.settleDays(), cftvc, index, discountCurve, flatFirstPeriod,
                    volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, LinearFlat(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseAtmOptionletCurve<LinearFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Linear>>(
                asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                tmp->calendar(), tmp->businessDayConvention(), index, tmp->dayCounter(),
                                tmp->volatilityType(), tmp->displacement()));
        } else if (config.timeInterpolation() == "BackwardFlat") {
            QuantLib::ext::shared_ptr<PiecewiseAtmOptionletCurve<BackwardFlat>> tmp =
                QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<BackwardFlat>>(
                    config.settleDays(), cftvc, index, discountCurve, flatFirstPeriod,
                    volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, BackwardFlat(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseAtmOptionletCurve<BackwardFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, Linear>>(
                asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                tmp->calendar(), tmp->businessDayConvention(), index, tmp->dayCounter(),
                                tmp->volatilityType(), tmp->displacement()));
        } else if (config.timeInterpolation() == "Cubic") {
            QuantLib::ext::shared_ptr<PiecewiseAtmOptionletCurve<Cubic>> tmp =
                QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<Cubic>>(
                    config.settleDays(), cftvc, index, discountCurve, flatFirstPeriod,
                    volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, Cubic(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseAtmOptionletCurve<Cubic, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Linear>>(
                asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                tmp->calendar(), tmp->businessDayConvention(), index, tmp->dayCounter(),
                                tmp->volatilityType(), tmp->displacement()));
        } else if (config.timeInterpolation() == "CubicFlat") {
            QuantLib::ext::shared_ptr<PiecewiseAtmOptionletCurve<CubicFlat>> tmp =
                QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<CubicFlat>>(
                    config.settleDays(), cftvc, index, discountCurve, flatFirstPeriod,
                    volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, CubicFlat(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseAtmOptionletCurve<CubicFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Linear>>(
                asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                tmp->calendar(), tmp->businessDayConvention(), index, tmp->dayCounter(),
                                tmp->volatilityType(), tmp->displacement()));
        } else {
            QL_FAIL("Cap floor config " << config.curveID() << " has unexpected time interpolation "
                                        << config.timeInterpolation());
        }
    } else {
        // Legacy method where we interpolate on the term volatilities.
        // We don't need time interpolation in this instance - we just use the term volatility interpolation.
        if (config.interpolationMethod() == CftvsInterp::BicubicSpline) {
            if (config.flatExtrapolation()) {
                QuantLib::ext::shared_ptr<PiecewiseAtmOptionletCurve<CubicFlat>> tmp =
                    QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<CubicFlat>>(
                        config.settleDays(), cftvc, index, discountCurve, flatFirstPeriod,
                        volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, CubicFlat(),
                        QuantExt::IterativeBootstrap<
                            PiecewiseAtmOptionletCurve<CubicFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                            accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Linear>>(
                    asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                    tmp->calendar(), tmp->businessDayConvention(), index, tmp->dayCounter(),
                                    tmp->volatilityType(), tmp->displacement()));
            } else {
                QuantLib::ext::shared_ptr<PiecewiseAtmOptionletCurve<Cubic>> tmp =
                    QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<Cubic>>(
                        config.settleDays(), cftvc, index, discountCurve, flatFirstPeriod,
                        volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, Cubic(),
                        QuantExt::IterativeBootstrap<
                            PiecewiseAtmOptionletCurve<Cubic, QuantExt::IterativeBootstrap>::optionlet_curve>(
                            accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Linear>>(
                    asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                    tmp->calendar(), tmp->businessDayConvention(), index, tmp->dayCounter(),
                                    tmp->volatilityType(), tmp->displacement()));
            }
        } else if (config.interpolationMethod() == CftvsInterp::Bilinear) {
            if (config.flatExtrapolation()) {
                QuantLib::ext::shared_ptr<PiecewiseAtmOptionletCurve<LinearFlat>> tmp =
                    QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<LinearFlat>>(
                        config.settleDays(), cftvc, index, discountCurve, flatFirstPeriod,
                        volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt,
                        LinearFlat(),
                        QuantExt::IterativeBootstrap<
                            PiecewiseAtmOptionletCurve<LinearFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                            accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Linear>>(
                    asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                    tmp->calendar(), tmp->businessDayConvention(), index, tmp->dayCounter(),
                                    tmp->volatilityType(), tmp->displacement()));
            } else {
                QuantLib::ext::shared_ptr<PiecewiseAtmOptionletCurve<Linear>> tmp =
                    QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<Linear>>(
                        config.settleDays(), cftvc, index, discountCurve, flatFirstPeriod,
                        volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, Linear(),
                        QuantExt::IterativeBootstrap<
                            PiecewiseAtmOptionletCurve<Linear, QuantExt::IterativeBootstrap>::optionlet_curve>(
                            accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Linear>>(
                    asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                    tmp->calendar(), tmp->businessDayConvention(), index, tmp->dayCounter(),
                                    tmp->volatilityType(), tmp->displacement()));
            }
        } else {
            QL_FAIL("Cap floor config " << config.curveID() << " has unexpected interpolation method "
                                        << static_cast<int>(config.interpolationMethod()));
        }
    }
}

void CapFloorVolCurve::termOptSurface(const Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader,
                                  QuantLib::ext::shared_ptr<IborIndex> index, Handle<YieldTermStructure> discountCurve,
                                  Real shift) {

    // Get the cap floor term vol surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> cftvs = capSurface(asof, config, loader);

    // Get the ATM cap floor term vol curve if we are including an ATM curve
    bool includeAtm = config.includeAtm();
    Handle<QuantExt::CapFloorTermVolCurve> cftvc;
    if (includeAtm) {
        cftvc = Handle<QuantExt::CapFloorTermVolCurve>(atmCurve(asof, config, loader));
    }

    // Hardcode some values. Can add them to the CapFloorVolatilityCurveConfig later if needed.
    bool flatFirstPeriod = true;
    VolatilityType optVolType = Normal;
    Real optDisplacement = 0.0;

    // Get configuration values for bootstrap
    Real accuracy = config.bootstrapConfig().accuracy();
    Real globalAccuracy = config.bootstrapConfig().globalAccuracy();
    bool dontThrow = config.bootstrapConfig().dontThrow();
    Size maxAttempts = config.bootstrapConfig().maxAttempts();
    Real maxFactor = config.bootstrapConfig().maxFactor();
    Real minFactor = config.bootstrapConfig().minFactor();
    Size dontThrowSteps = config.bootstrapConfig().dontThrowSteps();

    // Get configuration values for parametric smile
    std::vector<std::vector<std::pair<Real, bool>>> initialModelParameters;
    Size maxCalibrationAttempts = 10;
    Real exitEarlyErrorThreshold = 0.005;
    Real maxAcceptableError = 0.05;
    if (config.parametricSmileConfiguration()) {
        auto alpha = config.parametricSmileConfiguration()->parameter("alpha");
        auto beta = config.parametricSmileConfiguration()->parameter("beta");
        auto nu = config.parametricSmileConfiguration()->parameter("nu");
        auto rho = config.parametricSmileConfiguration()->parameter("rho");
        QL_REQUIRE(alpha.initialValue.size() == beta.initialValue.size() &&
                       alpha.initialValue.size() == nu.initialValue.size() &&
                       alpha.initialValue.size() == rho.initialValue.size(),
                   "CapFloorVolCurve: parametric smile config: alpha size ("
                       << alpha.initialValue.size() << ") beta size (" << beta.initialValue.size() << ") nu size ("
                       << nu.initialValue.size() << ") rho size (" << rho.initialValue.size() << ") must match");
        for (Size i = 0; i < alpha.initialValue.size(); ++i) {
            initialModelParameters.push_back(std::vector<std::pair<Real, bool>>());
            initialModelParameters.back().push_back(std::make_pair(alpha.initialValue[i], alpha.isFixed));
            initialModelParameters.back().push_back(std::make_pair(beta.initialValue[i], beta.isFixed));
            initialModelParameters.back().push_back(std::make_pair(nu.initialValue[i], nu.isFixed));
            initialModelParameters.back().push_back(std::make_pair(rho.initialValue[i], rho.isFixed));
        }
        maxCalibrationAttempts = config.parametricSmileConfiguration()->calibration().maxCalibrationAttempts;
        exitEarlyErrorThreshold = config.parametricSmileConfiguration()->calibration().exitEarlyErrorThreshold;
        maxAcceptableError = config.parametricSmileConfiguration()->calibration().maxAcceptableError;
    }

    // On optionlets is the newly added interpolation approach whereas on term volatilities is legacy
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> optionletStripper;
    VolatilityType volType = volatilityType(config.volatilityType());
    bool onOpt = interpOnOpt(config);
    SabrParametricVolatility::ModelVariant sabrModelVariant;
    if (onOpt) {
        // This is not pretty but can't think of a better way (with template functions and or classes)
        if (config.timeInterpolation() == "Linear") {
            optionletStripper = QuantLib::ext::make_shared<PiecewiseOptionletStripper<Linear>>(
                cftvs, index, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                Linear(),
                QuantExt::IterativeBootstrap<
                    PiecewiseOptionletStripper<Linear, QuantExt::IterativeBootstrap>::optionlet_curve>(
                    accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps),
                config.rateComputationPeriod(), config.onCapSettlementDays());
            if (config.strikeInterpolation() == "Linear") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<Linear, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Linear>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "LinearFlat") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<Linear, LinearFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Linear, LinearFlat>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "Cubic") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<Linear, Cubic>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Cubic>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "CubicFlat") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<Linear, CubicFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Linear, CubicFlat>>(
                    asof, transform(*optionletStripper));
            } else if (tryParse(
                           config.strikeInterpolation(), sabrModelVariant,
                           std::function<QuantExt::SabrParametricVolatility::ModelVariant(const std::string&)>(
                               [](const std::string& s) { return parseSabrParametricVolatilityModelVariant(s); }))) {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<Linear, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::SabrStrippedOptionletAdapter<Linear>>(
                    asof, transform(*optionletStripper), sabrModelVariant, Linear(), boost::none,
                    initialModelParameters, maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);
            } else {
                QL_FAIL("Cap floor config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
            }
        } else if (config.timeInterpolation() == "LinearFlat") {
            optionletStripper = QuantLib::ext::make_shared<PiecewiseOptionletStripper<LinearFlat>>(
                cftvs, index, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                LinearFlat(),
                QuantExt::IterativeBootstrap<
                    PiecewiseOptionletStripper<LinearFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                    accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps),
                config.rateComputationPeriod(), config.onCapSettlementDays());
            if (config.strikeInterpolation() == "Linear") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<LinearFlat, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Linear>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "LinearFlat") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<LinearFlat, LinearFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, LinearFlat>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "Cubic") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<LinearFlat, Cubic>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Cubic>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "CubicFlat") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<LinearFlat, CubicFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, CubicFlat>>(
                    asof, transform(*optionletStripper));
            } else if (tryParse(
                           config.strikeInterpolation(), sabrModelVariant,
                           std::function<QuantExt::SabrParametricVolatility::ModelVariant(const std::string&)>(
                               [](const std::string& s) { return parseSabrParametricVolatilityModelVariant(s); }))) {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<LinearFlat, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::SabrStrippedOptionletAdapter<LinearFlat>>(
                    asof, transform(*optionletStripper), sabrModelVariant, LinearFlat(), boost::none,
                    initialModelParameters, maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);
            } else {
                QL_FAIL("Cap floor config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
            }
        } else if (config.timeInterpolation() == "BackwardFlat") {
            optionletStripper = QuantLib::ext::make_shared<PiecewiseOptionletStripper<BackwardFlat>>(
                cftvs, index, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                BackwardFlat(),
                QuantExt::IterativeBootstrap<
                    PiecewiseOptionletStripper<BackwardFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                    accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps),
                config.rateComputationPeriod(), config.onCapSettlementDays());
            if (config.strikeInterpolation() == "Linear") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<BackwardFlat, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, Linear>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "LinearFlat") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<BackwardFlat, LinearFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, LinearFlat>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "Cubic") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<BackwardFlat, Cubic>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, Cubic>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "CubicFlat") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<BackwardFlat, CubicFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, CubicFlat>>(
                    asof, transform(*optionletStripper));
            } else if (tryParse(
                           config.strikeInterpolation(), sabrModelVariant,
                           std::function<QuantExt::SabrParametricVolatility::ModelVariant(const std::string&)>(
                               [](const std::string& s) { return parseSabrParametricVolatilityModelVariant(s); }))) {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<BackwardFlat, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::SabrStrippedOptionletAdapter<Linear>>(
                    asof, transform(*optionletStripper), sabrModelVariant, Linear(), boost::none,
                    initialModelParameters, maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);
            } else {
                QL_FAIL("Cap floor config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
            }
        } else if (config.timeInterpolation() == "Cubic") {
            optionletStripper = QuantLib::ext::make_shared<PiecewiseOptionletStripper<Cubic>>(
                cftvs, index, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                Cubic(),
                QuantExt::IterativeBootstrap<
                    PiecewiseOptionletStripper<Cubic, QuantExt::IterativeBootstrap>::optionlet_curve>(
                    accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps),
                config.rateComputationPeriod(), config.onCapSettlementDays());
            if (config.strikeInterpolation() == "Linear") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<Cubic, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Linear>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "LinearFlat") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<Cubic, LinearFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, LinearFlat>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "Cubic") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<Cubic, Cubic>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Cubic>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "CubicFlat") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<Cubic, CubicFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, CubicFlat>>(
                    asof, transform(*optionletStripper));
            } else if (tryParse(
                           config.strikeInterpolation(), sabrModelVariant,
                           std::function<QuantExt::SabrParametricVolatility::ModelVariant(const std::string&)>(
                               [](const std::string& s) { return parseSabrParametricVolatilityModelVariant(s); }))) {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<Cubic, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::SabrStrippedOptionletAdapter<Cubic>>(
                    asof, transform(*optionletStripper), sabrModelVariant, Cubic(), boost::none, initialModelParameters,
                    maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);
            }
            {
                QL_FAIL("Cap floor config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
            }
        } else if (config.timeInterpolation() == "CubicFlat") {
            optionletStripper = QuantLib::ext::make_shared<PiecewiseOptionletStripper<CubicFlat>>(
                cftvs, index, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                CubicFlat(),
                QuantExt::IterativeBootstrap<
                    PiecewiseOptionletStripper<CubicFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                    accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps),
                config.rateComputationPeriod(), config.onCapSettlementDays());
            if (config.strikeInterpolation() == "Linear") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<CubicFlat, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Linear>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "LinearFlat") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<CubicFlat, LinearFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, LinearFlat>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "Cubic") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<CubicFlat, Cubic>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Cubic>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "CubicFlat") {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<CubicFlat, CubicFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, CubicFlat>>(
                    asof, transform(*optionletStripper));
            } else if (tryParse(
                           config.strikeInterpolation(), sabrModelVariant,
                           std::function<QuantExt::SabrParametricVolatility::ModelVariant(const std::string&)>(
                               [](const std::string& s) { return parseSabrParametricVolatilityModelVariant(s); }))) {
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<CubicFlat, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::SabrStrippedOptionletAdapter<CubicFlat>>(
                    asof, transform(*optionletStripper), sabrModelVariant, CubicFlat(), boost::none,
                    initialModelParameters, maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);
            } else {
                QL_FAIL("Cap floor config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
            }
        } else {
            QL_FAIL("Cap floor config " << config.curveID() << " has unexpected time interpolation "
                                        << config.timeInterpolation());
        }
    } else {
        // Legacy method where we interpolate on the term volatilities.
        // We don't need time interpolation in this instance - we just use the term volatility interpolation.
        if (config.interpolationMethod() == CftvsInterp::BicubicSpline) {
            if (config.flatExtrapolation()) {
                optionletStripper = QuantLib::ext::make_shared<PiecewiseOptionletStripper<CubicFlat>>(
                    cftvs, index, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                    CubicFlat(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseOptionletStripper<CubicFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps),
                    config.rateComputationPeriod(), config.onCapSettlementDays());
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<CubicFlat, CubicFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, CubicFlat>>(
                    asof, transform(*optionletStripper));
            } else {
                optionletStripper = QuantLib::ext::make_shared<PiecewiseOptionletStripper<Cubic>>(
                    cftvs, index, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                    Cubic(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseOptionletStripper<Cubic, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps),
                    config.rateComputationPeriod(), config.onCapSettlementDays());
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<Cubic, Cubic>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Cubic>>(
                    asof, transform(*optionletStripper));
            }
        } else if (config.interpolationMethod() == CftvsInterp::Bilinear) {
            if (config.flatExtrapolation()) {
                optionletStripper = QuantLib::ext::make_shared<PiecewiseOptionletStripper<LinearFlat>>(
                    cftvs, index, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                    LinearFlat(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseOptionletStripper<LinearFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps),
                    config.rateComputationPeriod(), config.onCapSettlementDays());
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<LinearFlat, LinearFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, LinearFlat>>(
                    asof, transform(*optionletStripper));
            } else {
                optionletStripper = QuantLib::ext::make_shared<PiecewiseOptionletStripper<Linear>>(
                    cftvs, index, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                    Linear(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseOptionletStripper<Linear, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps),
                    config.rateComputationPeriod(), config.onCapSettlementDays());
                if (includeAtm) {
                    optionletStripper = QuantLib::ext::make_shared<OptionletStripperWithAtm<Linear, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Linear>>(
                    asof, transform(*optionletStripper));
            }
        } else {
            QL_FAIL("Cap floor config " << config.curveID() << " has unexpected interpolation method "
                                        << static_cast<int>(config.interpolationMethod()));
        }
    }
}

void CapFloorVolCurve::optAtmOptCurve(const Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader,
                                       QuantLib::ext::shared_ptr<IborIndex> index, Handle<YieldTermStructure> discountCurve,
                                       Real shift) {
    QL_REQUIRE(config.optionalQuotes() == false, "Optional quotes for optionlet volatilities are not supported.");
    // Load optionlet atm vol curve
    bool tenorRelevant = false;
    bool wildcardTenor = false;

    Period tenor = parsePeriod(config.indexTenor()); // 1D, 1M, 3M, 6M, 12M
    string currency = config.currency();
    vector<Period> configTenors;
    std::map<Period, Real> capfloorVols;
    if (config.atmTenors()[0] == "*") {
        wildcardTenor = true;
    } else {
        configTenors = parseVectorOfValues<Period>(config.atmTenors(), &parsePeriod);
    }
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::CAPFLOOR << "/" << config.quoteType() << "/" << currency << "/";
    if (config.quoteIncludesIndexName())
        ss << config.index() << "/";
    ss << "*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {
        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");
        QuantLib::ext::shared_ptr<CapFloorQuote> cfq = QuantLib::ext::dynamic_pointer_cast<CapFloorQuote>(md);
        QL_REQUIRE(cfq, "Internal error: could not downcast MarketDatum '" << md->name() << "' to CapFloorQuote");
        QL_REQUIRE(cfq->ccy() == currency,
                   "CapFloorQuote ccy '" << cfq->ccy() << "' <> config ccy '" << currency << "'");
        if (cfq->underlying() == tenor && cfq->atm()) {
            auto findTenor = std::find(configTenors.begin(), configTenors.end(), cfq->term());
            if (wildcardTenor) {
                tenorRelevant = true;
            } else {
                tenorRelevant = findTenor != configTenors.end();
            }
            if (tenorRelevant) {
                // Check duplicate quotes
                auto k = capfloorVols.find(cfq->term());
                if (k != capfloorVols.end()) {
                    if (config.quoteIncludesIndexName())
                        QL_FAIL("Duplicate optionlet atm vol quote in config "
                                << config.curveID() << ", with underlying tenor " << tenor << " ,currency "
                                << currency << " and index " << config.index() << ", for tenor " << cfq->term());
                    else
                        QL_FAIL("Duplicate optionlet atm vol quote in config "
                                << config.curveID() << ", with underlying tenor " << tenor << " and currency "
                                << currency << ", for tenor " << cfq->term());
                }
                // Store the vols into a map to sort the wildcard tenors
                capfloorVols[cfq->term()]= cfq->quote()->value();
            }
        }
    }
    vector<Real> vols_tenor;
    auto tenor_itr = configTenors.begin();
    for (auto const& vols_outer : capfloorVols) {
        // Check if all tenor is available
        if (wildcardTenor) {
            configTenors.push_back(vols_outer.first);
        } else {
            QL_REQUIRE(*tenor_itr == vols_outer.first, "Quote with tenor " << *tenor_itr
                                                                           << " not loaded for optionlet vol config "
                                                                           << config.curveID());
            tenor_itr++;
        }
        vols_tenor.push_back(vols_outer.second);
    }
    // Find the fixing date of the term quotes
    vector<Date> fixingDates = populateFixingDates(asof, config, index, configTenors);
    DLOG("Found " << capfloorVols.size() << " quotes for optionlet vol surface " << config.curveID());
    // This is not pretty but can't think of a better way (with template functions and or classes)
    // Note: second template argument in StrippedOptionletAdapter doesn't matter so just use Linear here.
    if (config.timeInterpolation() == "Linear") {
        capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Linear>>(
            asof, transform(asof, fixingDates, vols_tenor, config.settleDays(),
                            config.calendar(), config.businessDayConvention(), index, config.dayCounter(),
                            volatilityType(config.volatilityType()), shift));
    } else if (config.timeInterpolation() == "LinearFlat") {
        capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Linear>>(
            asof, transform(asof, fixingDates, vols_tenor, config.settleDays(), config.calendar(),
                            config.businessDayConvention(), index, config.dayCounter(),
                            volatilityType(config.volatilityType()), shift));
    } else if (config.timeInterpolation() == "BackwardFlat") {
        capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, Linear>>(
            asof, transform(asof, fixingDates, vols_tenor, config.settleDays(), config.calendar(),
                            config.businessDayConvention(), index, config.dayCounter(),
                            volatilityType(config.volatilityType()), shift));
    } else if (config.timeInterpolation() == "Cubic") {
        capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Linear>>(
            asof, transform(asof, fixingDates, vols_tenor, config.settleDays(), config.calendar(),
                            config.businessDayConvention(), index, config.dayCounter(),
                            volatilityType(config.volatilityType()), shift));
    } else if (config.timeInterpolation() == "CubicFlat") {
        capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Linear>>(
            asof, transform(asof, fixingDates, vols_tenor, config.settleDays(), config.calendar(),
                            config.businessDayConvention(), index, config.dayCounter(),
                            volatilityType(config.volatilityType()), shift));
    } else {
        QL_FAIL("Cap floor config " << config.curveID() << " has unexpected time interpolation "
                                    << config.timeInterpolation());
    }
}

void CapFloorVolCurve::optOptSurface(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config,
                                     const Loader& loader, QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndex,
                                     QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
                                     QuantLib::Real shift) {
    QL_REQUIRE(config.optionalQuotes() == false, "Optional quotes for optionlet volatilities are not supported.");

    // Get configuration values for parametric smile
    std::vector<std::vector<std::pair<Real,bool>>> initialModelParameters;
    Size maxCalibrationAttempts = 10;
    Real exitEarlyErrorThreshold = 0.005;
    Real maxAcceptableError = 0.05;
    if (config.parametricSmileConfiguration()) {
        auto alpha = config.parametricSmileConfiguration()->parameter("alpha");
        auto beta = config.parametricSmileConfiguration()->parameter("beta");
        auto nu = config.parametricSmileConfiguration()->parameter("nu");
        auto rho = config.parametricSmileConfiguration()->parameter("rho");
        QL_REQUIRE(alpha.initialValue.size() == beta.initialValue.size() &&
                       alpha.initialValue.size() == nu.initialValue.size() &&
                       alpha.initialValue.size() == rho.initialValue.size(),
                   "CapFloorVolCurve: parametric smile config: alpha size ("
                       << alpha.initialValue.size() << ") beta size (" << beta.initialValue.size() << ") nu size ("
                       << nu.initialValue.size() << ") rho size (" << rho.initialValue.size() << ") must match");
        for (Size i = 0; i < alpha.initialValue.size(); ++i) {
            initialModelParameters.push_back(std::vector<std::pair<Real, bool>>());
            initialModelParameters.back().push_back(std::make_pair(alpha.initialValue[i], alpha.isFixed));
            initialModelParameters.back().push_back(std::make_pair(beta.initialValue[i], beta.isFixed));
            initialModelParameters.back().push_back(std::make_pair(nu.initialValue[i], nu.isFixed));
            initialModelParameters.back().push_back(std::make_pair(rho.initialValue[i], rho.isFixed));
        }
        maxCalibrationAttempts = config.parametricSmileConfiguration()->calibration().maxCalibrationAttempts;
        exitEarlyErrorThreshold = config.parametricSmileConfiguration()->calibration().exitEarlyErrorThreshold;
        maxAcceptableError = config.parametricSmileConfiguration()->calibration().maxAcceptableError;
    }

    // Load optionlet vol surface
    Size quoteCounter = 0;
    bool quoteRelevant = false;
    bool tenorRelevant = false;
    bool strikeRelevant = false;
    bool wildcardTenor = false;
    bool atmTenorRelevant = false;
    bool atmWildcardTenor = false;

    Period tenor = parsePeriod(config.indexTenor()); // 1D, 1M, 3M, 6M, 12M
    bool includeAtm = config.includeAtm();
    string currency = config.currency();
    vector<Period> configTenors;
    std::map<Period, std::map<Real, Real>> capfloorVols;
    vector<Period> atmConfigTenors;
    std::map<Period, Real> atmCapFloorVols;
    VolatilityType volType = volatilityType(config.volatilityType());
    bool isOis = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(iborIndex) != nullptr;

    if (config.tenors()[0] == "*") {
        wildcardTenor = true;
    } else {
        configTenors = parseVectorOfValues<Period>(config.tenors(), &parsePeriod);
    }
    if (includeAtm) {
        if (config.atmTenors()[0] == "*") {
            atmWildcardTenor = true;
        } else {
            atmConfigTenors = parseVectorOfValues<Period>(config.atmTenors(), &parsePeriod);
        }
    }
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::CAPFLOOR << "/" << config.quoteType() << "/" << currency << "/";
    if (config.quoteIncludesIndexName())
        ss << config.index() << "/";
    ss << "*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {
        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");
        QuantLib::ext::shared_ptr<CapFloorQuote> cfq = QuantLib::ext::dynamic_pointer_cast<CapFloorQuote>(md);
        QL_REQUIRE(cfq, "Internal error: could not downcast MarketDatum '" << md->name() << "' to CapFloorQuote");
        QL_REQUIRE(cfq->ccy() == currency,
                   "CapFloorQuote ccy '" << cfq->ccy() << "' <> config ccy '" << currency << "'");
        if (cfq->underlying() == tenor) {
            // Surface quotes
            if (!cfq->atm()) {
                auto findTenor = std::find(configTenors.begin(), configTenors.end(), cfq->term());
                if (wildcardTenor) {
                    tenorRelevant = true;
                } else {
                    tenorRelevant = findTenor != configTenors.end();
                }

                Real strike = cfq->strike();
                auto i = std::find_if(config.strikes().begin(), config.strikes().end(),
                                      [&strike](const std::string& x) { return close_enough(parseReal(x), strike); });
                strikeRelevant = i != config.strikes().end();

                quoteRelevant = strikeRelevant && tenorRelevant;

                if (quoteRelevant) {
                    quoteCounter++;
                    // Check duplicate quotes
                    auto k = capfloorVols.find(cfq->term());
                    if (k != capfloorVols.end()) {
                        if (k->second.find(cfq->strike()) != k->second.end()) {
                            if (config.quoteIncludesIndexName())
                                QL_FAIL("Duplicate optionlet vol quote in config "
                                        << config.curveID() << ", with underlying tenor " << tenor << " ,currency "
                                        << currency << " and index " << config.index() << ", for tenor " << cfq->term()
                                        << " and strike " << cfq->strike());
                            else
                                QL_FAIL("Duplicate optionlet vol quote in config "
                                        << config.curveID() << ", with underlying tenor " << tenor << " and currency "
                                        << currency << ", for tenor " << cfq->term() << " and strike "
                                        << cfq->strike());
                        }
                    }
                    // Store the vols into a map to sort the wildcard tenors
                    capfloorVols[cfq->term()][cfq->strike()] = cfq->quote()->value();
                }
            } else if (includeAtm) {
                // atm quotes
                auto findTenor = std::find(atmConfigTenors.begin(), atmConfigTenors.end(), cfq->term());
                if (atmWildcardTenor) {
                    atmTenorRelevant = true;
                } else {
                    atmTenorRelevant = findTenor != atmConfigTenors.end();
                }
                if (atmTenorRelevant) {
                    quoteCounter++;
                    // Check duplicate quotes
                    auto k = atmCapFloorVols.find(cfq->term());
                    if (k != atmCapFloorVols.end()) {
                        if (config.quoteIncludesIndexName())
                            QL_FAIL("Duplicate optionlet atm vol quote in config "
                                    << config.curveID() << ", with underlying tenor " << tenor << " ,currency "
                                    << currency << " and index " << config.index() << ", for tenor " << cfq->term());
                        else
                            QL_FAIL("Duplicate optionlet atm vol quote in config "
                                    << config.curveID() << ", with underlying tenor " << tenor << " and currency "
                                    << currency << ", for tenor " << cfq->term());
                    }
                    // Store the vols into a map to sort the wildcard tenors
                    atmCapFloorVols[cfq->term()] = cfq->quote()->value();
                }
            }   
        }
    }
    vector<Rate> strikes = parseVectorOfValues<Real>(config.strikes(), &parseReal);
    auto tenor_itr = configTenors.begin();
    for (auto const& vols_outer: capfloorVols) {
        // Check if all tenors are available
        if (!wildcardTenor) {
            QL_REQUIRE(*tenor_itr == vols_outer.first, "Quote with tenor " << *tenor_itr
                                                                             << " not loaded for optionlet vol config "
                                                                             << config.curveID());
            tenor_itr++;
        }
        // Check if all strikes are available
        for (Size j = 0; j < strikes.size(); j++) {
            auto it = vols_outer.second.find(strikes[j]);
            QL_REQUIRE(it != vols_outer.second.end(),
                       "Quote with tenor " << vols_outer.first << " and strike " << strikes[j]
                                                                    << " not loaded for optionlet vol config "
                                                                    << config.curveID());
        }
    }
    std::map<Date, Date> tenorMap; 
    if (includeAtm) {
        // Check if all tenor for atm quotes exists
        if (!atmWildcardTenor) {
            auto tenor_itr = atmConfigTenors.begin();
            for (auto const& vols_outer : atmCapFloorVols) {
                QL_REQUIRE(*tenor_itr == vols_outer.first, "Quote with tenor "
                                                               << *tenor_itr << " not loaded for optionlet vol config "
                                                               << config.curveID());
                tenor_itr++;
            }
        }
        // Add tenor to the mapping
        for (auto const& vols_outer : atmCapFloorVols) {
            capfloorVols[vols_outer.first];
        }
    }
    // Find the fixing date of the term quotes
    vector<Period> tenors;
    for (auto const& vols_outer : capfloorVols) {
        tenors.push_back(vols_outer.first);
    }
    // Find the fixing date of the term quotes
    vector<Date> fixingDates = populateFixingDates(asof, config, iborIndex, tenors);

    // populate strikes for atm quotes
    if (includeAtm){
        Rate atmStrike;
        for (auto const& vols_outer : atmCapFloorVols) {
            auto it = find(tenors.begin(), tenors.end(), vols_outer.first);
            Size ind = it - tenors.begin();
            if (isOis) {
                atmStrike = getOisAtmLevel(QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(iborIndex), fixingDates[ind],
                                           config.rateComputationPeriod());
            } else {
                atmStrike = iborIndex->forecastFixing(fixingDates[ind]);
            }
            if (capfloorVols[vols_outer.first].find(atmStrike) == capfloorVols[vols_outer.first].end()) {
                capfloorVols[vols_outer.first][atmStrike] = vols_outer.second;
            }
        }
    }
    vector<vector<Rate>> strikes_vec;
    vector<Rate> strikes_tenor;
    vector<vector<Handle<Quote>>> vols_vec;
    vector<Handle<Quote>> vols_tenor;
    for (auto const& vols_outer : capfloorVols) {
        for (auto const& vols_inner : vols_outer.second) {
            vols_tenor.push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(vols_inner.second)));
            strikes_tenor.push_back(vols_inner.first);
        }
        tenors.push_back(vols_outer.first);
        strikes_vec.push_back(strikes_tenor);
        strikes_tenor.clear();
        vols_vec.push_back(vols_tenor);
        vols_tenor.clear();
    }
    DLOG("Found " << quoteCounter << " quotes for optionlet vol surface " << config.curveID());
    QuantLib::ext::shared_ptr<StrippedOptionlet> optionletSurface;

    // Return for the cap floor term volatility surface
    optionletSurface = QuantLib::ext::make_shared<StrippedOptionlet>(
        config.settleDays(), config.calendar(), config.businessDayConvention(), iborIndex, fixingDates, strikes_vec,
        vols_vec, config.dayCounter(), volType, shift);

    SabrParametricVolatility::ModelVariant sabrModelVariant;

    // This is not pretty but can't think of a better way (with template functions and or classes)
    if (config.timeInterpolation() == "Linear") {
        if (config.strikeInterpolation() == "Linear") {
            capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Linear>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "LinearFlat") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Linear, LinearFlat>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "Cubic") {
            capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Cubic>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "CubicFlat") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Linear, CubicFlat>>(asof, optionletSurface);
        } else if (tryParse(config.strikeInterpolation(), sabrModelVariant,
                            std::function<QuantExt::SabrParametricVolatility::ModelVariant(const std::string&)>(
                                [](const std::string& s) { return parseSabrParametricVolatilityModelVariant(s); }))) {
            capletVol_ = QuantLib::ext::make_shared<QuantExt::SabrStrippedOptionletAdapter<Linear>>(
                asof, optionletSurface, sabrModelVariant, Linear(), boost::none, initialModelParameters,
                maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);
        } else {
            QL_FAIL("Optionlet vol config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
        }
    } else if (config.timeInterpolation() == "LinearFlat") {
        if (config.strikeInterpolation() == "Linear") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Linear>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "LinearFlat") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, LinearFlat>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "Cubic") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Cubic>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "CubicFlat") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, CubicFlat>>(asof, optionletSurface);
        } else if (tryParse(config.strikeInterpolation(), sabrModelVariant,
                            std::function<QuantExt::SabrParametricVolatility::ModelVariant(const std::string&)>(
                                [](const std::string& s) { return parseSabrParametricVolatilityModelVariant(s); }))) {
            capletVol_ = QuantLib::ext::make_shared<QuantExt::SabrStrippedOptionletAdapter<LinearFlat>>(
                asof, optionletSurface, sabrModelVariant, LinearFlat(), boost::none, initialModelParameters,
                maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);
        } else {
            QL_FAIL("Optionlet vol config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
        }
    } else if (config.timeInterpolation() == "BackwardFlat") {
        if (config.strikeInterpolation() == "Linear") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, Linear>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "LinearFlat") {
            capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, LinearFlat>>(
                asof, optionletSurface);
        } else if (config.strikeInterpolation() == "Cubic") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, Cubic>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "CubicFlat") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, CubicFlat>>(asof, optionletSurface);
        } else if (tryParse(config.strikeInterpolation(), sabrModelVariant,
                            std::function<QuantExt::SabrParametricVolatility::ModelVariant(const std::string&)>(
                                [](const std::string& s) { return parseSabrParametricVolatilityModelVariant(s); }))) {
            capletVol_ = QuantLib::ext::make_shared<QuantExt::SabrStrippedOptionletAdapter<BackwardFlat>>(
                asof, optionletSurface, sabrModelVariant, BackwardFlat(), boost::none, initialModelParameters,
                maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);
        } else {
            QL_FAIL("Optionlet vol config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
        }
    } else if (config.timeInterpolation() == "Cubic") {
        if (config.strikeInterpolation() == "Linear") {
            capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Linear>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "LinearFlat") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, LinearFlat>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "Cubic") {
            capletVol_ = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Cubic>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "CubicFlat") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, CubicFlat>>(asof, optionletSurface);
        } else if (tryParse(config.strikeInterpolation(), sabrModelVariant,
                            std::function<QuantExt::SabrParametricVolatility::ModelVariant(const std::string&)>(
                                [](const std::string& s) { return parseSabrParametricVolatilityModelVariant(s); }))) {
            capletVol_ = QuantLib::ext::make_shared<QuantExt::SabrStrippedOptionletAdapter<Cubic>>(
                asof, optionletSurface, sabrModelVariant, Cubic(), boost::none, initialModelParameters,
                maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);
        } else {
            QL_FAIL("Optionlet vol config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
        }
    } else if (config.timeInterpolation() == "CubicFlat") {
        if (config.strikeInterpolation() == "Linear") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Linear>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "LinearFlat") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, LinearFlat>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "Cubic") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Cubic>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "CubicFlat") {
            capletVol_ =
                QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, CubicFlat>>(asof, optionletSurface);
        } else if (tryParse(config.strikeInterpolation(), sabrModelVariant,
                            std::function<QuantExt::SabrParametricVolatility::ModelVariant(const std::string&)>(
                                [](const std::string& s) { return parseSabrParametricVolatilityModelVariant(s); }))) {
            capletVol_ = QuantLib::ext::make_shared<QuantExt::SabrStrippedOptionletAdapter<CubicFlat>>(
                asof, optionletSurface, sabrModelVariant, CubicFlat(), boost::none, initialModelParameters,
                maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);
        } else {
            QL_FAIL("Optionlet vol config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
        }
    } else {
        QL_FAIL("Optionlet vol config " << config.curveID() << " has unexpected time interpolation "
                                        << config.timeInterpolation());
    }
}

QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface>
CapFloorVolCurve::capSurface(const Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader) const {

    // Map to store the quote values that we load with key (period, strike) where strike
    // needs a custom comparator to avoid == with double
    auto comp = [](const pair<Period, Rate>& a, const pair<Period, Rate>& b) {
        return (a.first < b.first) || (!(b.first < a.first) && (a.second < b.second && !close(a.second, b.second)));
    };
    map<pair<Period, Rate>, Real, decltype(comp)> volQuotes(comp);

    bool optionalQuotes = config.optionalQuotes();
    Size quoteCounter = 0;
    bool quoteRelevant = false;
    bool tenorRelevant = false;
    bool strikeRelevant = false;

    vector<Real> qtStrikes;
    vector<Real> qtData;
    vector<Period> qtTenors;
    Period tenor = parsePeriod(config.indexTenor()); // 1D, 1M, 3M, 6M, 12M
    string currency = config.currency();
    vector<Period> configTenors = parseVectorOfValues<Period>(config.tenors(), &parsePeriod);

    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::CAPFLOOR << "/" << config.quoteType() << "/" << currency << "/";
    if (config.quoteIncludesIndexName())
        ss << config.index() << "/";
    ss << "*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {
        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");
        QuantLib::ext::shared_ptr<CapFloorQuote> cfq = QuantLib::ext::dynamic_pointer_cast<CapFloorQuote>(md);
        QL_REQUIRE(cfq, "Internal error: could not downcast MarketDatum '" << md->name() << "' to CapFloorQuote");
        QL_REQUIRE(cfq->ccy() == currency, "CapFloorQuote ccy '" << cfq->ccy() << "' <> config ccy '" << currency << "'");
        if (cfq->underlying() == tenor && !cfq->atm()) {
            auto j = std::find(configTenors.begin(), configTenors.end(), cfq->term());
            tenorRelevant = j != configTenors.end();

            Real strike = cfq->strike();
            auto i = std::find_if(config.strikes().begin(), config.strikes().end(),
                [&strike](const std::string& x) {
                return close_enough(parseReal(x), strike);
            });
            strikeRelevant = i != config.strikes().end();

            quoteRelevant = strikeRelevant && tenorRelevant;

            if (quoteRelevant) {
                // if we have optional quotes we just make vectors of all quotes and let the sparse surface
                // handle them
                quoteCounter++;
                if (optionalQuotes) {
                    qtStrikes.push_back(cfq->strike());
                    qtTenors.push_back(cfq->term());
                    qtData.push_back(cfq->quote()->value());
                }
                auto key = make_pair(cfq->term(), cfq->strike());
                auto r = volQuotes.insert(make_pair(key, cfq->quote()->value()));
                if (config.quoteIncludesIndexName())
                    QL_REQUIRE(r.second, "Duplicate cap floor quote in config " << config.curveID() << ", with underlying tenor " << tenor <<
                        " ,currency " << currency << " and index " << config.index() << ", for tenor " << key.first << " and strike " << key.second);
                else
                    QL_REQUIRE(r.second, "Duplicate cap floor quote in config " << config.curveID() << ", with underlying tenor " << tenor <<
                        " and currency " << currency << ", for tenor " << key.first << " and strike " << key.second);
            }
        }
    }

    Size totalQuotes = config.tenors().size() * config.strikes().size();
    if (quoteCounter < totalQuotes) {
        WLOG("Found only " << quoteCounter << " out of " << totalQuotes << " quotes for CapFloor surface " << config.curveID());
    }

    vector<Period> tenors = parseVectorOfValues<Period>(config.tenors(), &parsePeriod);
    vector<Rate> strikes = parseVectorOfValues<Real>(config.strikes(), &parseReal);
    Matrix vols(tenors.size(), strikes.size());
    for (Size i = 0; i < tenors.size(); i++) {
        for (Size j = 0; j < strikes.size(); j++) {
            auto key = make_pair(tenors[i], strikes[j]);
            auto it = volQuotes.find(key);
            if (!optionalQuotes) {
                QL_REQUIRE(it != volQuotes.end(), "Quote with tenor " << key.first << " and strike " << key.second
                    << " not loaded for cap floor config "
                    << config.curveID());
                // Organise the values in to a square matrix
                vols[i][j] = it->second;
            } else {
                if (it == volQuotes.end()) {
                    DLOG("Could not find quote with tenor " << key.first << " and strike " << key.second <<
                        " for cap floor config " << config.curveID());
                }
            }
        }
    }

    DLOG("Found " << quoteCounter << " quotes for capfloor surface " << config.curveID());
    if (optionalQuotes) {
        QL_REQUIRE(quoteCounter > 0, "No Quotes provided for CapFloor surface " << config.curveID());
        if (config.interpolationMethod() == CapFloorTermVolSurfaceExact::Bilinear) {
            return QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceSparse<Linear, Linear>>(
                config.settleDays(), config.calendar(), config.businessDayConvention(), config.dayCounter(), qtTenors,
                qtStrikes, qtData, true, true);
        } else if (config.interpolationMethod() == CapFloorTermVolSurfaceExact::BicubicSpline) {
            return QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceSparse<Cubic, Cubic>>(
                config.settleDays(), config.calendar(), config.businessDayConvention(), config.dayCounter(), qtTenors,
                qtStrikes, qtData, true, true);
        } else {
            QL_FAIL("Invalid Interpolation method for capfloor surface " << config.curveID() << ", must be either "
                << CapFloorTermVolSurfaceExact::Bilinear << " or " << CapFloorTermVolSurfaceExact::BicubicSpline << ".");
        }
    } else {
        // Return for the cap floor term volatility surface
        return QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(config.settleDays(), config.calendar(),
            config.businessDayConvention(), tenors, strikes, vols,
            config.dayCounter(), config.interpolationMethod());
    }        
}

QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolCurve>
CapFloorVolCurve::atmCurve(const Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader) const {

    // Map to store the quote values
    map<Period, Handle<Quote>> volQuotes;

    bool optionalQuotes = config.optionalQuotes();
    Period tenor = parsePeriod(config.indexTenor()); // 1D, 1M, 3M, 6M, 12M
    string currency = config.currency();
    // Load the relevant quotes
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::CAPFLOOR << "/" << config.quoteType() << "/" << currency << "/*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {
        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");
        QuantLib::ext::shared_ptr<CapFloorQuote> cfq = QuantLib::ext::dynamic_pointer_cast<CapFloorQuote>(md);
        QL_REQUIRE(cfq, "Internal error: could not downcast MarketDatum '" << md->name() << "' to CapFloorQuote");
        QL_REQUIRE(cfq->ccy() == currency, "CapFloorQuote ccy '" << cfq->ccy() << "' <> config ccy '" << currency << "'");
        if (cfq->underlying() == tenor && cfq->atm()) {
            auto j = std::find(config.atmTenors().begin(), config.atmTenors().end(), to_string(cfq->term()));
            if (j != config.atmTenors().end()) {
                auto r = volQuotes.insert(make_pair(cfq->term(), cfq->quote()));
                QL_REQUIRE(r.second, "Duplicate ATM cap floor quote in config " << config.curveID() << " for tenor "
                    << cfq->term());
            }
        }
    }

    // Check that the loaded quotes cover all of the configured ATM tenors
    vector<Period> tenors = parseVectorOfValues<Period>(config.atmTenors(), &parsePeriod);
    vector<Handle<Quote>> vols;
    vector<Period> quoteTenors;
    if (!optionalQuotes) {
        vols.resize(tenors.size());
        quoteTenors = tenors;
    }
    for (Size i = 0; i < tenors.size(); i++) {
        auto it = volQuotes.find(tenors[i]);
        if (!optionalQuotes) {
            QL_REQUIRE(it != volQuotes.end(),
                "ATM cap floor quote in config " << config.curveID() << " for tenor " << tenors[i] << " not found ");
            vols[i] = it->second;
        } else {
            if (it == volQuotes.end()) {
                DLOG("Could not find ATM cap floor quote with tenor " << tenors[i] << " for cap floor config " << config.curveID());
            } else {
                vols.push_back(it->second);
                quoteTenors.push_back(it->first);
            }
        }
    }

    if (optionalQuotes) {
        QL_REQUIRE(vols.size() > 0, "No ATM cap floor quotes found for cap floor config " << config.curveID());
        if (vols.size() == 1)
            WLOG("Only one ATM cap floor quote found for cap floor config " << config.curveID() << ", using constant volatility");
    }

    // Return for the cap floor ATM term volatility curve
    // The interpolation here is also based on the interpolation method parameter in the configuration
    // Flat first period is true by default (see ctor)
    if (config.interpolationMethod() == CftvsInterp::BicubicSpline) {
        if (config.flatExtrapolation()) {
            return QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<CubicFlat>>(
                config.settleDays(), config.calendar(), config.businessDayConvention(), quoteTenors, vols,
                config.dayCounter());
        } else {
            return QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Cubic>>(config.settleDays(), config.calendar(),
                                                                               config.businessDayConvention(), quoteTenors,
                                                                               vols, config.dayCounter());
        }
    } else if (config.interpolationMethod() == CftvsInterp::Bilinear) {
        if (config.flatExtrapolation()) {
            return QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<LinearFlat>>(
                config.settleDays(), config.calendar(), config.businessDayConvention(), quoteTenors, vols,
                config.dayCounter());
        } else {
            return QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Linear>>(config.settleDays(), config.calendar(),
                                                                                config.businessDayConvention(), quoteTenors,
                                                                                vols, config.dayCounter());
        }
    } else {
        QL_FAIL("Cap floor config " << config.curveID() << " has unexpected interpolation method "
                                    << static_cast<int>(config.interpolationMethod()));
    }
}

Real CapFloorVolCurve::shiftQuote(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config,
                                  const Loader& loader) const {

    QL_REQUIRE(config.volatilityType() == CfgVolType::ShiftedLognormal,
               "Method shiftQuote should not be called with a config who's volatility type is not ShiftedLognormal");

    // Search for the shift quote in the configured quotes
    for (const string& quoteId : config.quotes()) {

        QuantLib::ext::shared_ptr<MarketDatum> md = loader.get(quoteId, asof);

        // If it is a shift quote
        if (QuantLib::ext::shared_ptr<CapFloorShiftQuote> sq = QuantLib::ext::dynamic_pointer_cast<CapFloorShiftQuote>(md)) {
            return sq->quote()->value();
        }
    }

    QL_FAIL("Could not find a shift quote for cap floor config " << config.curveID());
}

// This method is used to pull out the stripped optionlets from the QuantExt::OptionletStripper instance that is
// bootstrapped. The reason is that we do not want all of the cap floor helpers and their coupons in scope during
// a potential XVA run as this leads to delays when fixings are updated.
QuantLib::ext::shared_ptr<StrippedOptionlet> CapFloorVolCurve::transform(const QuantExt::OptionletStripper& os) const {

    vector<vector<Handle<Quote>>> vols(os.optionletFixingDates().size());
    for (Size i = 0; i < os.optionletFixingDates().size(); i++) {
        for (Size j = 0; j < os.optionletVolatilities(i).size(); j++) {
            vols[i].push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(os.optionletVolatilities(i)[j])));
        }
    }

    vector<vector<Real>> optionletStrikes;
    for (Size i = 0; i < os.optionletFixingDates().size(); i++) {
        optionletStrikes.push_back(os.optionletStrikes(i));
    }

    QuantLib::ext::shared_ptr<StrippedOptionlet> res = QuantLib::ext::make_shared<StrippedOptionlet>(
        os.settlementDays(), os.calendar(), os.businessDayConvention(), os.index(), os.optionletFixingDates(),
        optionletStrikes, vols, os.dayCounter(), os.volatilityType(), os.displacement(), os.atmOptionletRates());

    res->unregisterWithAll();

    return res;
}

QuantLib::ext::shared_ptr<StrippedOptionlet>
CapFloorVolCurve::transform(const Date& asof, vector<Date> dates, const vector<Volatility>& volatilities,
                            Natural settleDays, const Calendar& cal, BusinessDayConvention bdc,
                            QuantLib::ext::shared_ptr<IborIndex> index, const DayCounter& dc, VolatilityType type,
                            Real displacement) const {

    vector<vector<Handle<Quote>>> vols(dates.size());
    for (Size i = 0; i < dates.size(); i++) {
        vols[i].push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(volatilities[i])));
    }

    // Temp fix because QuantLib::StrippedOptionlet requires optionlet dates strictly greater than
    // the evaluation date. Would rather relax this to '>=' in QuantLib::StrippedOptionlet
    if (dates[0] == asof) {
        dates[0]++;
    }

    vector<Rate> strikes = {0.0};
    QuantLib::ext::shared_ptr<StrippedOptionlet> res = QuantLib::ext::make_shared<StrippedOptionlet>(
        settleDays, cal, bdc, index, dates, strikes, vols, dc, type, displacement);

    res->unregisterWithAll();

    return res;
}

vector<Date> CapFloorVolCurve::populateFixingDates(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config,
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndex, const vector<Period>& configTenors) {
    // Find the fixing date of the term quotes
    vector<Date> fixingDates;
    bool isOis = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(iborIndex) != nullptr;
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> dummyEngine =
        QuantLib::ext::make_shared<BlackCapFloorEngine>(iborIndex->forwardingTermStructure(), 0.20, config.dayCounter());
    for (Size i = 0; i < configTenors.size(); i++) {
        if (isOis) {
            Leg dummyCap =
                MakeOISCapFloor(CapFloor::Cap, configTenors[i], QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(iborIndex),
                                config.rateComputationPeriod(), 0.04)
                    .withTelescopicValueDates(true)
                    .withSettlementDays(config.settleDays());
            auto lastCoupon = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(dummyCap.back());
            QL_REQUIRE(lastCoupon, "OptionletStripper::populateDates(): expected CappedFlooredOvernightIndexedCoupon");
            fixingDates.push_back(std::max(asof + 1, lastCoupon->underlying()->fixingDates().front()));
        } else {
            CapFloor dummyCap =
                MakeCapFloor(CapFloor::Cap, configTenors[i], iborIndex, 0.04, 0 * Days).withPricingEngine(dummyEngine);
            QuantLib::ext::shared_ptr<FloatingRateCoupon> lastCoupon = dummyCap.lastFloatingRateCoupon();
            fixingDates.push_back(std::max(asof + 1, lastCoupon->fixingDate()));
        }
    }
    return fixingDates;
}
void CapFloorVolCurve::buildCalibrationInfo(const Date& asof, const CurveConfigurations& curveConfigs,
                                            const QuantLib::ext::shared_ptr<CapFloorVolatilityCurveConfig> config,
                                            const QuantLib::ext::shared_ptr<IborIndex>& index) {
    DLOG("Building calibration info for cap floor vols");

    ReportConfig rc = effectiveReportConfig(curveConfigs.reportConfigIrCapFloorVols(), config->reportConfig());
    bool reportOnStrikeGrid = *rc.reportOnStrikeGrid();
    bool reportOnStrikeSpreadGrid = *rc.reportOnStrikeSpreadGrid();
    std::vector<Real> strikes = *rc.strikes();
    std::vector<Real> strikeSpreads = *rc.strikeSpreads();
    std::vector<Period> expiries = *rc.expiries();
    std::vector<Period> underlyingTenorsReport(1, index->tenor());

    calibrationInfo_ = QuantLib::ext::make_shared<IrVolCalibrationInfo>();

    calibrationInfo_->dayCounter = config->dayCounter().empty() ? "na" : config->dayCounter().name();
    calibrationInfo_->calendar = config->calendar().empty() ? "na" : config->calendar().name();
    calibrationInfo_->volatilityType = ore::data::to_string(capletVol_->volatilityType());
    calibrationInfo_->underlyingTenors = underlyingTenorsReport;

    bool isOis = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index) != nullptr;

    Size onSettlementDays = 0;
    if (isOis) {
        onSettlementDays = config->onCapSettlementDays();
    }

    std::vector<Real> times;                 // fixing times of caplets
    std::vector<std::vector<Real>> forwards; // fair rates of caplets
    for (auto const& p : expiries) {
        Date fixingDate;
        Real forward;
        if (isOis) {
            Leg dummyCap = MakeOISCapFloor(CapFloor::Cap, p, QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index),
                                           config->rateComputationPeriod(), 0.04)
                               .withTelescopicValueDates(true)
                               .withSettlementDays(onSettlementDays);
            if (dummyCap.empty())
                continue;
            auto lastCoupon = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(dummyCap.back());
            QL_REQUIRE(lastCoupon, "OptionletStripper::populateDates(): expected CappedFlooredOvernightIndexedCoupon");
            fixingDate = std::max(asof + 1, lastCoupon->underlying()->fixingDates().front());
            forward = lastCoupon->underlying()->rate();
        } else {
            CapFloor dummyCap = MakeCapFloor(CapFloor::Cap, p, index, 0.04, 0 * Days);
            if (dummyCap.floatingLeg().empty())
                continue;
            QuantLib::ext::shared_ptr<FloatingRateCoupon> lastCoupon = dummyCap.lastFloatingRateCoupon();
            fixingDate = lastCoupon->fixingDate();
            forward = index->fixing(fixingDate);
        }
        calibrationInfo_->expiryDates.push_back(fixingDate);
        times.push_back(capletVol_->dayCounter().empty() ? Actual365Fixed().yearFraction(asof, fixingDate)
                                                         : capletVol_->timeFromReference(fixingDate));
        forwards.push_back(std::vector<Real>(1, forward));
    }

    calibrationInfo_->times = times;
    calibrationInfo_->forwards = forwards;

    std::vector<std::vector<std::vector<Real>>> callPricesStrike(
        times.size(),
        std::vector<std::vector<Real>>(underlyingTenorsReport.size(), std::vector<Real>(strikes.size(), 0.0)));
    std::vector<std::vector<std::vector<Real>>> callPricesStrikeSpread(
        times.size(),
        std::vector<std::vector<Real>>(underlyingTenorsReport.size(), std::vector<Real>(strikeSpreads.size(), 0.0)));

    calibrationInfo_->isArbitrageFree = true;

    if (reportOnStrikeGrid) {
        calibrationInfo_->strikes = strikes;
        calibrationInfo_->strikeGridStrikes = std::vector<std::vector<std::vector<Real>>>(
            times.size(),
            std::vector<std::vector<Real>>(underlyingTenorsReport.size(), std::vector<Real>(strikes.size(), 0.0)));
        calibrationInfo_->strikeGridProb = std::vector<std::vector<std::vector<Real>>>(
            times.size(),
            std::vector<std::vector<Real>>(underlyingTenorsReport.size(), std::vector<Real>(strikes.size(), 0.0)));
        calibrationInfo_->strikeGridImpliedVolatility = std::vector<std::vector<std::vector<Real>>>(
            times.size(),
            std::vector<std::vector<Real>>(underlyingTenorsReport.size(), std::vector<Real>(strikes.size(), 0.0)));
        calibrationInfo_->strikeGridCallSpreadArbitrage = std::vector<std::vector<std::vector<bool>>>(
            times.size(),
            std::vector<std::vector<bool>>(underlyingTenorsReport.size(), std::vector<bool>(strikes.size(), true)));
        calibrationInfo_->strikeGridButterflyArbitrage = std::vector<std::vector<std::vector<bool>>>(
            times.size(),
            std::vector<std::vector<bool>>(underlyingTenorsReport.size(), std::vector<bool>(strikes.size(), true)));
        TLOG("Strike surface arbitrage analysis result:");
        for (Size u = 0; u < underlyingTenorsReport.size(); ++u) {
            TLOG("Underlying tenor " << underlyingTenorsReport[u]);
            for (Size i = 0; i < times.size(); ++i) {
                Real t = times[i];
                Real shift = capletVol_->volatilityType() == Normal ? 0.0 : capletVol_->displacement();
                bool validSlice = true;
                for (Size j = 0; j < strikes.size(); ++j) {
                    try {
                        Real stddev = 0.0;
                        if (capletVol_->volatilityType() == ShiftedLognormal) {
                            if ((strikes[j] > -shift || close_enough(strikes[j], -shift)) &&
                                (forwards[i][u] > -shift || close_enough(forwards[i][u], -shift))) {
                                stddev =
                                    std::sqrt(capletVol_->blackVariance(t, strikes[j]));
                                callPricesStrike[i][u][j] =
                                    blackFormula(Option::Type::Call, strikes[j], forwards[i][u], stddev);
                            }
                        } else {
                            stddev = std::sqrt(capletVol_->blackVariance(t, strikes[j]));
                            callPricesStrike[i][u][j] =
                                bachelierBlackFormula(Option::Type::Call, strikes[j], forwards[i][u], stddev);
                        }
                        calibrationInfo_->strikeGridStrikes[i][u][j] = strikes[j];
                        calibrationInfo_->strikeGridImpliedVolatility[i][u][j] = stddev / std::sqrt(t);
                    } catch (const std::exception& e) {
                        validSlice = false;
                        TLOG("error for time " << t << " strike " << strikes[j] << ": " << e.what());
                    }
                }
                if (validSlice) {
                    try {
                        QuantExt::CarrMadanMarginalProbabilitySafeStrikes cm(calibrationInfo_->strikeGridStrikes[i][u],
                                                                             forwards[i][u], callPricesStrike[i][u],
                                                                             capletVol_->volatilityType(), shift);
                        calibrationInfo_->strikeGridCallSpreadArbitrage[i][u] = cm.callSpreadArbitrage();
                        calibrationInfo_->strikeGridButterflyArbitrage[i][u] = cm.butterflyArbitrage();
                        if (!cm.arbitrageFree())
                            calibrationInfo_->isArbitrageFree = false;
                        calibrationInfo_->strikeGridProb[i][u] = cm.density();
                        TLOGGERSTREAM(arbitrageAsString(cm));
                    } catch (const std::exception& e) {
                        TLOG("error for time " << t << ": " << e.what());
                        calibrationInfo_->isArbitrageFree = false;
                        TLOGGERSTREAM("..(invalid slice)..");
                    }
                } else {
                    TLOGGERSTREAM("..(invalid slice)..");
                }
            }
        }
        TLOG("Strike cube arbitrage analysis completed.");
    }

    if (reportOnStrikeSpreadGrid) {
        calibrationInfo_->strikeSpreads = strikeSpreads;
        calibrationInfo_->strikeSpreadGridStrikes = std::vector<std::vector<std::vector<Real>>>(
            times.size(), std::vector<std::vector<Real>>(underlyingTenorsReport.size(),
                                                         std::vector<Real>(strikeSpreads.size(), 0.0)));
        calibrationInfo_->strikeSpreadGridProb = std::vector<std::vector<std::vector<Real>>>(
            times.size(), std::vector<std::vector<Real>>(underlyingTenorsReport.size(),
                                                         std::vector<Real>(strikeSpreads.size(), 0.0)));
        calibrationInfo_->strikeSpreadGridImpliedVolatility = std::vector<std::vector<std::vector<Real>>>(
            times.size(), std::vector<std::vector<Real>>(underlyingTenorsReport.size(),
                                                         std::vector<Real>(strikeSpreads.size(), 0.0)));
        calibrationInfo_->strikeSpreadGridCallSpreadArbitrage = std::vector<std::vector<std::vector<bool>>>(
            times.size(), std::vector<std::vector<bool>>(underlyingTenorsReport.size(),
                                                         std::vector<bool>(strikeSpreads.size(), true)));
        calibrationInfo_->strikeSpreadGridButterflyArbitrage = std::vector<std::vector<std::vector<bool>>>(
            times.size(), std::vector<std::vector<bool>>(underlyingTenorsReport.size(),
                                                         std::vector<bool>(strikeSpreads.size(), true)));
        TLOG("Strike Spread surface arbitrage analysis result:");
        for (Size u = 0; u < underlyingTenorsReport.size(); ++u) {
            TLOG("Underlying tenor " << underlyingTenorsReport[u]);
            for (Size i = 0; i < times.size(); ++i) {
                Real t = times[i];
                Real shift = capletVol_->volatilityType() == Normal ? 0.0 : capletVol_->displacement();
                bool validSlice = true;
                for (Size j = 0; j < strikeSpreads.size(); ++j) {
                    Real strike = forwards[i][u] + strikeSpreads[j];
                    try {
                        Real stddev = 0.0;
                        if (capletVol_->volatilityType() == ShiftedLognormal) {
                            if ((strike > -shift || close_enough(strike, -shift)) &&
                                (forwards[i][u] > -shift || close_enough(forwards[i][u], -shift))) {
                                stddev = std::sqrt(capletVol_->blackVariance(t, strike));
                                callPricesStrikeSpread[i][u][j] =
                                    blackFormula(Option::Type::Call, strike, forwards[i][u], stddev);
                            }
                        } else {
                            stddev = std::sqrt(capletVol_->blackVariance(t, strike));
                            callPricesStrikeSpread[i][u][j] =
                                bachelierBlackFormula(Option::Type::Call, strike, forwards[i][u], stddev);
                        }
                        calibrationInfo_->strikeSpreadGridStrikes[i][u][j] = strike;
                        calibrationInfo_->strikeSpreadGridImpliedVolatility[i][u][j] = stddev / std::sqrt(t);
                    } catch (const std::exception& e) {
                        validSlice = false;
                        TLOG("error for time " << t << " strike spread " << strikeSpreads[j] << " strike " << strike
                                               << ": " << e.what());
                    }
                }
                if (validSlice) {
                    try {
                        QuantExt::CarrMadanMarginalProbabilitySafeStrikes cm(
                            calibrationInfo_->strikeSpreadGridStrikes[i][u], forwards[i][u],
                            callPricesStrikeSpread[i][u], capletVol_->volatilityType(), shift);
                        calibrationInfo_->strikeSpreadGridCallSpreadArbitrage[i][u] = cm.callSpreadArbitrage();
                        calibrationInfo_->strikeSpreadGridButterflyArbitrage[i][u] = cm.butterflyArbitrage();
                        if (!cm.arbitrageFree())
                            calibrationInfo_->isArbitrageFree = false;
                        calibrationInfo_->strikeSpreadGridProb[i][u] = cm.density();
                        TLOGGERSTREAM(arbitrageAsString(cm));
                    } catch (const std::exception& e) {
                        TLOG("error for time " << t << ": " << e.what());
                        calibrationInfo_->isArbitrageFree = false;
                        TLOGGERSTREAM("..(invalid slice)..");
                    }
                } else {
                    TLOGGERSTREAM("..(invalid slice)..");
                }
            }
        }
        TLOG("Strike Spread cube arbitrage analysis completed.");
    }

    // output SABR calibration to log, if SABR was used

    QuantLib::ext::shared_ptr<QuantExt::SabrParametricVolatility> p;
    if (auto s = QuantLib::ext::dynamic_pointer_cast<SabrStrippedOptionletAdapter<Linear>>(capletVol_))
        p = QuantExt::ext::dynamic_pointer_cast<SabrParametricVolatility>(s->parametricVolatility());
    else if (auto s = QuantLib::ext::dynamic_pointer_cast<SabrStrippedOptionletAdapter<LinearFlat>>(capletVol_))
        p = QuantExt::ext::dynamic_pointer_cast<SabrParametricVolatility>(s->parametricVolatility());
    else if (auto s = QuantLib::ext::dynamic_pointer_cast<SabrStrippedOptionletAdapter<Cubic>>(capletVol_))
        p = QuantExt::ext::dynamic_pointer_cast<SabrParametricVolatility>(s->parametricVolatility());
    else if (auto s = QuantLib::ext::dynamic_pointer_cast<SabrStrippedOptionletAdapter<CubicFlat>>(capletVol_))
        p = QuantExt::ext::dynamic_pointer_cast<SabrParametricVolatility>(s->parametricVolatility());
    else if (auto s = QuantLib::ext::dynamic_pointer_cast<SabrStrippedOptionletAdapter<BackwardFlat>>(capletVol_))
        p = QuantExt::ext::dynamic_pointer_cast<SabrParametricVolatility>(s->parametricVolatility());

    if (p) {
        DLOG("SABR parameters:");
        DLOG("alpha (pls ignore second row, this is present for technical reasons):");
        DLOGGERSTREAM(p->alpha());
        DLOG("beta (pls ignore second row, this is present for technical reasons):");
        DLOGGERSTREAM(p->beta());
        DLOG("nu (pls ignore second row, this is present for technical reasons):");
        DLOGGERSTREAM(p->nu());
        DLOG("rho (pls ignore second row, this is present for technical reasons):");
        DLOGGERSTREAM(p->rho());
        DLOG("lognormal shift (pls ignore second row, this is present for technical reasons):");
        DLOGGERSTREAM(p->lognormalShift());
        DLOG("calibration attempts:");
        DLOGGERSTREAM(p->numberOfCalibrationAttempts());
        DLOG("calibration error:");
        DLOGGERSTREAM(p->calibrationError());
        DLOG("isInterpolated (1 means calibration failed and point is interpolated):");
        DLOGGERSTREAM(p->isInterpolated());
    }

    DLOG("Building calibration info cap floor vols completed.");
}

} // namespace data
} // namespace ore
