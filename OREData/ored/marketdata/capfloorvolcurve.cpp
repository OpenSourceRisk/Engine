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

#include <ored/marketdata/capfloorvolcurve.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/capfloortermvolsurface.hpp>
#include <qle/termstructures/datedstrippedoptionlet.hpp>
#include <qle/termstructures/datedstrippedoptionletadapter.hpp>
#include <qle/termstructures/optionletstripper1.hpp>
#include <qle/termstructures/optionletstripper2.hpp>
#include <qle/termstructures/optionletstripperwithatm.hpp>
#include <qle/termstructures/piecewiseatmoptionletcurve.hpp>
#include <qle/termstructures/piecewiseoptionletstripper.hpp>
#include <qle/termstructures/strippedoptionletadapter.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolcurve.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletadapter.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

typedef ore::data::CapFloorVolatilityCurveConfig::VolatilityType CfgVolType;
typedef ore::data::CapFloorVolatilityCurveConfig::Type CfgType;
typedef QuantExt::CapFloorTermVolSurface::InterpolationMethod CftvsInterp;

namespace ore {
namespace data {

// Currently, only two possibilities for variable InterpolateOn: TermVolatilities and OptionletVolatilities
// Here, we convert the value to a bool for use in building the structures. May need to broaden if we add more.
bool interpOnOpt(CapFloorVolatilityCurveConfig& config) {
    QL_REQUIRE(config.interpolateOn() == "TermVolatilities" || config.interpolateOn() == "OptionletVolatilities",
               "Expected InterpolateOn to be one of TermVolatilities or OptionletVolatilities");
    return config.interpolateOn() == "OptionletVolatilities";
}

CapFloorVolCurve::CapFloorVolCurve(const Date& asof, const CapFloorVolatilityCurveSpec& spec, const Loader& loader,
                                   const CurveConfigurations& curveConfigs, boost::shared_ptr<IborIndex> iborIndex,
                                   Handle<YieldTermStructure> discountCurve)
    : spec_(spec) {

    try {
        // The configuration
        const boost::shared_ptr<CapFloorVolatilityCurveConfig>& config =
            curveConfigs.capFloorVolCurveConfig(spec_.curveConfigID());

        // Read the shift early if the configured volatility type is shifted lognormal
        Real shift = 0.0;
        if (config->volatilityType() == CfgVolType::ShiftedLognormal) {
            shift = shiftQuote(asof, *config, loader);
        }

        // There are three possible cap floor configurations
        if (config->type() == CfgType::Atm) {
            atmOptCurve(asof, *config, loader, iborIndex, discountCurve, shift);
        } else if (config->type() == CfgType::Surface || config->type() == CfgType::SurfaceWithAtm) {
            optSurface(asof, *config, loader, iborIndex, discountCurve, shift);
        } else {
            QL_FAIL("Unexpected type (" << static_cast<int>(config->type()) << ") for cap floor config "
                                        << config->curveID());
        }

        // Turn on or off extrapolation
        capletVol_->enableExtrapolation(config->extrapolate());

    } catch (exception& e) {
        QL_FAIL("cap/floor vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("cap/floor vol curve building failed: unknown error");
    }

    // force bootstrap so that errors are thrown during the build, not later
    capletVol_->volatility(QL_EPSILON, capletVol_->minStrike());
}

void CapFloorVolCurve::atmOptCurve(const Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader,
                                   boost::shared_ptr<IborIndex> iborIndex, Handle<YieldTermStructure> discountCurve,
                                   Real shift) {

    // Get the ATM cap floor term vol curve
    boost::shared_ptr<QuantExt::CapFloorTermVolCurve> cftvc = atmCurve(asof, config, loader);

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
            boost::shared_ptr<PiecewiseAtmOptionletCurve<Linear>> tmp =
                boost::make_shared<PiecewiseAtmOptionletCurve<Linear>>(
                    config.settleDays(), cftvc, iborIndex, discountCurve, flatFirstPeriod,
                    volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, Linear(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseAtmOptionletCurve<Linear, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Linear>>(
                asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                tmp->calendar(), tmp->businessDayConvention(), iborIndex, tmp->dayCounter(),
                                tmp->volatilityType(), tmp->displacement()));
        } else if (config.timeInterpolation() == "LinearFlat") {
            boost::shared_ptr<PiecewiseAtmOptionletCurve<LinearFlat>> tmp =
                boost::make_shared<PiecewiseAtmOptionletCurve<LinearFlat>>(
                    config.settleDays(), cftvc, iborIndex, discountCurve, flatFirstPeriod,
                    volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, LinearFlat(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseAtmOptionletCurve<LinearFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Linear>>(
                asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                tmp->calendar(), tmp->businessDayConvention(), iborIndex, tmp->dayCounter(),
                                tmp->volatilityType(), tmp->displacement()));
        } else if (config.timeInterpolation() == "BackwardFlat") {
            boost::shared_ptr<PiecewiseAtmOptionletCurve<BackwardFlat>> tmp =
                boost::make_shared<PiecewiseAtmOptionletCurve<BackwardFlat>>(
                    config.settleDays(), cftvc, iborIndex, discountCurve, flatFirstPeriod,
                    volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, BackwardFlat(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseAtmOptionletCurve<BackwardFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, Linear>>(
                asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                tmp->calendar(), tmp->businessDayConvention(), iborIndex, tmp->dayCounter(),
                                tmp->volatilityType(), tmp->displacement()));
        } else if (config.timeInterpolation() == "Cubic") {
            boost::shared_ptr<PiecewiseAtmOptionletCurve<Cubic>> tmp =
                boost::make_shared<PiecewiseAtmOptionletCurve<Cubic>>(
                    config.settleDays(), cftvc, iborIndex, discountCurve, flatFirstPeriod,
                    volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, Cubic(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseAtmOptionletCurve<Cubic, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Linear>>(
                asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                tmp->calendar(), tmp->businessDayConvention(), iborIndex, tmp->dayCounter(),
                                tmp->volatilityType(), tmp->displacement()));
        } else if (config.timeInterpolation() == "CubicFlat") {
            boost::shared_ptr<PiecewiseAtmOptionletCurve<CubicFlat>> tmp =
                boost::make_shared<PiecewiseAtmOptionletCurve<CubicFlat>>(
                    config.settleDays(), cftvc, iborIndex, discountCurve, flatFirstPeriod,
                    volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, CubicFlat(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseAtmOptionletCurve<CubicFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Linear>>(
                asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                tmp->calendar(), tmp->businessDayConvention(), iborIndex, tmp->dayCounter(),
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
                boost::shared_ptr<PiecewiseAtmOptionletCurve<CubicFlat>> tmp =
                    boost::make_shared<PiecewiseAtmOptionletCurve<CubicFlat>>(
                        config.settleDays(), cftvc, iborIndex, discountCurve, flatFirstPeriod,
                        volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, CubicFlat(),
                        QuantExt::IterativeBootstrap<
                            PiecewiseAtmOptionletCurve<CubicFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                            accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Linear>>(
                    asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                    tmp->calendar(), tmp->businessDayConvention(), iborIndex, tmp->dayCounter(),
                                    tmp->volatilityType(), tmp->displacement()));
            } else {
                boost::shared_ptr<PiecewiseAtmOptionletCurve<Cubic>> tmp =
                    boost::make_shared<PiecewiseAtmOptionletCurve<Cubic>>(
                        config.settleDays(), cftvc, iborIndex, discountCurve, flatFirstPeriod,
                        volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, Cubic(),
                        QuantExt::IterativeBootstrap<
                            PiecewiseAtmOptionletCurve<Cubic, QuantExt::IterativeBootstrap>::optionlet_curve>(
                            accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Linear>>(
                    asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                    tmp->calendar(), tmp->businessDayConvention(), iborIndex, tmp->dayCounter(),
                                    tmp->volatilityType(), tmp->displacement()));
            }
        } else if (config.interpolationMethod() == CftvsInterp::Bilinear) {
            if (config.flatExtrapolation()) {
                boost::shared_ptr<PiecewiseAtmOptionletCurve<LinearFlat>> tmp =
                    boost::make_shared<PiecewiseAtmOptionletCurve<LinearFlat>>(
                        config.settleDays(), cftvc, iborIndex, discountCurve, flatFirstPeriod,
                        volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt,
                        LinearFlat(),
                        QuantExt::IterativeBootstrap<
                            PiecewiseAtmOptionletCurve<LinearFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                            accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Linear>>(
                    asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                    tmp->calendar(), tmp->businessDayConvention(), iborIndex, tmp->dayCounter(),
                                    tmp->volatilityType(), tmp->displacement()));
            } else {
                boost::shared_ptr<PiecewiseAtmOptionletCurve<Linear>> tmp =
                    boost::make_shared<PiecewiseAtmOptionletCurve<Linear>>(
                        config.settleDays(), cftvc, iborIndex, discountCurve, flatFirstPeriod,
                        volatilityType(config.volatilityType()), shift, optVolType, optDisplacement, onOpt, Linear(),
                        QuantExt::IterativeBootstrap<
                            PiecewiseAtmOptionletCurve<Linear, QuantExt::IterativeBootstrap>::optionlet_curve>(
                            accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Linear>>(
                    asof, transform(asof, tmp->curve()->dates(), tmp->curve()->volatilities(), tmp->settlementDays(),
                                    tmp->calendar(), tmp->businessDayConvention(), iborIndex, tmp->dayCounter(),
                                    tmp->volatilityType(), tmp->displacement()));
            }
        } else {
            QL_FAIL("Cap floor config " << config.curveID() << " has unexpected interpolation method "
                                        << static_cast<int>(config.interpolationMethod()));
        }
    }
}

void CapFloorVolCurve::optSurface(const Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader,
                                  boost::shared_ptr<IborIndex> iborIndex, Handle<YieldTermStructure> discountCurve,
                                  Real shift) {

    // Get the cap floor term vol surface
    boost::shared_ptr<QuantExt::CapFloorTermVolSurface> cftvs = capSurface(asof, config, loader);

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

    // On optionlets is the newly added interpolation approach whereas on term volatilities is legacy
    boost::shared_ptr<QuantExt::OptionletStripper> optionletStripper;
    VolatilityType volType = volatilityType(config.volatilityType());
    bool onOpt = interpOnOpt(config);
    if (onOpt) {
        // This is not pretty but can't think of a better way (with template functions and or classes)
        if (config.timeInterpolation() == "Linear") {
            optionletStripper = boost::make_shared<PiecewiseOptionletStripper<Linear>>(
                cftvs, iborIndex, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                Linear(),
                QuantExt::IterativeBootstrap<
                    PiecewiseOptionletStripper<Linear, QuantExt::IterativeBootstrap>::optionlet_curve>(
                    accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            if (config.strikeInterpolation() == "Linear") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<Linear, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Linear>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "LinearFlat") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<Linear, LinearFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Linear, LinearFlat>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "Cubic") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<Linear, Cubic>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Cubic>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "CubicFlat") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<Linear, CubicFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Linear, CubicFlat>>(
                    asof, transform(*optionletStripper));
            } else {
                QL_FAIL("Cap floor config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
            }
        } else if (config.timeInterpolation() == "LinearFlat") {
            optionletStripper = boost::make_shared<PiecewiseOptionletStripper<LinearFlat>>(
                cftvs, iborIndex, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                LinearFlat(),
                QuantExt::IterativeBootstrap<
                    PiecewiseOptionletStripper<LinearFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                    accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            if (config.strikeInterpolation() == "Linear") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<LinearFlat, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Linear>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "LinearFlat") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<LinearFlat, LinearFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, LinearFlat>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "Cubic") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<LinearFlat, Cubic>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Cubic>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "CubicFlat") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<LinearFlat, CubicFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, CubicFlat>>(
                    asof, transform(*optionletStripper));
            } else {
                QL_FAIL("Cap floor config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
            }
        } else if (config.timeInterpolation() == "BackwardFlat") {
            optionletStripper = boost::make_shared<PiecewiseOptionletStripper<BackwardFlat>>(
                cftvs, iborIndex, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                BackwardFlat(),
                QuantExt::IterativeBootstrap<
                    PiecewiseOptionletStripper<BackwardFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                    accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            if (config.strikeInterpolation() == "Linear") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<BackwardFlat, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, Linear>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "LinearFlat") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<BackwardFlat, LinearFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, LinearFlat>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "Cubic") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<BackwardFlat, Cubic>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, Cubic>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "CubicFlat") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<BackwardFlat, CubicFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, CubicFlat>>(
                    asof, transform(*optionletStripper));
            } else {
                QL_FAIL("Cap floor config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
            }
        } else if (config.timeInterpolation() == "Cubic") {
            optionletStripper = boost::make_shared<PiecewiseOptionletStripper<Cubic>>(
                cftvs, iborIndex, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                Cubic(),
                QuantExt::IterativeBootstrap<
                    PiecewiseOptionletStripper<Cubic, QuantExt::IterativeBootstrap>::optionlet_curve>(
                    accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            if (config.strikeInterpolation() == "Linear") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<Cubic, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Linear>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "LinearFlat") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<Cubic, LinearFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, LinearFlat>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "Cubic") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<Cubic, Cubic>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Cubic>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "CubicFlat") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<Cubic, CubicFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, CubicFlat>>(
                    asof, transform(*optionletStripper));
            } else {
                QL_FAIL("Cap floor config " << config.curveID() << " has unexpected strike interpolation "
                                            << config.strikeInterpolation());
            }
        } else if (config.timeInterpolation() == "CubicFlat") {
            optionletStripper = boost::make_shared<PiecewiseOptionletStripper<CubicFlat>>(
                cftvs, iborIndex, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement, onOpt,
                CubicFlat(),
                QuantExt::IterativeBootstrap<
                    PiecewiseOptionletStripper<CubicFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                    accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
            if (config.strikeInterpolation() == "Linear") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<CubicFlat, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Linear>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "LinearFlat") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<CubicFlat, LinearFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, LinearFlat>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "Cubic") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<CubicFlat, Cubic>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Cubic>>(
                    asof, transform(*optionletStripper));
            } else if (config.strikeInterpolation() == "CubicFlat") {
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<CubicFlat, CubicFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, CubicFlat>>(
                    asof, transform(*optionletStripper));
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
                optionletStripper = boost::make_shared<PiecewiseOptionletStripper<CubicFlat>>(
                    cftvs, iborIndex, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement,
                    onOpt, CubicFlat(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseOptionletStripper<CubicFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<CubicFlat, CubicFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, CubicFlat>>(
                    asof, transform(*optionletStripper));
            } else {
                optionletStripper = boost::make_shared<PiecewiseOptionletStripper<Cubic>>(
                    cftvs, iborIndex, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement,
                    onOpt, Cubic(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseOptionletStripper<Cubic, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<Cubic, Cubic>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Cubic>>(
                    asof, transform(*optionletStripper));
            }
        } else if (config.interpolationMethod() == CftvsInterp::Bilinear) {
            if (config.flatExtrapolation()) {
                optionletStripper = boost::make_shared<PiecewiseOptionletStripper<LinearFlat>>(
                    cftvs, iborIndex, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement,
                    onOpt, LinearFlat(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseOptionletStripper<LinearFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<LinearFlat, LinearFlat>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, LinearFlat>>(
                    asof, transform(*optionletStripper));
            } else {
                optionletStripper = boost::make_shared<PiecewiseOptionletStripper<Linear>>(
                    cftvs, iborIndex, discountCurve, flatFirstPeriod, volType, shift, optVolType, optDisplacement,
                    onOpt, Linear(),
                    QuantExt::IterativeBootstrap<
                        PiecewiseOptionletStripper<Linear, QuantExt::IterativeBootstrap>::optionlet_curve>(
                        accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor, dontThrowSteps));
                if (includeAtm) {
                    optionletStripper = boost::make_shared<OptionletStripperWithAtm<Linear, Linear>>(
                        optionletStripper, cftvc, discountCurve, volType, shift);
                }
                capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Linear>>(
                    asof, transform(*optionletStripper));
            }
        } else {
            QL_FAIL("Cap floor config " << config.curveID() << " has unexpected interpolation method "
                                        << static_cast<int>(config.interpolationMethod()));
        }
    }
}

boost::shared_ptr<QuantExt::CapFloorTermVolSurface>
CapFloorVolCurve::capSurface(const Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader) const {

    // Map to store the quote values that we load with key (period, strike) where strike
    // needs a custom comparator to avoid == with double
    auto comp = [](const pair<Period, Rate>& a, const pair<Period, Rate>& b) {
        return (a.first < b.first) || (!(b.first < a.first) && (a.second < b.second && !close(a.second, b.second)));
    };
    map<pair<Period, Rate>, Real, decltype(comp)> volQuotes(comp);

    // Load the relevant quotes
    for (const string& quoteId : config.quotes()) {

        boost::shared_ptr<MarketDatum> md = loader.get(quoteId, asof);

        // If it is a non-ATM cap floor quote, store it and fail if there are duplicates
        if (boost::shared_ptr<CapFloorQuote> cfq = boost::dynamic_pointer_cast<CapFloorQuote>(md)) {
            if (!cfq->atm()) {
                auto key = make_pair(cfq->term(), cfq->strike());
                auto r = volQuotes.insert(make_pair(key, cfq->quote()->value()));
                QL_REQUIRE(r.second, "Duplicate cap floor quote in config " << config.curveID() << " for tenor "
                                                                            << key.first << " and strike "
                                                                            << key.second);
            }
        }
    }

    // Organise the values in to a square matrix
    vector<Period> tenors = parseVectorOfValues<Period>(config.tenors(), &parsePeriod);
    vector<Rate> strikes = parseVectorOfValues<Real>(config.strikes(), &parseReal);
    Matrix vols(tenors.size(), strikes.size());
    for (Size i = 0; i < tenors.size(); i++) {
        for (Size j = 0; j < strikes.size(); j++) {
            auto key = make_pair(tenors[i], strikes[j]);
            auto it = volQuotes.find(key);
            QL_REQUIRE(it != volQuotes.end(), "Quote with tenor " << key.first << " and strike " << key.second
                                                                  << " not loaded for cap floor config "
                                                                  << config.curveID());
            vols[i][j] = it->second;
        }
    }

    // Return for the cap floor term volatility surface
    return boost::make_shared<QuantExt::CapFloorTermVolSurface>(config.settleDays(), config.calendar(),
                                                                config.businessDayConvention(), tenors, strikes, vols,
                                                                config.dayCounter(), config.interpolationMethod());
}

boost::shared_ptr<QuantExt::CapFloorTermVolCurve>
CapFloorVolCurve::atmCurve(const Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader) const {

    // Map to store the quote values
    map<Period, Handle<Quote>> volQuotes;

    // Load the relevant quotes
    for (const string& quoteId : config.quotes()) {

        boost::shared_ptr<MarketDatum> md = loader.get(quoteId, asof);

        // If it is an ATM cap floor quote, store it and fail if there are duplicates
        if (boost::shared_ptr<CapFloorQuote> cfq = boost::dynamic_pointer_cast<CapFloorQuote>(md)) {
            if (cfq->atm()) {
                auto r = volQuotes.insert(make_pair(cfq->term(), cfq->quote()));
                QL_REQUIRE(r.second, "Duplicate ATM cap floor quote in config " << config.curveID() << " for tenor "
                                                                                << cfq->term());
            }
        }
    }

    // Check that the loaded quotes cover all of the configured ATM tenors
    vector<Period> tenors = parseVectorOfValues<Period>(config.atmTenors(), &parsePeriod);
    vector<Handle<Quote>> vols(tenors.size());
    for (Size i = 0; i < tenors.size(); i++) {
        auto it = volQuotes.find(tenors[i]);
        QL_REQUIRE(it != volQuotes.end(),
                   "ATM cap floor quote in config " << config.curveID() << " for tenor " << tenors[i] << " not found ");
        vols[i] = it->second;
    }

    // Return for the cap floor ATM term volatility curve
    // The interpolation here is also based on the interpolation method parameter in the configuration
    // Flat first period is true by default (see ctor)
    if (config.interpolationMethod() == CftvsInterp::BicubicSpline) {
        if (config.flatExtrapolation()) {
            return boost::make_shared<InterpolatedCapFloorTermVolCurve<CubicFlat>>(
                config.settleDays(), config.calendar(), config.businessDayConvention(), tenors, vols,
                config.dayCounter());
        } else {
            return boost::make_shared<InterpolatedCapFloorTermVolCurve<Cubic>>(config.settleDays(), config.calendar(),
                                                                               config.businessDayConvention(), tenors,
                                                                               vols, config.dayCounter());
        }
    } else if (config.interpolationMethod() == CftvsInterp::Bilinear) {
        if (config.flatExtrapolation()) {
            return boost::make_shared<InterpolatedCapFloorTermVolCurve<LinearFlat>>(
                config.settleDays(), config.calendar(), config.businessDayConvention(), tenors, vols,
                config.dayCounter());
        } else {
            return boost::make_shared<InterpolatedCapFloorTermVolCurve<Linear>>(config.settleDays(), config.calendar(),
                                                                                config.businessDayConvention(), tenors,
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

        boost::shared_ptr<MarketDatum> md = loader.get(quoteId, asof);

        // If it is a shift quote
        if (boost::shared_ptr<CapFloorShiftQuote> sq = boost::dynamic_pointer_cast<CapFloorShiftQuote>(md)) {
            return sq->quote()->value();
        }
    }

    QL_FAIL("Could not find a shift quote for cap floor config " << config.curveID());
}

// This method is used to pull out the stripped optionlets from the QuantExt::OptionletStripper instance that is
// bootstrapped. The reason is that we do not want all of the cap floor helpers and their coupons in scope during
// a potential XVA run as this leads to delays when fixings are updated.
boost::shared_ptr<StrippedOptionlet> CapFloorVolCurve::transform(const QuantExt::OptionletStripper& os) const {

    vector<vector<Handle<Quote>>> vols(os.optionletFixingDates().size());
    for (Size i = 0; i < os.optionletFixingDates().size(); i++) {
        for (Size j = 0; j < os.optionletVolatilities(i).size(); j++) {
            vols[i].push_back(Handle<Quote>(boost::make_shared<SimpleQuote>(os.optionletVolatilities(i)[j])));
        }
    }

    boost::shared_ptr<StrippedOptionlet> res = boost::make_shared<StrippedOptionlet>(
        os.settlementDays(), os.calendar(), os.businessDayConvention(), os.iborIndex(), os.optionletFixingDates(),
        os.optionletStrikes(0), vols, os.dayCounter(), os.volatilityType(), os.displacement());

    res->unregisterWithAll();

    return res;
}

boost::shared_ptr<StrippedOptionlet>
CapFloorVolCurve::transform(const Date& asof, vector<Date> dates, const vector<Volatility>& volatilities,
                            Natural settleDays, const Calendar& cal, BusinessDayConvention bdc,
                            boost::shared_ptr<IborIndex> iborIndex, const DayCounter& dc, VolatilityType type,
                            Real displacement) const {

    vector<vector<Handle<Quote>>> vols(dates.size());
    for (Size i = 0; i < dates.size(); i++) {
        vols[i].push_back(Handle<Quote>(boost::make_shared<SimpleQuote>(volatilities[i])));
    }

    // Temp fix because QuantLib::StrippedOptionlet requires optionlet dates strictly greater than
    // the evaluation date. Would rather relax this to '>=' in QuantLib::StrippedOptionlet
    if (dates[0] == asof) {
        dates[0]++;
    }

    vector<Rate> strikes = {0.0};
    boost::shared_ptr<StrippedOptionlet> res = boost::make_shared<StrippedOptionlet>(
        settleDays, cal, bdc, iborIndex, dates, strikes, vols, dc, type, displacement);

    res->unregisterWithAll();

    return res;
}

} // namespace data
} // namespace ore
