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

#include <ql/currencies/exchangeratemanager.hpp>
#include <ql/math/functional.hpp>
#include <ql/math/randomnumbers/haltonrsg.hpp>
#include <ql/pricingengines/bond/bondfunctions.hpp>
#include <ql/pricingengines/bond/discountingbondengine.hpp>
#include <ql/quotes/derivedquote.hpp>
#include <ql/termstructures/yield/bondhelpers.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/nonlinearfittingmethods.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/piecewisezerospreadedtermstructure.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/termstructures/yield/overnightindexfutureratehelper.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/imm.hpp>

#include <ql/indexes/ibor/all.hpp>
#include <ql/math/interpolations/convexmonotoneinterpolation.hpp>
#include <ql/math/interpolations/mixedinterpolation.hpp>
#include <qle/indexes/ibor/brlcdi.hpp>
#include <qle/math/logquadraticinterpolation.hpp>
#include <qle/math/quadraticinterpolation.hpp>
#include <qle/termstructures/averageoisratehelper.hpp>
#include <qle/termstructures/basistwoswaphelper.hpp>
#include <qle/termstructures/brlcdiratehelper.hpp>
#include <qle/termstructures/crossccybasismtmresetswaphelper.hpp>
#include <qle/termstructures/crossccybasisswaphelper.hpp>
#include <qle/termstructures/crossccyfixfloatmtmresetswaphelper.hpp>
#include <qle/termstructures/crossccyfixfloatswaphelper.hpp>
#include <qle/termstructures/discountratiomodifiedcurve.hpp>
#include <qle/termstructures/immfraratehelper.hpp>
#include <qle/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/oisratehelper.hpp>
#include <qle/termstructures/subperiodsswaphelper.hpp>
#include <qle/termstructures/tenorbasisswaphelper.hpp>
#include <qle/termstructures/weightedyieldtermstructure.hpp>
#include <qle/termstructures/yieldplusdefaultyieldtermstructure.hpp>
#include <qle/termstructures/iborfallbackcurve.hpp>
#include <qle/termstructures/overnightfallbackcurve.hpp>
#include <qle/termstructures/bondyieldshiftedcurvetermstructure.hpp>

#include <ored/marketdata/defaultcurve.hpp>
#include <ored/marketdata/fittedbondcurvehelpermarket.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace {
/* Helper function to return the key required to look up the map in the YieldCurve ctor */
string yieldCurveKey(const Currency& curveCcy, const string& curveID, const Date&) {
    ore::data::YieldCurveSpec tempSpec(curveCcy.code(), curveID);
    return tempSpec.name();
}
} // namespace

namespace ore {
namespace data {

template <template <class> class CurveType>
QuantLib::ext::shared_ptr<YieldTermStructure> buildYieldCurve(const vector<Date>& dates, const vector<QuantLib::Real>& rates,
                                                      const DayCounter& dayCounter,
                                                      YieldCurve::InterpolationMethod interpolationMethod, Size n) {

    QuantLib::ext::shared_ptr<YieldTermStructure> yieldts;
    switch (interpolationMethod) {
    case YieldCurve::InterpolationMethod::Linear:
        yieldts.reset(new CurveType<QuantLib::Linear>(dates, rates, dayCounter, QuantLib::Linear()));
        break;
    case YieldCurve::InterpolationMethod::LogLinear:
        yieldts.reset(new CurveType<QuantLib::LogLinear>(dates, rates, dayCounter, QuantLib::LogLinear()));
        break;
    case YieldCurve::InterpolationMethod::NaturalCubic:
        yieldts.reset(new CurveType<QuantLib::Cubic>(dates, rates, dayCounter,
                                                     QuantLib::Cubic(CubicInterpolation::Kruger, true)));
        break;
    case YieldCurve::InterpolationMethod::FinancialCubic:
        yieldts.reset(new CurveType<QuantLib::Cubic>(dates, rates, dayCounter,
                                                     QuantLib::Cubic(CubicInterpolation::Kruger, true,
                                                                     CubicInterpolation::SecondDerivative, 0.0,
                                                                     CubicInterpolation::FirstDerivative)));
        break;
    case YieldCurve::InterpolationMethod::ConvexMonotone:
        yieldts.reset(
            new CurveType<QuantLib::ConvexMonotone>(dates, rates, dayCounter, Calendar(), {}, {}, QuantLib::ConvexMonotone()));
        break;
    case YieldCurve::InterpolationMethod::Quadratic:
         yieldts.reset(new CurveType<QuantExt::Quadratic>(dates, rates, dayCounter, QuantExt::Quadratic(1, 0, 1, 0, 1)));
         break;
     case YieldCurve::InterpolationMethod::LogQuadratic:
         yieldts.reset(
             new CurveType<QuantExt::LogQuadratic>(dates, rates, dayCounter, QuantExt::LogQuadratic(1, 0, -1, 0, 1)));
         break;
     case YieldCurve::InterpolationMethod::Hermite:
         yieldts.reset(new CurveType<QuantLib::Cubic>(dates, rates, dayCounter, Cubic(CubicInterpolation::Parabolic)));
         break;
     case YieldCurve::InterpolationMethod::CubicSpline:
         yieldts.reset(new CurveType<QuantLib::Cubic>(dates, rates, dayCounter,
                                                      Cubic(CubicInterpolation::Spline, false,
                                                            CubicInterpolation::SecondDerivative, 0.0,
                                                            CubicInterpolation::SecondDerivative, 0.0)));
         break;
     case YieldCurve::InterpolationMethod::DefaultLogMixedLinearCubic:
         yieldts.reset(
             new CurveType<DefaultLogMixedLinearCubic>(dates, rates, dayCounter, DefaultLogMixedLinearCubic(n)));
         break;
     case YieldCurve::InterpolationMethod::MonotonicLogMixedLinearCubic:
         yieldts.reset(
             new CurveType<MonotonicLogMixedLinearCubic>(dates, rates, dayCounter, MonotonicLogMixedLinearCubic(n)));
         break;
     case YieldCurve::InterpolationMethod::KrugerLogMixedLinearCubic:
         yieldts.reset(
             new CurveType<KrugerLogMixedLinearCubic>(dates, rates, dayCounter, KrugerLogMixedLinearCubic(n)));
         break;
     case YieldCurve::InterpolationMethod::LogMixedLinearCubicNaturalSpline:
         yieldts.reset(new CurveType<LogMixedLinearCubic>(
             dates, rates, dayCounter,
             LogMixedLinearCubic(n, MixedInterpolation::ShareRanges, CubicInterpolation::Spline, false,
                                 CubicInterpolation::SecondDerivative, 0.0, CubicInterpolation::SecondDerivative,
                                 0.0)));
         break;

     default:
         QL_FAIL("Interpolation method '" << interpolationMethod << "' not recognised.");
    }
    return yieldts;
}

QuantLib::ext::shared_ptr<YieldTermStructure> zerocurve(const vector<Date>& dates, const vector<Rate>& yields,
                                                const DayCounter& dayCounter,
                                                YieldCurve::InterpolationMethod interpolationMethod, Size n) {
    return buildYieldCurve<InterpolatedZeroCurve>(dates, yields, dayCounter, interpolationMethod, n);
}

QuantLib::ext::shared_ptr<YieldTermStructure> discountcurve(const vector<Date>& dates, const vector<DiscountFactor>& dfs,
                                                    const DayCounter& dayCounter,
                                                    YieldCurve::InterpolationMethod interpolationMethod, Size n) {
    return buildYieldCurve<InterpolatedDiscountCurve>(dates, dfs, dayCounter, interpolationMethod, n);
}

QuantLib::ext::shared_ptr<YieldTermStructure> forwardcurve(const vector<Date>& dates, const vector<Rate>& forwards,
                                                   const DayCounter& dayCounter,
                                                   YieldCurve::InterpolationMethod interpolationMethod, Size n) {
    return buildYieldCurve<InterpolatedForwardCurve>(dates, forwards, dayCounter, interpolationMethod, n);
}

/* Helper functions
 */
YieldCurve::InterpolationMethod parseYieldCurveInterpolationMethod(const string& s) {
    if (s == "Linear")
        return YieldCurve::InterpolationMethod::Linear;
    else if (s == "LogLinear")
        return YieldCurve::InterpolationMethod::LogLinear;
    else if (s == "NaturalCubic")
        return YieldCurve::InterpolationMethod::NaturalCubic;
    else if (s == "FinancialCubic")
        return YieldCurve::InterpolationMethod::FinancialCubic;
    else if (s == "ConvexMonotone")
        return YieldCurve::InterpolationMethod::ConvexMonotone;
    else if (s == "ExponentialSplines")
        return YieldCurve::InterpolationMethod::ExponentialSplines;
    else if (s == "Quadratic")
        return YieldCurve::InterpolationMethod::Quadratic;
    else if (s == "LogQuadratic")
        return YieldCurve::InterpolationMethod::LogQuadratic;
    else if (s == "LogNaturalCubic")
        return YieldCurve::InterpolationMethod::LogNaturalCubic;
    else if (s == "LogFinancialCubic")
        return YieldCurve::InterpolationMethod::LogFinancialCubic;
    else if (s == "LogCubicSpline")
        return YieldCurve::InterpolationMethod::LogCubicSpline;
    else if (s == "Hermite")
        return YieldCurve::InterpolationMethod::Hermite;
    else if (s == "CubicSpline")
        return YieldCurve::InterpolationMethod::CubicSpline;
    else if (s == "DefaultLogMixedLinearCubic")
        return YieldCurve::InterpolationMethod::DefaultLogMixedLinearCubic;
    else if (s == "MonotonicLogMixedLinearCubic")
        return YieldCurve::InterpolationMethod::MonotonicLogMixedLinearCubic;
    else if (s == "KrugerLogMixedLinearCubic")
        return YieldCurve::InterpolationMethod::KrugerLogMixedLinearCubic;
    else if (s == "LogMixedLinearCubicNaturalSpline")
        return YieldCurve::InterpolationMethod::LogMixedLinearCubicNaturalSpline;
    else if (s == "NelsonSiegel")
        return YieldCurve::InterpolationMethod::NelsonSiegel;
    else if (s == "Svensson")
        return YieldCurve::InterpolationMethod::Svensson;
    else
        QL_FAIL("Yield curve interpolation method " << s << " not recognized");
};

YieldCurve::InterpolationVariable parseYieldCurveInterpolationVariable(const string& s) {
    if (s == "Zero")
        return YieldCurve::InterpolationVariable::Zero;
    else if (s == "Discount")
        return YieldCurve::InterpolationVariable::Discount;
    else if (s == "Forward")
        return YieldCurve::InterpolationVariable::Forward;
    else
        QL_FAIL("Yield curve interpolation variable " << s << " not recognized");
};

std::ostream& operator<<(std::ostream& out, const YieldCurve::InterpolationMethod m) {
    if (m == YieldCurve::InterpolationMethod::Linear)
        return out << "Linear";
    else if (m == YieldCurve::InterpolationMethod::LogLinear)
        return out << "LogLinear";
    else if (m == YieldCurve::InterpolationMethod::NaturalCubic)
        return out << "NaturalCubic";
    else if (m == YieldCurve::InterpolationMethod::FinancialCubic)
        return out << "FinancialCubic";
    else if (m == YieldCurve::InterpolationMethod::ConvexMonotone)
        return out << "ConvexMonotone";
    else if (m == YieldCurve::InterpolationMethod::ExponentialSplines)
        return out << "ExponentialSplines";
    else if (m == YieldCurve::InterpolationMethod::Quadratic)
        return out << "Quadratic";
    else if (m == YieldCurve::InterpolationMethod::LogQuadratic)
        return out << "LogQuadratic";
    else if (m == YieldCurve::InterpolationMethod::LogNaturalCubic)
        return out << "LogNaturalCubic";
    else if (m == YieldCurve::InterpolationMethod::LogFinancialCubic)
        return out << "LogFinancialCubic";
    else if (m == YieldCurve::InterpolationMethod::LogCubicSpline)
        return out << "LogCubicSpline";
    else if (m == YieldCurve::InterpolationMethod::Hermite)
        return out << "Hermite";
    else if (m == YieldCurve::InterpolationMethod::CubicSpline)
        return out << "CubicSpline";
    else if (m == YieldCurve::InterpolationMethod::DefaultLogMixedLinearCubic)
        return out << "DefaultLogMixedLinearCubic";
    else if (m == YieldCurve::InterpolationMethod::MonotonicLogMixedLinearCubic)
        return out << "MonotonicLogMixedLinearCubic";
    else if (m == YieldCurve::InterpolationMethod::KrugerLogMixedLinearCubic)
        return out << "KrugerLogMixedLinearCubic";
    else if (m == YieldCurve::InterpolationMethod::LogMixedLinearCubicNaturalSpline)
        return out << "LogMixedLinearCubicNaturalSpline";
    else if (m == YieldCurve::InterpolationMethod::NelsonSiegel)
        return out << "NelsonSiegel";
    else if (m == YieldCurve::InterpolationMethod::Svensson)
        return out << "Svensson";
    else
        QL_FAIL("Yield curve interpolation method " << static_cast<int>(m) << " not recognized");
}

YieldCurve::YieldCurve(Date asof, YieldCurveSpec curveSpec, const CurveConfigurations& curveConfigs,
                       const Loader& loader, const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& requiredYieldCurves,
                       const map<string, QuantLib::ext::shared_ptr<DefaultCurve>>& requiredDefaultCurves,
                       const FXTriangulation& fxTriangulation,
                       const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
                       const IborFallbackConfig& iborFallbackConfig, const bool preserveQuoteLinkage,
                       const bool buildCalibrationInfo, const Market* market)
    : asofDate_(asof), curveSpec_(curveSpec), loader_(loader), requiredYieldCurves_(requiredYieldCurves),
      requiredDefaultCurves_(requiredDefaultCurves), fxTriangulation_(fxTriangulation), referenceData_(referenceData),
      iborFallbackConfig_(iborFallbackConfig), preserveQuoteLinkage_(preserveQuoteLinkage),
      buildCalibrationInfo_(buildCalibrationInfo), market_(market) {

    try {

        curveConfig_ = curveConfigs.yieldCurveConfig(curveSpec_.curveConfigID());
        QL_REQUIRE(curveConfig_, "No yield curve configuration found "
                                 "for config ID "
                                     << curveSpec_.curveConfigID());
        currency_ = parseCurrency(curveConfig_->currency());

        /* If discount curve is not the curve being built, look for it in the map that is passed in. */
        string discountCurveID = curveConfig_->discountCurveID();
        if (discountCurveID != curveConfig_->curveID() && !discountCurveID.empty()) {
            discountCurveID = yieldCurveKey(currency_, discountCurveID, asofDate_);
            map<string, QuantLib::ext::shared_ptr<YieldCurve>>::iterator it;
            it = requiredYieldCurves_.find(discountCurveID);
            if (it != requiredYieldCurves_.end()) {
                discountCurve_ = it->second;
            } else {
                QL_FAIL("The discount curve, " << discountCurveID
                                               << ", required in the building "
                                                  "of the curve, "
                                               << curveSpec_.name() << ", was not found.");
            }
        }

        curveSegments_ = curveConfig_->curveSegments();
        interpolationMethod_ = parseYieldCurveInterpolationMethod(curveConfig_->interpolationMethod());
        interpolationVariable_ = parseYieldCurveInterpolationVariable(curveConfig_->interpolationVariable());
        zeroDayCounter_ = parseDayCounter(curveConfig_->zeroDayCounter());
        extrapolation_ = curveConfig_->extrapolation();

        if (curveSegments_[0]->type() == YieldCurveSegment::Type::Discount) {
            DLOG("Building DiscountCurve " << curveSpec_);
            buildDiscountCurve();
        } else if (curveSegments_[0]->type() == YieldCurveSegment::Type::Zero) {
            DLOG("Building ZeroCurve " << curveSpec_);
            buildZeroCurve();
        } else if (curveSegments_[0]->type() == YieldCurveSegment::Type::ZeroSpread) {
            DLOG("Building ZeroSpreadedCurve " << curveSpec_);
            buildZeroSpreadedCurve();
        } else if (curveSegments_[0]->type() == YieldCurveSegment::Type::DiscountRatio) {
            DLOG("Building discount ratio yield curve " << curveSpec_);
            buildDiscountRatioCurve();
        } else if (curveSegments_[0]->type() == YieldCurveSegment::Type::FittedBond) {
            DLOG("Building FittedBondCurve " << curveSpec_);
            buildFittedBondCurve();
        } else if (curveSegments_[0]->type() == YieldCurveSegment::Type::WeightedAverage) {
            DLOG("Building WeightedAverageCurve " << curveSpec_);
            buildWeightedAverageCurve();
        } else if (curveSegments_[0]->type() == YieldCurveSegment::Type::YieldPlusDefault) {
            DLOG("Building YieldPlusDefaultCurve " << curveSpec_);
            buildYieldPlusDefaultCurve();
        } else if (curveSegments_[0]->type() == YieldCurveSegment::Type::IborFallback) {
            DLOG("Building IborFallbackCurve " << curveSpec_);
            buildIborFallbackCurve();
        } else if (curveSegments_[0]->type() == YieldCurveSegment::Type::BondYieldShifted) {
            DLOG("Building BondYieldShiftedCurve " << curveSpec_);
            buildBondYieldShiftedCurve();
        } else {
            DLOG("Bootstrapping YieldCurve " << curveSpec_);
            buildBootstrappedCurve();
        }

        h_.linkTo(p_);
        if (extrapolation_) {
            h_->enableExtrapolation();
        }

        // populate shared calibration info

        if (buildCalibrationInfo_) {
            if (calibrationInfo_ == nullptr)
                calibrationInfo_ = QuantLib::ext::make_shared<YieldCurveCalibrationInfo>();
            calibrationInfo_->dayCounter = zeroDayCounter_.name();
            calibrationInfo_->currency = currency_.code();
            if (calibrationInfo_->pillarDates.empty()) {
                for (auto const& p : YieldCurveCalibrationInfo::defaultPeriods)
                    calibrationInfo_->pillarDates.push_back(asofDate_ + p);
            }
            for (auto const& d : calibrationInfo_->pillarDates) {
                calibrationInfo_->zeroRates.push_back(p_->zeroRate(d, zeroDayCounter_, Continuous));
                calibrationInfo_->discountFactors.push_back(p_->discount(d));
                calibrationInfo_->times.push_back(p_->timeFromReference(d));
            }
        }

    } catch (QuantLib::Error& e) {
        QL_FAIL("yield curve building failed for curve " << curveSpec_.curveConfigID() << " on date "
                                                         << io::iso_date(asof) << ": " << e.what());
    } catch (std::exception& e) {
        QL_FAIL(e.what());
    } catch (...) {
        QL_FAIL("unknown error");
    }

    // force bootstrap so that errors are thrown during the build, not later
    h_->discount(QL_EPSILON);

    DLOG("Yield curve " << curveSpec_.name() << " built");
}

QuantLib::ext::shared_ptr<YieldTermStructure>
YieldCurve::piecewisecurve(vector<QuantLib::ext::shared_ptr<RateHelper>> instruments) {

    // Ensure that the instruments are sorted. This is done in IterativeBootstrap, but we need
    // a sorted instruments vector in the code here as well.
    std::sort(instruments.begin(), instruments.end(), QuantLib::detail::BootstrapHelperSorter());

    // Get configuration values for bootstrap
    Real accuracy = curveConfig_->bootstrapConfig().accuracy();
    Real globalAccuracy = curveConfig_->bootstrapConfig().globalAccuracy();
    bool dontThrow = curveConfig_->bootstrapConfig().dontThrow();
    Size maxAttempts = curveConfig_->bootstrapConfig().maxAttempts();
    Real maxFactor = curveConfig_->bootstrapConfig().maxFactor();
    Real minFactor = curveConfig_->bootstrapConfig().minFactor();
    Size dontThrowSteps = curveConfig_->bootstrapConfig().dontThrowSteps();

    QuantLib::ext::shared_ptr<YieldTermStructure> yieldts;
    switch (interpolationVariable_) {
    case InterpolationVariable::Zero:
        switch (interpolationMethod_) {
        case InterpolationMethod::Linear: {
            typedef PiecewiseYieldCurve<ZeroYield, Linear, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, Linear(),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::LogLinear: {
            typedef PiecewiseYieldCurve<ZeroYield, LogLinear, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, LogLinear(),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::NaturalCubic: {
            typedef PiecewiseYieldCurve<ZeroYield, Cubic, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, Cubic(CubicInterpolation::Kruger, true),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::FinancialCubic: {
            typedef PiecewiseYieldCurve<ZeroYield, Cubic, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_,
                Cubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                      CubicInterpolation::FirstDerivative),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::ConvexMonotone: {
            typedef PiecewiseYieldCurve<ZeroYield, ConvexMonotone, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, ConvexMonotone(),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::Hermite: {
             typedef PiecewiseYieldCurve<ZeroYield, Cubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, Cubic(CubicInterpolation::Parabolic),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::CubicSpline: {
             typedef PiecewiseYieldCurve<ZeroYield, Cubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_,
                 Cubic(CubicInterpolation::Spline, false, CubicInterpolation::SecondDerivative, 0.0,
                       CubicInterpolation::SecondDerivative, 0.0),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor,
                                          dontThrowSteps));
         } break;
         case InterpolationMethod::Quadratic: {
             typedef PiecewiseYieldCurve<ZeroYield, QuantExt::Quadratic, QuantExt::IterativeBootstrap> my_curve;
             yieldts =
                 QuantLib::ext::make_shared<my_curve>(
 					asofDate_, instruments, zeroDayCounter_, QuantExt::Quadratic(1, 0, 1, 0, 1),
 					my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
 														   minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::LogQuadratic: {
             typedef PiecewiseYieldCurve<ZeroYield, QuantExt::LogQuadratic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, QuantExt::LogQuadratic(1, 0, -1, 0, 1),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::LogNaturalCubic: {
             typedef PiecewiseYieldCurve<ZeroYield, LogCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, LogCubic(CubicInterpolation::Kruger, true),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::LogFinancialCubic: {
             typedef PiecewiseYieldCurve<ZeroYield, LogCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_,
                 LogCubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                       CubicInterpolation::FirstDerivative),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor,
                                          dontThrowSteps));
         } break;
         case InterpolationMethod::LogCubicSpline: {
             typedef PiecewiseYieldCurve<ZeroYield, LogCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_,
                 LogCubic(CubicInterpolation::Spline, false, CubicInterpolation::SecondDerivative, 0.0,
                          CubicInterpolation::SecondDerivative, 0.0),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor,
                                          dontThrowSteps));
         } break;
         case InterpolationMethod::DefaultLogMixedLinearCubic: {
             typedef PiecewiseYieldCurve<ZeroYield, DefaultLogMixedLinearCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, DefaultLogMixedLinearCubic(mixedInterpolationSize_),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::MonotonicLogMixedLinearCubic: {
             typedef PiecewiseYieldCurve<ZeroYield, MonotonicLogMixedLinearCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, MonotonicLogMixedLinearCubic(mixedInterpolationSize_),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::KrugerLogMixedLinearCubic: {
             typedef PiecewiseYieldCurve<ZeroYield, KrugerLogMixedLinearCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, KrugerLogMixedLinearCubic(mixedInterpolationSize_),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::LogMixedLinearCubicNaturalSpline: {
             typedef PiecewiseYieldCurve<ZeroYield, LogMixedLinearCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_,
                 LogMixedLinearCubic(mixedInterpolationSize_, MixedInterpolation::ShareRanges,
                                     CubicInterpolation::Spline, false, CubicInterpolation::SecondDerivative, 0.0,
                                     CubicInterpolation::SecondDerivative, 0.0),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
        default:
            QL_FAIL("Interpolation method '" << interpolationMethod_ << "' not recognised.");
        }
        break;
    case InterpolationVariable::Discount:
        switch (interpolationMethod_) {
        case InterpolationMethod::Linear: {
            typedef PiecewiseYieldCurve<Discount, Linear, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, Linear(),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::LogLinear: {
            typedef PiecewiseYieldCurve<Discount, LogLinear, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, LogLinear(),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::NaturalCubic: {
            typedef PiecewiseYieldCurve<Discount, Cubic, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, Cubic(CubicInterpolation::Kruger, true),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::FinancialCubic: {
            typedef PiecewiseYieldCurve<Discount, Cubic, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_,
                Cubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                      CubicInterpolation::FirstDerivative),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::ConvexMonotone: {
            typedef PiecewiseYieldCurve<Discount, ConvexMonotone, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, ConvexMonotone(),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::Hermite: {
             typedef PiecewiseYieldCurve<Discount, Cubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, Cubic(CubicInterpolation::Parabolic),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::CubicSpline: {
             typedef PiecewiseYieldCurve<Discount, Cubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_,
                 Cubic(CubicInterpolation::Spline, false, CubicInterpolation::SecondDerivative, 0.0,
                       CubicInterpolation::SecondDerivative, 0.0),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::Quadratic: {
             typedef PiecewiseYieldCurve<Discount, QuantExt::Quadratic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, QuantExt::Quadratic(1, 0, 1, 0, 1),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::LogQuadratic: {
             typedef PiecewiseYieldCurve<Discount, QuantExt::LogQuadratic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, QuantExt::LogQuadratic(1, 0, -1, 0, 1),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::LogNaturalCubic: {
             typedef PiecewiseYieldCurve<Discount, LogCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, LogCubic(CubicInterpolation::Kruger, true),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::LogFinancialCubic: {
             typedef PiecewiseYieldCurve<Discount, LogCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_,
                 QuantLib::LogCubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                                 CubicInterpolation::FirstDerivative),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor,
                                          dontThrowSteps));
         } break;
         case InterpolationMethod::LogCubicSpline: {
             typedef PiecewiseYieldCurve<Discount,LogCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_,
                 LogCubic(CubicInterpolation::Spline, false, CubicInterpolation::SecondDerivative, 0.0,
                       CubicInterpolation::SecondDerivative, 0.0),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor,
                                          dontThrowSteps));
         } break;
         case InterpolationMethod::DefaultLogMixedLinearCubic: {
             typedef PiecewiseYieldCurve<Discount, DefaultLogMixedLinearCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, DefaultLogMixedLinearCubic(mixedInterpolationSize_),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::MonotonicLogMixedLinearCubic: {
             typedef PiecewiseYieldCurve<Discount, MonotonicLogMixedLinearCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, MonotonicLogMixedLinearCubic(mixedInterpolationSize_),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::KrugerLogMixedLinearCubic: {
             typedef PiecewiseYieldCurve<Discount, KrugerLogMixedLinearCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, KrugerLogMixedLinearCubic(mixedInterpolationSize_),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::LogMixedLinearCubicNaturalSpline: {
             typedef PiecewiseYieldCurve<Discount, LogMixedLinearCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_,
                 LogMixedLinearCubic(mixedInterpolationSize_, MixedInterpolation::ShareRanges,
                                     CubicInterpolation::Spline, false, CubicInterpolation::SecondDerivative, 0.0,
                                     CubicInterpolation::SecondDerivative, 0.0),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
        default:
            QL_FAIL("Interpolation method '" << interpolationMethod_ << "' not recognised.");
        }
        break;
    case InterpolationVariable::Forward:
        switch (interpolationMethod_) {
        case InterpolationMethod::Linear: {
            typedef PiecewiseYieldCurve<ForwardRate, Linear, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, Linear(),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::LogLinear: {
            typedef PiecewiseYieldCurve<ForwardRate, LogLinear, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, LogLinear(),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::NaturalCubic: {
            typedef PiecewiseYieldCurve<ForwardRate, Cubic, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, Cubic(CubicInterpolation::Kruger, true),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::FinancialCubic: {
            typedef PiecewiseYieldCurve<ForwardRate, Cubic, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_,
                Cubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                      CubicInterpolation::FirstDerivative),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::ConvexMonotone: {
            typedef PiecewiseYieldCurve<ForwardRate, ConvexMonotone, QuantExt::IterativeBootstrap> my_curve;
            yieldts = QuantLib::ext::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, ConvexMonotone(),
                my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::Hermite: {
             typedef PiecewiseYieldCurve<ForwardRate, Cubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, Cubic(CubicInterpolation::Parabolic),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::CubicSpline: {
             typedef PiecewiseYieldCurve<ForwardRate, Cubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_,
                 Cubic(CubicInterpolation::Spline, false, CubicInterpolation::SecondDerivative, 0.0,
                       CubicInterpolation::SecondDerivative, 0.0),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::Quadratic: {
             typedef PiecewiseYieldCurve<ForwardRate, QuantExt::Quadratic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, QuantExt::Quadratic(1, 0, 1, 0, 1),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::LogQuadratic: {
             typedef PiecewiseYieldCurve<ForwardRate, QuantExt::LogQuadratic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, QuantExt::LogQuadratic(1, 0, -1, 0, 1),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::LogNaturalCubic: {
             typedef PiecewiseYieldCurve<ForwardRate, LogCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, LogCubic(CubicInterpolation::Kruger, true),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor,
                                          dontThrowSteps));
         } break;
         case InterpolationMethod::LogFinancialCubic: {
             typedef PiecewiseYieldCurve<ForwardRate, LogCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_,
                 LogCubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                       CubicInterpolation::FirstDerivative),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor,
                                          dontThrowSteps));
         } break;
         case InterpolationMethod::LogCubicSpline: {
             typedef PiecewiseYieldCurve<ForwardRate, LogCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_,
                 LogCubic(CubicInterpolation::Spline, false, CubicInterpolation::SecondDerivative, 0.0,
                       CubicInterpolation::SecondDerivative, 0.0),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor,
                                          dontThrowSteps));
         } break;
         case InterpolationMethod::DefaultLogMixedLinearCubic: {
             typedef PiecewiseYieldCurve<ForwardRate, DefaultLogMixedLinearCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, DefaultLogMixedLinearCubic(mixedInterpolationSize_),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::MonotonicLogMixedLinearCubic: {
             typedef PiecewiseYieldCurve<ForwardRate, MonotonicLogMixedLinearCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, MonotonicLogMixedLinearCubic(mixedInterpolationSize_),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::KrugerLogMixedLinearCubic: {
             typedef PiecewiseYieldCurve<ForwardRate, KrugerLogMixedLinearCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_, KrugerLogMixedLinearCubic(mixedInterpolationSize_),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
         case InterpolationMethod::LogMixedLinearCubicNaturalSpline: {
             typedef PiecewiseYieldCurve<ForwardRate, LogMixedLinearCubic, QuantExt::IterativeBootstrap> my_curve;
             yieldts = QuantLib::ext::make_shared<my_curve>(
                 asofDate_, instruments, zeroDayCounter_,
                 LogMixedLinearCubic(mixedInterpolationSize_, MixedInterpolation::ShareRanges,
                                     CubicInterpolation::Spline, false, CubicInterpolation::SecondDerivative, 0.0,
                                     CubicInterpolation::SecondDerivative, 0.0),
                 my_curve::bootstrap_type(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                        minFactor, dontThrowSteps));
         } break;
        default:
            QL_FAIL("Interpolation method '" << interpolationMethod_ << "' not recognised.");
        }
        break;
    default:
        QL_FAIL("Interpolation variable not recognised.");
    }

    if (preserveQuoteLinkage_)
        p_ = yieldts;
    else {
        // Build fixed zero/discount curve that matches the bootstrapped curve
        // initially, but does NOT react to quote changes: This is a workaround
        // for a QuantLib problem, where a fixed reference date piecewise
        // yield curve reacts to evaluation date changes because the bootstrap
        // helper recompute their start date (because they are relative date
        // helper for deposits, fras, swaps, etc.).
        vector<Date> dates(instruments.size() + 1, asofDate_);
        vector<Real> zeros(instruments.size() + 1, 0.0);
        vector<Real> discounts(instruments.size() + 1, 1.0);
        vector<Real> forwards(instruments.size() + 1, 0.0);

        if (extrapolation_) {
            yieldts->enableExtrapolation();
        }
        for (Size i = 0; i < instruments.size(); i++) {
            dates[i + 1] = instruments[i]->pillarDate();
            zeros[i + 1] = yieldts->zeroRate(dates[i + 1], zeroDayCounter_, Continuous);
            discounts[i + 1] = yieldts->discount(dates[i + 1]);
            forwards[i + 1] = yieldts->forwardRate(dates[i + 1], dates[i + 1], zeroDayCounter_, Continuous);
        }
        zeros[0] = zeros[1];
        forwards[0] = forwards[1];
        if (interpolationVariable_ == InterpolationVariable::Zero)
            p_ = zerocurve(dates, zeros, zeroDayCounter_, interpolationMethod_, mixedInterpolationSize_);
        else if (interpolationVariable_ == InterpolationVariable::Discount)
            p_ = discountcurve(dates, discounts, zeroDayCounter_, interpolationMethod_, mixedInterpolationSize_);
        else if (interpolationVariable_ == InterpolationVariable::Forward)
            p_ = forwardcurve(dates, forwards, zeroDayCounter_, interpolationMethod_, mixedInterpolationSize_);
        else
            QL_FAIL("Interpolation variable not recognised.");
    }

    // set calibration info
    if (buildCalibrationInfo_) {
        calibrationInfo_ = QuantLib::ext::make_shared<PiecewiseYieldCurveCalibrationInfo>();
        for (Size i = 0; i < instruments.size(); ++i) {
            calibrationInfo_->pillarDates.push_back(instruments[i]->pillarDate());
        }
    }

    return p_;
}

void YieldCurve::buildZeroCurve() {

    QL_REQUIRE(curveSegments_.size() <= 1, "More than one zero curve "
                                           "segment not supported yet.");
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::Zero, "The curve segment is not of type Zero.");

    const QuantLib::ext::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();

    // Fill a vector of zero quotes.
    vector<QuantLib::ext::shared_ptr<ZeroQuote>> zeroQuotes;
    QuantLib::ext::shared_ptr<DirectYieldCurveSegment> zeroCurveSegment =
        QuantLib::ext::dynamic_pointer_cast<DirectYieldCurveSegment>(curveSegments_[0]);
    auto zeroQuoteIDs = zeroCurveSegment->quotes();

    for (Size i = 0; i < zeroQuoteIDs.size(); ++i) {
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(zeroQuoteIDs[i], asofDate_);
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::ZERO,
                       "Market quote not of type zero.");
            QuantLib::ext::shared_ptr<ZeroQuote> zeroQuote = QuantLib::ext::dynamic_pointer_cast<ZeroQuote>(marketQuote);
            zeroQuotes.push_back(zeroQuote);
        }
    }

    // Create the (date, zero) pairs.
    map<Date, Rate> data;
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(curveSegments_[0]->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << curveSegments_[0]->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::Zero, "Conventions ID does not give zero rate conventions.");
    QuantLib::ext::shared_ptr<ZeroRateConvention> zeroConvention = QuantLib::ext::dynamic_pointer_cast<ZeroRateConvention>(convention);
    DayCounter quoteDayCounter = zeroConvention->dayCounter();
    for (Size i = 0; i < zeroQuotes.size(); ++i) {
        QL_REQUIRE(quoteDayCounter == zeroQuotes[i]->dayCounter(),
                   "The day counter should be the same between the conventions"
                   "and the quote.");
        if (!zeroQuotes[i]->tenorBased()) {
            data[zeroQuotes[i]->date()] = zeroQuotes[i]->quote()->value();
        } else {
            QL_REQUIRE(zeroConvention->tenorBased(), "Using tenor based "
                                                     "zero rates without tenor based zero rate conventions.");
            Date zeroDate = asofDate_;
            if (zeroConvention->spotLag() > 0) {
                zeroDate = zeroConvention->spotCalendar().advance(zeroDate, zeroConvention->spotLag() * Days);
            }
            zeroDate = zeroConvention->tenorCalendar().advance(zeroDate, zeroQuotes[i]->tenor(),
                                                               zeroConvention->rollConvention(), zeroConvention->eom());
            data[zeroDate] = zeroQuotes[i]->quote()->value();
        }
    }

    QL_REQUIRE(data.size() > 0, "No market data found for curve spec " << curveSpec_.name() << " with as of date "
                                                                       << io::iso_date(asofDate_));

    // \todo review - more flexible (flat vs. linear extrap)?
    if (data.begin()->first > asofDate_) {
        Rate rate = data.begin()->second;
        data[asofDate_] = rate;
        DLOG("Insert zero curve point at time zero for " << curveSpec_.name() << ": "
                                                        << "date " << QuantLib::io::iso_date(asofDate_) << ", "
                                                        << "zero " << fixed << setprecision(4) << data[asofDate_]);
    }

    QL_REQUIRE(data.size() > 1, "The single zero rate quote provided "
                                "should be associated with a date greater than as of date.");

    // First build temporary curves
    vector<Date> dates;
    vector<Rate> zeroes, discounts;
    dates.push_back(data.begin()->first);
    zeroes.push_back(data.begin()->second);
    discounts.push_back(1.0);

    Compounding zeroCompounding = zeroConvention->compounding();
    Frequency zeroCompoundingFreq = zeroConvention->compoundingFrequency();
    map<Date, Rate>::iterator it;
    for (it = ++data.begin(); it != data.end(); ++it) {
        dates.push_back(it->first);
        InterestRate tempRate(it->second, quoteDayCounter, zeroCompounding, zeroCompoundingFreq);
        Time t = quoteDayCounter.yearFraction(asofDate_, it->first);
        /* Convert zero rate to continuously compounded if necessary */
        if (zeroCompounding == Continuous) {
            zeroes.push_back(it->second);
        } else {
            zeroes.push_back(tempRate.equivalentRate(Continuous, NoFrequency, t));
        }
        discounts.push_back(tempRate.discountFactor(t));
        DLOG("Add zero curve point for " << curveSpec_.name() << ": " << io::iso_date(dates.back()) << " " << fixed
                                        << setprecision(4) << zeroes.back() << " / " << discounts.back());
    }

    QL_REQUIRE(dates.size() == zeroes.size(), "Date and zero vectors differ in size.");
    QL_REQUIRE(dates.size() == discounts.size(), "Date and discount vectors differ in size.");

    // Now build curve with requested conventions
    if (interpolationVariable_ == YieldCurve::InterpolationVariable::Zero) {
        QuantLib::ext::shared_ptr<YieldTermStructure> tempCurve =
            zerocurve(dates, zeroes, quoteDayCounter, interpolationMethod_);
        zeroes.clear();
        for (Size i = 0; i < dates.size(); ++i) {
            Rate zero = tempCurve->zeroRate(dates[i], zeroDayCounter_, Continuous);
            zeroes.push_back(zero);
        }
        p_ = zerocurve(dates, zeroes, zeroDayCounter_, interpolationMethod_);
    } else if (interpolationVariable_ == YieldCurve::InterpolationVariable::Discount) {
        QuantLib::ext::shared_ptr<YieldTermStructure> tempCurve =
            discountcurve(dates, discounts, quoteDayCounter, interpolationMethod_);
        discounts.clear();
        for (Size i = 0; i < dates.size(); ++i) {
            DiscountFactor discount = tempCurve->discount(dates[i]);
            discounts.push_back(discount);
        }
        p_ = discountcurve(dates, discounts, zeroDayCounter_, interpolationMethod_);
    } else {
        QL_FAIL("Unknown yield curve interpolation variable.");
    }
}

void YieldCurve::buildZeroSpreadedCurve() {
    QL_REQUIRE(curveSegments_.size() <= 1, "More than one zero spreaded curve "
                                           "segment not supported yet.");
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::ZeroSpread,
               "The curve segment is not of type Zero Spread.");

    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    
    // Fill a vector of zero spread quotes.
    vector<QuantLib::ext::shared_ptr<ZeroQuote>> quotes;
    QuantLib::ext::shared_ptr<ZeroSpreadedYieldCurveSegment> segment =
        QuantLib::ext::dynamic_pointer_cast<ZeroSpreadedYieldCurveSegment>(curveSegments_[0]);
    auto quoteIDs = segment->quotes();

    Date today = Settings::instance().evaluationDate();
    vector<Date> dates;
    vector<Handle<Quote>> quoteHandles;
    for (Size i = 0; i < quoteIDs.size(); ++i) {
        if (QuantLib::ext::shared_ptr<MarketDatum> md = loader_.get(quoteIDs[i], asofDate_)) {
            QL_REQUIRE(md->instrumentType() == MarketDatum::InstrumentType::ZERO, "Market quote not of type zero.");
            QL_REQUIRE(md->quoteType() == MarketDatum::QuoteType::YIELD_SPREAD,
                       "Market quote not of type yield spread.");
            QuantLib::ext::shared_ptr<ZeroQuote> zeroQuote = QuantLib::ext::dynamic_pointer_cast<ZeroQuote>(md);
            quotes.push_back(zeroQuote);
            dates.push_back(zeroQuote->tenorBased() ? today + zeroQuote->tenor() : zeroQuote->date());
            quoteHandles.push_back(zeroQuote->quote());
        }
    }

    QL_REQUIRE(!quotes.empty(),
               "Cannot build curve with spec " << curveSpec_.name() << " because there are no spread quotes");

    string referenceCurveID = segment->referenceCurveID();
    QuantLib::ext::shared_ptr<YieldCurve> referenceCurve;
    if (referenceCurveID != curveConfig_->curveID() && !referenceCurveID.empty()) {
        referenceCurveID = yieldCurveKey(currency_, referenceCurveID, asofDate_);
        map<string, QuantLib::ext::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(referenceCurveID);
        if (it != requiredYieldCurves_.end()) {
            referenceCurve = it->second;
        } else {
            QL_FAIL("The reference curve, " << referenceCurveID
                                            << ", required in the building "
                                               "of the curve, "
                                            << curveSpec_.name() << ", was not found.");
        }
    }

    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::Zero, "Conventions ID does not give zero rate conventions.");
    QuantLib::ext::shared_ptr<ZeroRateConvention> zeroConvention = QuantLib::ext::dynamic_pointer_cast<ZeroRateConvention>(convention);
    DayCounter quoteDayCounter = zeroConvention->dayCounter();
    Compounding comp = zeroConvention->compounding();
    Frequency freq = zeroConvention->compoundingFrequency();

    p_ = QuantLib::ext::shared_ptr<YieldTermStructure>(new PiecewiseZeroSpreadedTermStructure(
        referenceCurve->handle(), quoteHandles, dates, comp, freq, quoteDayCounter));
}

void YieldCurve::buildWeightedAverageCurve() {
    QL_REQUIRE(curveSegments_.size() == 1,
               "One segment required for weighted average curve, got " << curveSegments_.size());
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::WeightedAverage,
               "The curve segment is not of type Weighted Average.");
    auto segment = QuantLib::ext::dynamic_pointer_cast<WeightedAverageYieldCurveSegment>(curveSegments_[0]);
    QL_REQUIRE(segment != nullptr, "expected WeightedAverageYieldCurveSegment, this is unexpected");
    auto it1 = requiredYieldCurves_.find(yieldCurveKey(currency_, segment->referenceCurveID1(), asofDate_));
    auto it2 = requiredYieldCurves_.find(yieldCurveKey(currency_, segment->referenceCurveID2(), asofDate_));
    QL_REQUIRE(it1 != requiredYieldCurves_.end(), "Could not find reference curve1: " << segment->referenceCurveID1());
    QL_REQUIRE(it2 != requiredYieldCurves_.end(), "Could not find reference curve2: " << segment->referenceCurveID2());
    p_ = QuantLib::ext::make_shared<WeightedYieldTermStructure>(it1->second->handle(), it2->second->handle(),
                                                        segment->weight1(), segment->weight2());
}

void YieldCurve::buildYieldPlusDefaultCurve() {
    QL_REQUIRE(curveSegments_.size() == 1,
               "One segment required for yield plus default curve, got " << curveSegments_.size());
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::YieldPlusDefault,
               "The curve segment is not of type Yield Plus Default.");
    auto segment = QuantLib::ext::dynamic_pointer_cast<YieldPlusDefaultYieldCurveSegment>(curveSegments_[0]);
    QL_REQUIRE(segment != nullptr, "expected YieldPlusDefaultCurveSegment, this is unexpected");
    auto it = requiredYieldCurves_.find(yieldCurveKey(currency_, segment->referenceCurveID(), asofDate_));
    QL_REQUIRE(it != requiredYieldCurves_.end(), "Could not find reference curve: " << segment->referenceCurveID());
    std::vector<Handle<DefaultProbabilityTermStructure>> defaultCurves;
    std::vector<Handle<Quote>> recRates;
    for (Size i = 0; i < segment->defaultCurveIDs().size(); ++i) {
        auto it = requiredDefaultCurves_.find(segment->defaultCurveIDs()[i]);
        QL_REQUIRE(it != requiredDefaultCurves_.end(),
                   "Could not find default curve: " << segment->defaultCurveIDs()[i]);
        defaultCurves.push_back(Handle<DefaultProbabilityTermStructure>(it->second->creditCurve()->curve()));
        recRates.push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(it->second->recoveryRate())));
    }
    p_ = QuantLib::ext::make_shared<YieldPlusDefaultYieldTermStructure>(it->second->handle(), defaultCurves, recRates,
                                                                segment->weights());
}

void YieldCurve::buildIborFallbackCurve() {
    QL_REQUIRE(curveSegments_.size() == 1,
               "One segment required for ibor fallback curve, got " << curveSegments_.size());
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::IborFallback,
               "The curve segment is not of type Ibor Fallback");
    auto segment = QuantLib::ext::dynamic_pointer_cast<IborFallbackCurveSegment>(curveSegments_[0]);
    QL_REQUIRE(segment != nullptr, "expected IborFallbackCurve, internal error");
    auto it = requiredYieldCurves_.find(segment->rfrCurve());
    QL_REQUIRE(it != requiredYieldCurves_.end(), "Could not find rfr curve: '" << segment->rfrCurve() << "')");
    QL_REQUIRE(
        (segment->rfrIndex() && segment->spread()) || iborFallbackConfig_.isIndexReplaced(segment->iborIndex()),
        "buildIborFallbackCurve(): ibor index '"
            << segment->iborIndex()
            << "' must be specified in ibor fallback config, if RfrIndex or Spread is not specified in curve config");
    std::string rfrIndexName = segment->rfrIndex() ? *segment->rfrIndex() : iborFallbackConfig_.fallbackData(segment->iborIndex()).rfrIndex;
    Real spread = segment->spread() ? *segment->spread() : iborFallbackConfig_.fallbackData(segment->iborIndex()).spread;
    // we don't support convention based indices here, this might change with ore ticket 1758
    Handle<YieldTermStructure> dummyCurve(QuantLib::ext::make_shared<FlatForward>(asofDate_, 0.0, zeroDayCounter_));
    auto originalIndex = parseIborIndex(segment->iborIndex(), dummyCurve);
    auto rfrIndex = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(parseIborIndex(rfrIndexName, it->second->handle()));
    QL_REQUIRE(rfrIndex, "buidlIborFallbackCurve(): rfr index '"
                             << rfrIndexName << "' could not be cast to OvernightIndex, is this index name correct?");
    DLOG("building ibor fallback curve for '" << segment->iborIndex() << "' with rfrIndex='" << rfrIndexName
                                              << "' and spread=" << spread);
    if (auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(originalIndex)) {
        p_ = QuantLib::ext::make_shared<OvernightFallbackCurve>(on, rfrIndex, spread, Date::minDate());
    } else {
        p_ = QuantLib::ext::make_shared<IborFallbackCurve>(originalIndex, rfrIndex, spread, Date::minDate());
    }
}

void YieldCurve::buildDiscountCurve() {

    QL_REQUIRE(curveSegments_.size() <= 1, "More than one discount curve "
                                           "segment not supported yet.");
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::Discount,
               "The curve segment is not of type Discount.");

    // Create the (date, discount) pairs.
    map<Date, DiscountFactor> data;
    QuantLib::ext::shared_ptr<DirectYieldCurveSegment> discountCurveSegment =
        QuantLib::ext::dynamic_pointer_cast<DirectYieldCurveSegment>(curveSegments_[0]);
    auto discountQuoteIDs = discountCurveSegment->quotes();

    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention;

    vector<string> quotes;
    quotes.reserve(discountQuoteIDs.size()); // Reserve space for efficiency

    std::transform(discountQuoteIDs.begin(), discountQuoteIDs.end(), std::back_inserter(quotes),
                   [](const std::pair<string, bool>& pair) {
                       return pair.first; // Extract only the quote part
                   });

    auto wildcard = getUniqueWildcard(quotes);

    std::set<QuantLib::ext::shared_ptr<MarketDatum>> marketData;
    if (wildcard) {
        marketData = loader_.get(*wildcard, asofDate_);
    } else {
        std::ostringstream ss;
        ss << MarketDatum::InstrumentType::DISCOUNT << "/" << MarketDatum::QuoteType::RATE << "/" << currency_ << "/*";
        Wildcard w(ss.str());
        marketData = loader_.get(w, asofDate_);
    }

    for (const auto& marketQuote : marketData) {
        QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::DISCOUNT,
                    "Market quote not of type Discount.");
        QuantLib::ext::shared_ptr<DiscountQuote> discountQuote = QuantLib::ext::dynamic_pointer_cast<DiscountQuote>(marketQuote);

        // filtering
        if (!wildcard) {
            vector<string>::const_iterator it = find(quotes.begin(), quotes.end(), discountQuote->name());
            if (it == quotes.end())
                continue;
        }

        if (discountQuote->date() != Date()) {

            data[discountQuote->date()] = discountQuote->quote()->value();

        } else if (discountQuote->tenor() != Period()) {

            if (!convention)
                convention = conventions->get(discountCurveSegment->conventionsID());
            QuantLib::ext::shared_ptr<ZeroRateConvention> zeroConvention =
                QuantLib::ext::dynamic_pointer_cast<ZeroRateConvention>(convention);
            QL_REQUIRE(zeroConvention, "could not cast to ZeroRateConvention");

            Calendar cal = zeroConvention->tenorCalendar();
            BusinessDayConvention rollConvention = zeroConvention->rollConvention();
            Date date = cal.adjust(cal.adjust(asofDate_, rollConvention) + discountQuote->tenor(), rollConvention);
            DLOG("YieldCurve::buildDiscountCurve - tenor " << discountQuote->tenor() << " to date "
                                                            << io::iso_date(date));
            data[date] = discountQuote->quote()->value();

        } else {
            QL_FAIL("YieldCurve::buildDiscountCurve - neither date nor tenor recognised");
        }
    }

    // Some logging and checks
    QL_REQUIRE(data.size() > 0, "No market data found for curve spec " << curveSpec_.name() << " with as of date "
                                                                       << io::iso_date(asofDate_));
    if (!wildcard) {
        QL_REQUIRE(data.size() == quotes.size(), "Found " << data.size() << " quotes, but "
                                                          << quotes.size()
                                                          << " quotes given in config " << curveConfig_->curveID());
    }

    if (data.begin()->first > asofDate_) {
        DLOG("Insert discount curve point at time zero for " << curveSpec_.name());
        data[asofDate_] = 1.0;
    }

    QL_REQUIRE(data.size() > 1, "The single discount quote provided "
                                "should be associated with a date greater than as of date.");

    // First build a temporary discount curve
    vector<Date> dates;
    vector<DiscountFactor> discounts;
    map<Date, DiscountFactor>::iterator it;
    for (it = data.begin(); it != data.end(); ++it) {
        dates.push_back(it->first);
        discounts.push_back(it->second);
        DLOG("Add discount curve point for " << curveSpec_.name() << ": " << io::iso_date(dates.back()) << " "
                                            << discounts.back());
    }

    QL_REQUIRE(dates.size() == discounts.size(), "Date and discount vectors differ in size.");

    QuantLib::ext::shared_ptr<YieldTermStructure> tempDiscCurve =
        discountcurve(dates, discounts, zeroDayCounter_, interpolationMethod_);

    // Now build curve with requested conventions
    if (interpolationVariable_ == YieldCurve::InterpolationVariable::Discount) {
        p_ = tempDiscCurve;
    } else if (interpolationVariable_ == YieldCurve::InterpolationVariable::Zero) {
        vector<Rate> zeroes;
        for (Size i = 0; i < dates.size(); ++i) {
            Rate zero = tempDiscCurve->zeroRate(dates[i], zeroDayCounter_, Continuous);
            zeroes.push_back(zero);
        }
        p_ = zerocurve(dates, zeroes, zeroDayCounter_, interpolationMethod_);
    } else {
        QL_FAIL("Unknown yield curve interpolation variable.");
    }
}

void YieldCurve::buildBootstrappedCurve() {

    QL_REQUIRE(!curveSegments_.empty(), "no curve segments defined.");

    /* Loop over segments and add helpers for each segment. */

    DLOG("Building instrument sets for yield curve segments 0..." << curveSegments_.size() - 1);

    std::vector<vector<QuantLib::ext::shared_ptr<RateHelper>>> instrumentsPerSegment(curveSegments_.size());

    for (Size i = 0; i < curveSegments_.size(); ++i) {
        switch (curveSegments_[i]->type()) {
        case YieldCurveSegment::Type::Deposit:
            addDeposits(curveSegments_[i], instrumentsPerSegment[i]);
            break;
        case YieldCurveSegment::Type::FRA:
            addFras(curveSegments_[i], instrumentsPerSegment[i]);
            break;
        case YieldCurveSegment::Type::Future:
            addFutures(curveSegments_[i], instrumentsPerSegment[i]);
            break;
        case YieldCurveSegment::Type::OIS:
            addOISs(curveSegments_[i], instrumentsPerSegment[i]);
            break;
        case YieldCurveSegment::Type::Swap:
            addSwaps(curveSegments_[i], instrumentsPerSegment[i]);
            break;
        case YieldCurveSegment::Type::AverageOIS:
            addAverageOISs(curveSegments_[i], instrumentsPerSegment[i]);
            break;
        case YieldCurveSegment::Type::TenorBasis:
            addTenorBasisSwaps(curveSegments_[i], instrumentsPerSegment[i]);
            break;
        case YieldCurveSegment::Type::TenorBasisTwo:
            addTenorBasisTwoSwaps(curveSegments_[i], instrumentsPerSegment[i]);
            break;
        case YieldCurveSegment::Type::BMABasis:
            addBMABasisSwaps(curveSegments_[i], instrumentsPerSegment[i]);
            break;
        case YieldCurveSegment::Type::FXForward:
            addFXForwards(curveSegments_[i], instrumentsPerSegment[i]);
            break;
        case YieldCurveSegment::Type::CrossCcyBasis:
            addCrossCcyBasisSwaps(curveSegments_[i], instrumentsPerSegment[i]);
            break;
        case YieldCurveSegment::Type::CrossCcyFixFloat:
            addCrossCcyFixFloatSwaps(curveSegments_[i], instrumentsPerSegment[i]);
            break;
        default:
            QL_FAIL("Yield curve segment type not recognized.");
            break;
        }
    }

    /* If we have two instruments with identical pillar dates wthin a segment, remove the earlier one */

    for (Size i = 0; i < curveSegments_.size(); ++i) {
        for (auto it = instrumentsPerSegment[i].begin(); it != instrumentsPerSegment[i].end();) {
            if (std::next(it, 1) != instrumentsPerSegment[i].end() &&
                (*it)->pillarDate() == (*std::next(it, 1))->pillarDate()) {
                DLOG("Removing instrument with pillar date "
                     << (*it)->pillarDate() << " in segment #" << i
                     << " because the next instrument in the same segment has the same pillar date");
                it = instrumentsPerSegment[i].erase(it);
            } else
                ++it;
        }
    }

    /* Determine min and max pillar date in each segment */

    std::vector<std::pair<Date, Date>> minMaxDatePerSegment(curveSegments_.size(),
                                                            std::make_pair(Date::maxDate(), Date::minDate()));
    for (Size i = 0; i < curveSegments_.size(); ++i) {
        if (!instrumentsPerSegment[i].empty()) {
            auto [minIt, maxIt] =
                std::minmax_element(instrumentsPerSegment[i].begin(), instrumentsPerSegment[i].end(),
                                    [](const QuantLib::ext::shared_ptr<RateHelper>& h, const QuantLib::ext::shared_ptr<RateHelper>& j) {
                                        return h->pillarDate() < h->pillarDate();
                                    });
            minMaxDatePerSegment[i] = std::make_pair((*minIt)->pillarDate(), (*maxIt)->pillarDate());
        }
    }

    /* If there are two segments with different priorities and overlapping instruments, remove instruments as
     * appropriate */

    for (Size i = 0; i < curveSegments_.size(); ++i) {
        if (i < curveSegments_.size() - 1 && curveSegments_[i]->priority() > curveSegments_[i + 1]->priority()) {
            for (auto it = instrumentsPerSegment[i].begin(); it != instrumentsPerSegment[i].end();) {
                if ((*it)->pillarDate() > minMaxDatePerSegment[i + 1].first - curveSegments_[i]->minDistance()) {
                    DLOG("Removing instrument in segment #"
                         << i << " (priority " << curveSegments_[i]->priority() << ") because its pillar date "
                         << (*it)->pillarDate() << " > " << minMaxDatePerSegment[i + 1].first
                         << " (min pillar date in segment #" << (i + 1) << ", priority "
                         << curveSegments_[i + 1]->priority() << ") minus " << curveSegments_[i]->minDistance()
                         << " (min distance in segment #" << i << ")");
                    it = instrumentsPerSegment[i].erase(it);
                } else
                    ++it;
            }
        }
        if (i > 0 && curveSegments_[i - 1]->priority() < curveSegments_[i]->priority()) {
            for (auto it = instrumentsPerSegment[i].begin(); it != instrumentsPerSegment[i].end();) {
                if ((*it)->pillarDate() < minMaxDatePerSegment[i - 1].second + curveSegments_[i - 1]->minDistance()) {
                    DLOG("Removing instrument in segment #"
                         << i << " (priority " << curveSegments_[i]->priority() << ") because its pillar date "
                         << (*it)->pillarDate() << " < " << minMaxDatePerSegment[i - 1].second
                         << " (max pillar date in segment #" << (i - 1) << ", priority "
                         << curveSegments_[i - 1]->priority() << ") plus " << curveSegments_[i - 1]->minDistance()
                         << " (min distance in segment #" << (i - 1) << ")");
                    it = instrumentsPerSegment[i].erase(it);
                } else
                    ++it;
            }
        }
    }

    /* Set mixed interpolation size using the specified cutoff for the number of segments */

    mixedInterpolationSize_ = 0;
    for (Size i = 0; i < curveConfig_->mixedInterpolationCutoff(); ++i)
        mixedInterpolationSize_ += instrumentsPerSegment[i].size();

    /* Now put all remaining instruments into a single vector. */

    vector<QuantLib::ext::shared_ptr<RateHelper>> instruments;
    for (Size i = 0; i < curveSegments_.size(); ++i) {
        instruments.insert(instruments.end(), instrumentsPerSegment[i].begin(), instrumentsPerSegment[i].end());
        DLOG("Adding " << instrumentsPerSegment[i].size() << " instruments for segment #" << i);
    }

    /* Build the bootstrapped curve from the instruments. */

    DLOG("Bootstrapping with " << instruments.size() << " instruments");
    QL_REQUIRE(instruments.size() > 0,
               "Empty instrument list for date = " << io::iso_date(asofDate_) << " and curve = " << curveSpec_.name());
    p_ = piecewisecurve(instruments);
}

void YieldCurve::buildDiscountRatioCurve() {
    QL_REQUIRE(curveSegments_.size() == 1, "A discount ratio curve must contain exactly one segment");
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::DiscountRatio,
               "The curve segment is not of type 'DiscountRatio'.");

    QuantLib::ext::shared_ptr<DiscountRatioYieldCurveSegment> segment =
        QuantLib::ext::dynamic_pointer_cast<DiscountRatioYieldCurveSegment>(curveSegments_[0]);

    // Find the underlying curves in the reference curves
    QuantLib::ext::shared_ptr<YieldCurve> baseCurve = getYieldCurve(segment->baseCurveCurrency(), segment->baseCurveId());
    QL_REQUIRE(baseCurve, "The base curve '" << segment->baseCurveId() << "' cannot be empty");

    QuantLib::ext::shared_ptr<YieldCurve> numCurve =
        getYieldCurve(segment->numeratorCurveCurrency(), segment->numeratorCurveId());
    QL_REQUIRE(numCurve, "The numerator curve '" << segment->numeratorCurveId() << "' cannot be empty");

    QuantLib::ext::shared_ptr<YieldCurve> denCurve =
        getYieldCurve(segment->denominatorCurveCurrency(), segment->denominatorCurveId());
    QL_REQUIRE(denCurve, "The denominator curve '" << segment->denominatorCurveId() << "' cannot be empty");

    p_ = QuantLib::ext::make_shared<DiscountRatioModifiedCurve>(baseCurve->handle(), numCurve->handle(), denCurve->handle());
}

QuantLib::ext::shared_ptr<YieldCurve> YieldCurve::getYieldCurve(const string& ccy, const string& id) const {
    if (id != curveConfig_->curveID() && !id.empty()) {
        string idLookup = yieldCurveKey(parseCurrency(ccy), id, asofDate_);
        map<string, QuantLib::ext::shared_ptr<YieldCurve>>::const_iterator it = requiredYieldCurves_.find(idLookup);
        QL_REQUIRE(it != requiredYieldCurves_.end(), "The curve '" << idLookup
                                                                   << "' required in the building of the curve '"
                                                                   << curveSpec_.name() << "' was not found.");
        return it->second;
    } else {
        return nullptr;
    }
}

void YieldCurve::buildFittedBondCurve() {
    QL_REQUIRE(curveSegments_.size() == 1, "FittedBond curve must contain exactly one segment.");
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::FittedBond,
               "The curve segment is not of type 'FittedBond'.");

    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    
    QuantLib::ext::shared_ptr<FittedBondYieldCurveSegment> curveSegment =
        QuantLib::ext::dynamic_pointer_cast<FittedBondYieldCurveSegment>(curveSegments_[0]);
    QL_REQUIRE(curveSegment != nullptr, "could not cast to FittedBondYieldCurveSegment, this is unexpected");

    // init calibration info for this curve

    auto calInfo = QuantLib::ext::make_shared<FittedBondCurveCalibrationInfo>();
    if (buildCalibrationInfo_) {
        calInfo->dayCounter = zeroDayCounter_.name();
        calInfo->currency = currency_.code();
    }

    // build vector of bond helpers

    auto quoteIDs = curveSegment->quotes();
    std::vector<QuantLib::ext::shared_ptr<QuantLib::Bond>> bonds;
    std::vector<QuantLib::ext::shared_ptr<BondHelper>> helpers;
    std::vector<Real> marketPrices, marketYields;
    std::vector<std::string> securityIDs;
    std::vector<Date> securityMaturityDates;
    Date lastMaturity = Date::minDate(), firstMaturity = Date::maxDate();

    // not really relevant, we just need a working engine configuration so that the bond can be built
    // the pricing engine here is _not_ used during the curve fitting, for this a local engine is
    // set up within FittedBondDiscountCurve
    auto engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";
    engineData->engineParameters("Bond") = {{"TimestepPeriod", "6M"}};

    std::map<std::string, Handle<YieldTermStructure>> iborCurveMapping;
    for (auto const& c : curveSegment->iborIndexCurves()) {
        auto index = parseIborIndex(c.first);
        auto key = yieldCurveKey(index->currency(), c.second, asofDate_);
        auto y = requiredYieldCurves_.find(key);
        QL_REQUIRE(y != requiredYieldCurves_.end(), "required yield curve '" << key << "' for iborIndex '" << c.first
                                                                             << "' not provided for fitted bond curve");
        iborCurveMapping[c.first] = y->second->handle();
    }

    auto engineFactory =
        QuantLib::ext::make_shared<EngineFactory>(engineData, QuantLib::ext::make_shared<FittedBondCurveHelperMarket>(iborCurveMapping),
                                          std::map<MarketContext, string>(), referenceData_, iborFallbackConfig_);

    for (Size i = 0; i < quoteIDs.size(); ++i) {
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(quoteIDs[i], asofDate_);
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BOND &&
                           marketQuote->quoteType() == MarketDatum::QuoteType::PRICE,
                       "Market quote not of type Bond / Price.");
            QuantLib::ext::shared_ptr<BondPriceQuote> bondQuote = QuantLib::ext::dynamic_pointer_cast<BondPriceQuote>(marketQuote);
            QL_REQUIRE(bondQuote, "market quote has type bond quote, but can not be casted, this is unexpected.");
	        auto m = [](Real x) { return 100.0 * x; };
            Handle<Quote> rescaledBondQuote(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(bondQuote->quote(), m));
            string securityID = bondQuote->securityID();

            QL_REQUIRE(referenceData_ != nullptr, "reference data required to build fitted bond curve");
            auto res = BondFactory::instance().build(engineFactory, referenceData_, securityID);
            auto qlInstr = res.bond;
            // skip bonds with settlement date <= curve reference date or which are otherwise non-tradeable
            if (qlInstr->settlementDate() > asofDate_ && QuantLib::BondFunctions::isTradable(*qlInstr)) {
                bonds.push_back(qlInstr);
                helpers.push_back(QuantLib::ext::make_shared<BondHelper>(rescaledBondQuote, qlInstr));
                Date thisMaturity = qlInstr->maturityDate();
                lastMaturity = std::max(lastMaturity, thisMaturity);
                firstMaturity = std::min(firstMaturity, thisMaturity);
                Real inflationFactor = res.inflationFactor();
                Real marketYield = qlInstr->yield(rescaledBondQuote->value() * inflationFactor,
                                                  ActualActual(ActualActual::ISDA),
                                                  Continuous, NoFrequency);
                DLOG("added bond " << securityID << ", maturity = " << QuantLib::io::iso_date(thisMaturity)
                                   << ", clean price = " << rescaledBondQuote->value() * inflationFactor
                                   << ", yield (cont,act/act) = " << marketYield);
                marketPrices.push_back(bondQuote->quote()->value() * inflationFactor);
                securityIDs.push_back(securityID);
                marketYields.push_back(marketYield);
                securityMaturityDates.push_back(thisMaturity);
            } else {
                DLOG("skipped bond " << securityID << " with settlement date "
                                     << QuantLib::io::iso_date(qlInstr->settlementDate()) << ", isTradable = "
                                     << std::boolalpha << QuantLib::BondFunctions::isTradable(*qlInstr));
            }
        }
    }

    calInfo->securities = securityIDs;
    calInfo->securityMaturityDates = securityMaturityDates;
    calInfo->marketPrices = marketPrices;
    calInfo->marketYields = marketYields;

    // fit bond curve to helpers

    QL_REQUIRE(helpers.size() >= 1, "no bonds for fitting bond curve");
    DLOG("Fitting bond curve with " << helpers.size() << " bonds.");

    Real minCutoffTime = 0.0, maxCutoffTime = QL_MAX_REAL;
    if (curveSegment->extrapolateFlat()) {
        minCutoffTime = zeroDayCounter_.yearFraction(asofDate_, firstMaturity);
        maxCutoffTime = zeroDayCounter_.yearFraction(asofDate_, lastMaturity);
        DLOG("extrapolate flat outside " << minCutoffTime << "," << maxCutoffTime);
    }

    QuantLib::ext::shared_ptr<FittedBondDiscountCurve::FittingMethod> method;
    switch (interpolationMethod_) {
    case InterpolationMethod::ExponentialSplines:
        method = QuantLib::ext::make_shared<ExponentialSplinesFitting>(true, Array(), ext::shared_ptr<OptimizationMethod>(),
                                                               Array(), minCutoffTime, maxCutoffTime);
        calInfo->fittingMethod = "ExponentialSplines";
        break;
    case InterpolationMethod::NelsonSiegel:
        method = QuantLib::ext::make_shared<NelsonSiegelFitting>(Array(), ext::shared_ptr<OptimizationMethod>(), Array(),
                                                         minCutoffTime, maxCutoffTime);
        calInfo->fittingMethod = "NelsonSiegel";
        break;
    case InterpolationMethod::Svensson:
        method = QuantLib::ext::make_shared<SvenssonFitting>(Array(), ext::shared_ptr<OptimizationMethod>(), Array(),
                                                     minCutoffTime, maxCutoffTime);
        calInfo->fittingMethod = "Svensson";
        break;
    default:
        QL_FAIL("unknown fitting method");
    }

    QuantLib::ext::shared_ptr<FittedBondDiscountCurve> tmp, current;
    Real minError = QL_MAX_REAL;
    HaltonRsg halton(method->size(), 42);
    // TODO randomised optimisation seeds are only implemented for NelsonSiegel so far
    Size trials = 1;
    if (interpolationMethod_ == InterpolationMethod::NelsonSiegel) {
        trials = curveConfig_->bootstrapConfig().maxAttempts();
    } else {
        if (curveConfig_->bootstrapConfig().maxAttempts() > 1) {
            WLOG("randomised optimisation seeds not implemented for given interpolation method");
        }
    }
    for (Size i = 0; i < trials; ++i) {
        Array guess;
        // first guess is the default guess (empty array, will be set to a zero vector in
        // FittedBondDiscountCurve::calculate())
        if (i == 0) {
            if (interpolationMethod_ == InterpolationMethod::NelsonSiegel) {
                // first guess will be based on the last bond yield and first bond yield
                guess = Array(4);
                Integer maxMaturity = static_cast<Integer>(
                    std::distance(securityMaturityDates.begin(),
                                  std::max_element(securityMaturityDates.begin(), securityMaturityDates.end())));
                Integer minMaturity = static_cast<Integer>(
                    std::distance(securityMaturityDates.begin(),
                                  std::min_element(securityMaturityDates.begin(), securityMaturityDates.end())));
                guess[0] = marketYields[maxMaturity];            // long term yield
                guess[1] = marketYields[minMaturity] - guess[0]; // short term component
                guess[2] = 0.0;
                guess[3] = 5.0;
                DLOG("using smart NelsonSiegel guess for trial #" << (i + 1) << ": " << guess);
            }
        } else {
            auto seq = halton.nextSequence();
            guess = Array(seq.value.begin(), seq.value.end());
            if (interpolationMethod_ == InterpolationMethod::NelsonSiegel) {
                guess[0] = guess[0] * 0.10 - 0.05; // long term yield
                guess[1] = guess[1] * 0.10 - 0.05; // short term component
                guess[2] = guess[2] * 0.10 - 0.05; // medium term component
                guess[3] = guess[3] * 5.0;         // decay factor
                DLOG("using random NelsonSiegel guess for trial #" << (i + 1) << ": " << guess);
            } else {
                QL_FAIL("randomised optimisation seed not implemented");
            }
        }
        current = QuantLib::ext::make_shared<FittedBondDiscountCurve>(asofDate_, helpers, zeroDayCounter_, *method, 1.0e-10,
                                                              10000, guess);
        Real cost = std::sqrt(current->fitResults().minimumCostValue());
        if (cost < minError) {
            minError = cost;
            tmp = current;
        }
        DLOG("calibration trial #" << (i + 1) << " out of " << trials << ": cost = " << cost
                                   << ", best so far = " << minError);
        if (cost < curveConfig_->bootstrapConfig().accuracy()) {
            DLOG("reached desired accuracy (" << curveConfig_->bootstrapConfig().accuracy()
                                              << ") - do not attempt more calibrations");
            break;
        }
    }
    QL_REQUIRE(tmp, "no best solution found for fitted bond curve - this is unexpected.");

    if (Norm2(tmp->fitResults().solution()) < 1.0e-4) {
        WLOG("Fit solution is close to 0. The curve fitting should be reviewed.");
    }

    DLOG("Fitted Bond Curve Summary:");
    DLOG("   solution:   " << tmp->fitResults().solution());
    DLOG("   iterations: " << tmp->fitResults().numberOfIterations());
    DLOG("   cost value: " << minError);

    std::vector<Real> modelPrices, modelYields;
    auto engine = QuantLib::ext::make_shared<DiscountingBondEngine>(Handle<YieldTermStructure>(tmp));
    for (Size i = 0; i < bonds.size(); ++i) {
        bonds[i]->setPricingEngine(engine);
        modelPrices.push_back(bonds[i]->cleanPrice() / 100.0);
        modelYields.push_back(bonds[i]->yield(bonds[i]->cleanPrice(), ActualActual(ActualActual::ISDA), Continuous, NoFrequency));
        DLOG("bond " << securityIDs[i] << ", model clean price = " << modelPrices.back()
                     << ", yield (cont,actact) = " << modelYields.back() << ", NPV = " << bonds[i]->NPV());
    }

    Real tolerance = curveConfig_->bootstrapConfig().globalAccuracy() == Null<Real>()
                         ? curveConfig_->bootstrapConfig().accuracy()
                         : curveConfig_->bootstrapConfig().globalAccuracy();
    QL_REQUIRE(curveConfig_->bootstrapConfig().dontThrow() || minError < tolerance,
               "Fitted Bond Curve cost value (" << minError << ") exceeds tolerance (" << tolerance << ")");

    if (extrapolation_)
        tmp->enableExtrapolation();

    p_ = tmp;

    Array solution = tmp->fitResults().solution();

    if (buildCalibrationInfo_) {
        calInfo->modelPrices = modelPrices;
        calInfo->modelYields = modelYields;
        calInfo->tolerance = tolerance;
        calInfo->costValue = minError;
        calInfo->solution = std::vector<double>(solution.begin(), solution.end());
        calInfo->iterations = static_cast<int>(tmp->fitResults().numberOfIterations());
        calibrationInfo_ = calInfo;
    }
}

void YieldCurve::buildBondYieldShiftedCurve() {
    QL_REQUIRE(curveSegments_.size() == 1,
               "One segment required for bond yield shifted curve, got " << curveSegments_.size());
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::BondYieldShifted,
               "The curve segment is not of type Bond Yield Shifted.");
    auto segment = QuantLib::ext::dynamic_pointer_cast<BondYieldShiftedYieldCurveSegment>(curveSegments_[0]);
    QL_REQUIRE(segment != nullptr, "expected BondYieldShiftedYieldCurveSegment, this is unexpected");
    auto it = requiredYieldCurves_.find(yieldCurveKey(currency_, segment->referenceCurveID(), asofDate_));
    QL_REQUIRE(it != requiredYieldCurves_.end(), "Could not find reference curve: " << segment->referenceCurveID());

    //prepare parameters
    auto quoteIDs = segment->quotes();
    QuantLib::ext::shared_ptr<QuantLib::Bond> bond;
    string securityID;
    Date securityMaturityDate;
    Rate bondYield = Null<Real>();
    Real thisDuration = Null<Real>();

    auto engineData = QuantLib::ext::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";
    engineData->engineParameters("Bond") = {{"TimestepPeriod", "3M"}};

    //  needed to link the ibors in case bond is a floater
    std::map<std::string, Handle<YieldTermStructure>> iborCurveMapping;
    for (auto const& c : segment->iborIndexCurves()) {
        auto index = parseIborIndex(c.first);
        auto key = yieldCurveKey(index->currency(), c.second, asofDate_);
        auto y = requiredYieldCurves_.find(key);
        QL_REQUIRE(y != requiredYieldCurves_.end(), "required ibor curve '" << key << "' for iborIndex '" << c.first
                                                                             << "' not provided");
        iborCurveMapping[c.first] = y->second->handle();
    }

    auto engineFactory = QuantLib::ext::make_shared<EngineFactory>(engineData,
                                                            QuantLib::ext::make_shared<FittedBondCurveHelperMarket>(iborCurveMapping),
                                                            std::map<MarketContext, string>(),
                                                            referenceData_,
                                                            iborFallbackConfig_);

    QL_REQUIRE(quoteIDs.size() > 0, "at least one bond for shifting of the reference curve required.");

    std::vector<Real> bondYields, bondDurations;

    for (Size b = 0; b < quoteIDs.size(); ++b) {

        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(quoteIDs[b], asofDate_);

        QL_REQUIRE(marketQuote != nullptr,"no quotes for the bond " << quoteIDs[b].first << " found. this is unexpected");

        QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BOND && marketQuote->quoteType() == MarketDatum::QuoteType::PRICE,
                "Market quote not of type Bond / Price.");
        QuantLib::ext::shared_ptr<BondPriceQuote> bondQuote = QuantLib::ext::dynamic_pointer_cast<BondPriceQuote>(marketQuote);
        QL_REQUIRE(bondQuote, "market quote has type bond quote, but can not be casted, this is unexpected.");
        auto m = [bondQuote](Real x) { return x * 100.0; };
        Handle<Quote> rescaledBondQuote(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(bondQuote->quote(), m));

        securityID = bondQuote->securityID();
        QL_REQUIRE(referenceData_ != nullptr, "reference data required to build bond yield shifted curve");

        auto res = BondFactory::instance().build(engineFactory, referenceData_, securityID);
        auto qlInstr = res.bond;
        auto inflationQuoteFactor = res.inflationFactor();

        // skip bonds with settlement date <= curve reference date or which are otherwise non-tradeable
        if (qlInstr->settlementDate() > asofDate_ && QuantLib::BondFunctions::isTradable(*qlInstr)) {

            Date thisMaturity = qlInstr->maturityDate();
            bondYield = qlInstr->yield(rescaledBondQuote->value() * inflationQuoteFactor,
                                                      ActualActual(ActualActual::ISDA), Continuous, NoFrequency);

            thisDuration = QuantLib::BondFunctions::duration(*qlInstr,InterestRate(bondYield,ActualActual(ActualActual::ISDA),Continuous,NoFrequency)
                                                                           ,Duration::Modified,asofDate_);

            DLOG("found bond " << securityID << ", maturity = " << QuantLib::io::iso_date(thisMaturity)
                << ", clean price = " << rescaledBondQuote->value() * inflationQuoteFactor
                << ", yield (cont,act/act) = " << bondYield
                << ", duration = " << thisDuration);

            bondYields.push_back(bondYield);
            bondDurations.push_back(thisDuration);

            DLOG("calculated duration of the bond " << securityID << " is equal to " << thisDuration)

        } else {

            DLOG("Skipping bond " << securityID
                << ", with settlement date " << QuantLib::io::iso_date(qlInstr->settlementDate())
                << ", isTradable = " << std::boolalpha << QuantLib::BondFunctions::isTradable(*qlInstr));
        }

    }

    p_ = QuantLib::ext::make_shared<BondYieldShiftedCurveTermStructure>(it->second->handle(), bondYields, bondDurations);

}

void YieldCurve::addDeposits(const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                             vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::Deposit,
               "Conventions ID does not give deposit rate conventions.");
    QuantLib::ext::shared_ptr<DepositConvention> depositConvention = QuantLib::ext::dynamic_pointer_cast<DepositConvention>(convention);

    QuantLib::ext::shared_ptr<SimpleYieldCurveSegment> depositSegment =
        QuantLib::ext::dynamic_pointer_cast<SimpleYieldCurveSegment>(segment);
    auto depositQuoteIDs = depositSegment->quotes();

    QL_REQUIRE(segment->pillarChoice() == QuantLib::Pillar::LastRelevantDate,
               "Deposit segment does not support pillar choice " << segment->pillarChoice());
    for (Size i = 0; i < depositQuoteIDs.size(); i++) {
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(depositQuoteIDs[i], asofDate_);

        // Check that we have a valid deposit quote
        if (marketQuote) {
            QuantLib::ext::shared_ptr<MoneyMarketQuote> depositQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::MM,
                       "Market quote not of type Deposit.");
            depositQuote = QuantLib::ext::dynamic_pointer_cast<MoneyMarketQuote>(marketQuote);

            // Create a deposit helper if we do.
            QuantLib::ext::shared_ptr<RateHelper> depositHelper;
            Period depositTerm = depositQuote->term();
            Period fwdStart = depositQuote->fwdStart();
            Natural fwdStartDays = static_cast<Natural>(fwdStart.length());
            Handle<Quote> hQuote(depositQuote->quote());

            QL_REQUIRE(fwdStart.units() == Days, "The forward start time unit for deposits "
                                                 "must be expressed in days.");

            if (depositConvention->indexBased()) {
                // indexName will have the form ccy-name so examples would be:
                // EUR-EONIA, USD-FedFunds, EUR-EURIBOR, USD-LIBOR, etc.
                string indexName = depositConvention->index();
                QuantLib::ext::shared_ptr<IborIndex> index;
                if (isOvernightIndex(indexName)) {
                    // No need for the term here
                    index = parseIborIndex(indexName);
                } else {
                    // Note that a depositTerm with units Days here could end up as a string with another unit
                    // For example:
                    // 7 * Days will go to ccy-tenor-1W - CNY IR index is a case
                    // 28 * Days will go to ccy-tenor-4W - MXN TIIE is a case
                    // parseIborIndex should be able to handle this for appropriate depositTerm values
                    stringstream ss;
                    ss << indexName << "-" << io::short_period(depositTerm);
                    indexName = ss.str();
                    index = parseIborIndex(indexName);
                }
                depositHelper = QuantLib::ext::make_shared<DepositRateHelper>(
                    hQuote, depositTerm, fwdStartDays, index->fixingCalendar(), index->businessDayConvention(),
                    index->endOfMonth(), index->dayCounter());
            } else {
                depositHelper = QuantLib::ext::make_shared<DepositRateHelper>(
                    hQuote, depositTerm, fwdStartDays, depositConvention->calendar(), depositConvention->convention(),
                    depositConvention->eom(), depositConvention->dayCounter());
            }
            instruments.push_back(depositHelper);
        }
    }
}

void YieldCurve::addFutures(const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                            vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::Future,
               "Conventions ID does not give deposit rate conventions.");
    QuantLib::ext::shared_ptr<FutureConvention> futureConvention = QuantLib::ext::dynamic_pointer_cast<FutureConvention>(convention);

    QuantLib::ext::shared_ptr<SimpleYieldCurveSegment> futureSegment =
        QuantLib::ext::dynamic_pointer_cast<SimpleYieldCurveSegment>(segment);
    auto futureQuoteIDs = futureSegment->quotes();

    QL_REQUIRE(segment->pillarChoice() == QuantLib::Pillar::LastRelevantDate,
               "Future segment does not support pillar choice " << segment->pillarChoice());
    for (Size i = 0; i < futureQuoteIDs.size(); i++) {
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(futureQuoteIDs[i], asofDate_);

        // Check that we have a valid future quote
        if (marketQuote) {

            if (auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(futureConvention->index())) {
                // Overnight Index Future
                QuantLib::ext::shared_ptr<OIFutureQuote> futureQuote;
                QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::OI_FUTURE,
                           "Market quote not of type Overnight Index Future.");
                futureQuote = QuantLib::ext::dynamic_pointer_cast<OIFutureQuote>(marketQuote);

                // check that the tenor of the quote is expressed in months or years, otherwise the date calculations
                // below do not make sense
                QL_REQUIRE(futureQuote->tenor().units() == Months || futureQuote->tenor().units() == Years,
                           "Tenor of future quote (" << futureQuote->name()
                                                     << ") must be expressed in months or years");

                // Create a Overnight index future helper
                Date startDate, endDate;
                if (futureConvention->dateGenerationRule() == FutureConvention::DateGenerationRule::IMM) {
                    Date refEnd = Date(1, futureQuote->expiryMonth(), futureQuote->expiryYear());
                    Date refStart = refEnd - futureQuote->tenor();
                    startDate = IMM::nextDate(refStart, false);
                    endDate = IMM::nextDate(refEnd, false);
                } else if (futureConvention->dateGenerationRule() ==
                           FutureConvention::DateGenerationRule::FirstDayOfMonth) {
                    endDate = Date(1, futureQuote->expiryMonth(), futureQuote->expiryYear()) + 1 * Months;
                    startDate = endDate - futureQuote->tenor();
                }

                if (endDate <= asofDate_) {
                    DLOG("Skipping the " << io::ordinal(i + 1) << " overnight index future instrument because its "
                                         << "end date, " << io::iso_date(endDate)
                                         << ", is on or before the valuation date, " << io::iso_date(asofDate_) << ".");
                    continue;
                }

                QuantLib::ext::shared_ptr<RateHelper> futureHelper = QuantLib::ext::make_shared<OvernightIndexFutureRateHelper>(
                    futureQuote->quote(), startDate, endDate, on, Handle<Quote>(),
                    futureConvention->overnightIndexFutureNettingType());
                instruments.push_back(futureHelper);

                TLOG("adding OI future helper: price=" << futureQuote->quote()->value() << " start=" << startDate
                                                       << " end=" << endDate << " nettingType="
                                                       << futureConvention->overnightIndexFutureNettingType());

            } else {
                // MM Future
                QuantLib::ext::shared_ptr<MMFutureQuote> futureQuote;
                QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::MM_FUTURE,
                           "Market quote not of type Money Market Future.");
                futureQuote = QuantLib::ext::dynamic_pointer_cast<MMFutureQuote>(marketQuote);

                // Create a MM future helper
                QL_REQUIRE(
                    futureConvention->dateGenerationRule() == FutureConvention::DateGenerationRule::IMM,
                    "For MM Futures only 'IMM' is allowed as the date generation rule, check the future convention '"
                        << segment->conventionsID() << "'");
                Date refDate(1, futureQuote->expiryMonth(), futureQuote->expiryYear());
                Date immDate = IMM::nextDate(refDate, false);

                if (immDate < asofDate_) {
                    DLOG("Skipping the " << io::ordinal(i + 1) << " money market future instrument because its "
                                         << "start date, " << io::iso_date(immDate)
                                         << ", is before the valuation date, " << io::iso_date(asofDate_) << ".");
                    continue;
                }

                QuantLib::ext::shared_ptr<RateHelper> futureHelper =
                    QuantLib::ext::make_shared<FuturesRateHelper>(futureQuote->quote(), immDate, futureConvention->index());

                instruments.push_back(futureHelper);
            }
        }
    }
}

void YieldCurve::addFras(const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                         vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::FRA, "Conventions ID does not give FRA conventions.");
    QuantLib::ext::shared_ptr<FraConvention> fraConvention = QuantLib::ext::dynamic_pointer_cast<FraConvention>(convention);

    QuantLib::ext::shared_ptr<SimpleYieldCurveSegment> fraSegment =
        QuantLib::ext::dynamic_pointer_cast<SimpleYieldCurveSegment>(segment);
    auto fraQuoteIDs = fraSegment->quotes();

    for (Size i = 0; i < fraQuoteIDs.size(); i++) {
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(fraQuoteIDs[i], asofDate_);

        // Check that we have a valid FRA quote
        if (marketQuote) {
            QL_REQUIRE((marketQuote->instrumentType() == MarketDatum::InstrumentType::FRA) ||
                           (marketQuote->instrumentType() == MarketDatum::InstrumentType::IMM_FRA),
                       "Market quote not of type FRA.");

            // Create a FRA helper if we do.

            QuantLib::ext::shared_ptr<RateHelper> fraHelper;

            if (marketQuote->instrumentType() == MarketDatum::InstrumentType::IMM_FRA) {
                QuantLib::ext::shared_ptr<ImmFraQuote> immFraQuote;
                immFraQuote = QuantLib::ext::dynamic_pointer_cast<ImmFraQuote>(marketQuote);
                Size imm1 = immFraQuote->imm1();
                Size imm2 = immFraQuote->imm2();
                fraHelper = QuantLib::ext::make_shared<ImmFraRateHelper>(immFraQuote->quote(), imm1, imm2,
                                                                 fraConvention->index(), fraSegment->pillarChoice());
            } else if (marketQuote->instrumentType() == MarketDatum::InstrumentType::FRA) {
                QuantLib::ext::shared_ptr<FRAQuote> fraQuote;
                fraQuote = QuantLib::ext::dynamic_pointer_cast<FRAQuote>(marketQuote);
                Period periodToStart = fraQuote->fwdStart();
                fraHelper = QuantLib::ext::make_shared<FraRateHelper>(fraQuote->quote(), periodToStart, fraConvention->index(),
                                                              fraSegment->pillarChoice());
            } else {
                QL_FAIL("Market quote not of type FRA.");
            }

            instruments.push_back(fraHelper);
        }
    }
}

void YieldCurve::addOISs(const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                         vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::OIS, "Conventions ID does not give OIS conventions.");
    QuantLib::ext::shared_ptr<OisConvention> oisConvention = QuantLib::ext::dynamic_pointer_cast<OisConvention>(convention);

    QuantLib::ext::shared_ptr<SimpleYieldCurveSegment> oisSegment =
        QuantLib::ext::dynamic_pointer_cast<SimpleYieldCurveSegment>(segment);

    // If projection curve ID is not the this curve.
    string projectionCurveID = oisSegment->projectionCurveID();
    QuantLib::ext::shared_ptr<OvernightIndex> onIndex = oisConvention->index();
    if (projectionCurveID != curveConfig_->curveID() && !projectionCurveID.empty()) {
        projectionCurveID = yieldCurveKey(currency_, projectionCurveID, asofDate_);
        QuantLib::ext::shared_ptr<YieldCurve> projectionCurve;
        map<string, QuantLib::ext::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(projectionCurveID);
        if (it != requiredYieldCurves_.end()) {
            projectionCurve = it->second;
        } else {
            QL_FAIL("The projection curve, " << projectionCurveID
                                             << ", required in the building "
                                                "of the curve, "
                                             << curveSpec_.name() << ", was not found.");
        }
        onIndex = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(onIndex->clone(projectionCurve->handle()));
    }

    // BRL CDI overnight needs a specialised rate helper so we use this below to switch
    QuantLib::ext::shared_ptr<BRLCdi> brlCdiIndex = QuantLib::ext::dynamic_pointer_cast<BRLCdi>(onIndex);

    auto oisQuoteIDs = oisSegment->quotes();
    for (Size i = 0; i < oisQuoteIDs.size(); i++) {
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(oisQuoteIDs[i], asofDate_);

        // Check that we have a valid OIS quote
        if (marketQuote) {
            QuantLib::ext::shared_ptr<SwapQuote> oisQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::IR_SWAP,
                       "Market quote (" << marketQuote->name() << ") not of type swap.");
            oisQuote = QuantLib::ext::dynamic_pointer_cast<SwapQuote>(marketQuote);

            // Create a swap helper if we do.
            Period oisTenor = oisQuote->term();
            QuantLib::ext::shared_ptr<RateHelper> oisHelper;
            if (brlCdiIndex) {
                QL_REQUIRE(segment->pillarChoice() == QuantLib::Pillar::LastRelevantDate,
                           "OIS segment for BRL-CDI does not support pillar choice " << segment->pillarChoice());
                oisHelper = QuantLib::ext::make_shared<BRLCdiRateHelper>(
                    oisTenor, oisQuote->quote(), brlCdiIndex,
                    discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>(), true);
            } else {
                
                if (oisQuote->startDate() == Null<Date>() || oisQuote->maturityDate() == Null<Date>())
                    oisHelper = QuantLib::ext::make_shared<QuantExt::OISRateHelper>(
                        oisConvention->spotLag(), oisTenor, oisQuote->quote(), onIndex, oisConvention->fixedDayCounter(),
                        oisConvention->fixedCalendar(), oisConvention->paymentLag(), oisConvention->eom(),
                        oisConvention->fixedFrequency(), oisConvention->fixedConvention(),
                        oisConvention->fixedPaymentConvention(), oisConvention->rule(),
                        discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>(), true,
                        oisSegment->pillarChoice());
                else {
                    oisHelper = QuantLib::ext::make_shared<QuantExt::DatedOISRateHelper>(oisQuote->startDate(),
                        oisQuote->maturityDate(), oisQuote->quote(), onIndex, oisConvention->fixedDayCounter(),
                        oisConvention->fixedCalendar(), oisConvention->paymentLag(),
                        oisConvention->fixedFrequency(), oisConvention->fixedConvention(),
                        oisConvention->fixedPaymentConvention(), oisConvention->rule(),
                        discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>(), true,
                        oisSegment->pillarChoice());
                }
            }
            instruments.push_back(oisHelper);
        }
    }
}

void YieldCurve::addSwaps(const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                          vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::Swap, "Conventions ID does not give swap conventions.");
    QuantLib::ext::shared_ptr<IRSwapConvention> swapConvention = QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(convention);

    QuantLib::ext::shared_ptr<SimpleYieldCurveSegment> swapSegment =
        QuantLib::ext::dynamic_pointer_cast<SimpleYieldCurveSegment>(segment);
    if (swapSegment->projectionCurveID() != curveConfig_->curveID() && !swapSegment->projectionCurveID().empty()) {
        QL_FAIL("Solving for discount curve given the projection"
                " curve is not implemented yet");
    }
    auto swapQuoteIDs = swapSegment->quotes();

    for (Size i = 0; i < swapQuoteIDs.size(); i++) {
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(swapQuoteIDs[i], asofDate_);

        // Check that we have a valid swap quote
        if (marketQuote) {
            QuantLib::ext::shared_ptr<SwapQuote> swapQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::IR_SWAP,
                       "Market quote not of type swap.");
            swapQuote = QuantLib::ext::dynamic_pointer_cast<SwapQuote>(marketQuote);
            QL_REQUIRE(swapQuote->startDate() == Null<Date>(),
                       "swap quote with fixed start date is not supported for ibor / subperiods swap instruments");
            // Create a swap helper if we do.
            Period swapTenor = swapQuote->term();
            QuantLib::ext::shared_ptr<RateHelper> swapHelper;
            if (swapConvention->hasSubPeriod()) {
                QL_REQUIRE(segment->pillarChoice() == QuantLib::Pillar::LastRelevantDate,
                           "Subperiod Swap segment does not support pillar choice " << segment->pillarChoice());
                swapHelper = QuantLib::ext::make_shared<SubPeriodsSwapHelper>(
                    swapQuote->quote(), swapTenor, Period(swapConvention->fixedFrequency()),
                    swapConvention->fixedCalendar(), swapConvention->fixedDayCounter(),
                    swapConvention->fixedConvention(), Period(swapConvention->floatFrequency()),
                    swapConvention->index(), swapConvention->index()->dayCounter(),
                    discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>(),
                    swapConvention->subPeriodsCouponType());
            } else {
                swapHelper = QuantLib::ext::make_shared<SwapRateHelper>(
                    swapQuote->quote(), swapTenor, swapConvention->fixedCalendar(), swapConvention->fixedFrequency(),
                    swapConvention->fixedConvention(), swapConvention->fixedDayCounter(), swapConvention->index(),
                    Handle<Quote>(), 0 * Days, discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>(),
                    Null<Natural>(), swapSegment->pillarChoice());
            }

            instruments.push_back(swapHelper);
        }
    }
}

void YieldCurve::addAverageOISs(const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                                vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::AverageOIS,
               "Conventions ID does not give average OIS conventions.");
    QuantLib::ext::shared_ptr<AverageOisConvention> averageOisConvention =
        QuantLib::ext::dynamic_pointer_cast<AverageOisConvention>(convention);

    QuantLib::ext::shared_ptr<AverageOISYieldCurveSegment> averageOisSegment =
        QuantLib::ext::dynamic_pointer_cast<AverageOISYieldCurveSegment>(segment);

    // If projection curve ID is not the this curve.
    string projectionCurveID = averageOisSegment->projectionCurveID();
    QuantLib::ext::shared_ptr<OvernightIndex> onIndex = averageOisConvention->index();
    if (projectionCurveID != curveConfig_->curveID() && !projectionCurveID.empty()) {
        projectionCurveID = yieldCurveKey(currency_, projectionCurveID, asofDate_);
        QuantLib::ext::shared_ptr<YieldCurve> projectionCurve;
        map<string, QuantLib::ext::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(projectionCurveID);
        if (it != requiredYieldCurves_.end()) {
            projectionCurve = it->second;
        } else {
            QL_FAIL("The projection curve, " << projectionCurveID
                                             << ", required in the building "
                                                "of the curve, "
                                             << curveSpec_.name() << ", was not found.");
        }
        onIndex = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(onIndex->clone(projectionCurve->handle()));
    }

    QL_REQUIRE(segment->pillarChoice() == QuantLib::Pillar::LastRelevantDate,
               "Average OIS segment does not support pillar choice " << segment->pillarChoice());
    auto averageOisQuoteIDs = averageOisSegment->quotes();
    for (Size i = 0; i < averageOisQuoteIDs.size(); i += 2) {
        // we are assuming i = spread, i+1 = rate
        QL_REQUIRE(i % 2 == 0, "i is not even");
        /* An average OIS quote is a composite of a swap quote and a basis
           spread quote. Check first that we have these. */
        // Firstly, the rate quote.
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(averageOisQuoteIDs[i], asofDate_);
        QuantLib::ext::shared_ptr<SwapQuote> swapQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::IR_SWAP,
                       "Market quote not of type swap.");
            swapQuote = QuantLib::ext::dynamic_pointer_cast<SwapQuote>(marketQuote);
            QL_REQUIRE(swapQuote->startDate() == Null<Date>(),
                       "swap quote with fixed start date is not supported for average ois instrument");

            // Secondly, the basis spread quote.
            marketQuote = loader_.get(averageOisQuoteIDs[i + 1], asofDate_);
            QuantLib::ext::shared_ptr<BasisSwapQuote> basisQuote;
            if (marketQuote) {
                QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BASIS_SWAP,
                           "Market quote not of type basis swap.");
                basisQuote = QuantLib::ext::dynamic_pointer_cast<BasisSwapQuote>(marketQuote);

                // Create an average OIS helper if we do.
                Period AverageOisTenor = swapQuote->term();
                QL_REQUIRE(AverageOisTenor == basisQuote->maturity(),
                           "The swap "
                           "and basis swap components of the Average OIS must "
                           "have the same maturity.");
                QuantLib::ext::shared_ptr<RateHelper> averageOisHelper(new QuantExt::AverageOISRateHelper(
                    swapQuote->quote(), averageOisConvention->spotLag() * Days, AverageOisTenor,
                    averageOisConvention->fixedTenor(), averageOisConvention->fixedDayCounter(),
                    averageOisConvention->fixedCalendar(), averageOisConvention->fixedConvention(),
                    averageOisConvention->fixedPaymentConvention(), onIndex, averageOisConvention->onTenor(),
                    basisQuote->quote(), averageOisConvention->rateCutoff(),
                    discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>(), true));

                instruments.push_back(averageOisHelper);
            }
        }
    }
}

void YieldCurve::addTenorBasisSwaps(const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                                    vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::TenorBasisSwap,
               "Conventions ID does not give tenor basis swap conventions.");
    QuantLib::ext::shared_ptr<TenorBasisSwapConvention> basisSwapConvention =
        QuantLib::ext::dynamic_pointer_cast<TenorBasisSwapConvention>(convention);

    QuantLib::ext::shared_ptr<TenorBasisYieldCurveSegment> basisSwapSegment =
        QuantLib::ext::dynamic_pointer_cast<TenorBasisYieldCurveSegment>(segment);

    // If short index projection curve ID is not this curve.
    string receiveCurveID = basisSwapSegment->receiveProjectionCurveID();
    QuantLib::ext::shared_ptr<IborIndex> receiveIndex = basisSwapConvention->receiveIndex();
    if (receiveCurveID != curveConfig_->curveID() && !receiveCurveID.empty()) {
        receiveCurveID = yieldCurveKey(currency_, receiveCurveID, asofDate_);
        QuantLib::ext::shared_ptr<YieldCurve> shortCurve;
        map<string, QuantLib::ext::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(receiveCurveID);
        if (it != requiredYieldCurves_.end()) {
            shortCurve = it->second;
        } else {
            QL_FAIL("The short side projection curve, " << receiveCurveID
                                                        << ", required in the building "
                                                           "of the curve, "
                                                        << curveSpec_.name() << ", was not found.");
        }
        receiveIndex = receiveIndex->clone(shortCurve->handle());
    }

    // If long index projection curve ID is not this curve.
    string payCurveID = basisSwapSegment->payProjectionCurveID();
    QuantLib::ext::shared_ptr<IborIndex> payIndex = basisSwapConvention->payIndex();
    if (payCurveID != curveConfig_->curveID() && !payCurveID.empty()) {
        payCurveID = yieldCurveKey(currency_, payCurveID, asofDate_);
        QuantLib::ext::shared_ptr<YieldCurve> longCurve;
        map<string, QuantLib::ext::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(payCurveID);
        if (it != requiredYieldCurves_.end()) {
            longCurve = it->second;
        } else {
            QL_FAIL("The long side projection curve, " << payCurveID
                                                       << ", required in the building "
                                                          "of the curve, "
                                                       << curveSpec_.name() << ", was not found.");
        }
        payIndex = payIndex->clone(longCurve->handle());
    }

    QL_REQUIRE(segment->pillarChoice() == QuantLib::Pillar::LastRelevantDate,
               "Tenor basis swap segment does not support pillar choice " << segment->pillarChoice());
    auto basisSwapQuoteIDs = basisSwapSegment->quotes();
    for (Size i = 0; i < basisSwapQuoteIDs.size(); i++) {
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(basisSwapQuoteIDs[i], asofDate_);

        // Check that we have a valid basis swap quote
        if (marketQuote) {
            QuantLib::ext::shared_ptr<BasisSwapQuote> basisSwapQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BASIS_SWAP,
                       "Market quote not of type basis swap.");
            basisSwapQuote = QuantLib::ext::dynamic_pointer_cast<BasisSwapQuote>(marketQuote);

            // Create a tenor basis swap helper if we do.
            Period basisSwapTenor = basisSwapQuote->maturity();
            QuantLib::ext::shared_ptr<RateHelper> basisSwapHelper;
            bool telescopicValueDates = true;
            basisSwapHelper.reset(
                new TenorBasisSwapHelper(basisSwapQuote->quote(), basisSwapTenor, payIndex, receiveIndex,
                                         discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>(),
                                         basisSwapConvention->spreadOnRec(), basisSwapConvention->includeSpread(),
                                         basisSwapConvention->payFrequency(), basisSwapConvention->receiveFrequency(),
                                         telescopicValueDates, basisSwapConvention->subPeriodsCouponType()));

            instruments.push_back(basisSwapHelper);
        }
    }
}

void YieldCurve::addTenorBasisTwoSwaps(const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                                       vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::TenorBasisTwoSwap,
               "Conventions ID does not give tenor basis two swap conventions.");
    QuantLib::ext::shared_ptr<TenorBasisTwoSwapConvention> basisSwapConvention =
        QuantLib::ext::dynamic_pointer_cast<TenorBasisTwoSwapConvention>(convention);

    QuantLib::ext::shared_ptr<TenorBasisYieldCurveSegment> basisSwapSegment =
        QuantLib::ext::dynamic_pointer_cast<TenorBasisYieldCurveSegment>(segment);

    // If short index projection curve ID is not this curve.
    string shortCurveID = basisSwapSegment->receiveProjectionCurveID();
    QuantLib::ext::shared_ptr<IborIndex> shortIndex = basisSwapConvention->shortIndex();
    if (shortCurveID != curveConfig_->curveID() && !shortCurveID.empty()) {
        shortCurveID = yieldCurveKey(currency_, shortCurveID, asofDate_);
        QuantLib::ext::shared_ptr<YieldCurve> shortCurve;
        map<string, QuantLib::ext::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(shortCurveID);
        if (it != requiredYieldCurves_.end()) {
            shortCurve = it->second;
        } else {
            QL_FAIL("The short side projection curve, " << shortCurveID
                                                        << ", required in the building "
                                                           "of the curve, "
                                                        << curveSpec_.name() << ", was not found.");
        }
        shortIndex = shortIndex->clone(shortCurve->handle());
    }

    // If long index projection curve ID is not this curve.
    string longCurveID = basisSwapSegment->payProjectionCurveID();
    QuantLib::ext::shared_ptr<IborIndex> longIndex = basisSwapConvention->longIndex();
    if (longCurveID != curveConfig_->curveID() && !longCurveID.empty()) {
        longCurveID = yieldCurveKey(currency_, longCurveID, asofDate_);
        QuantLib::ext::shared_ptr<YieldCurve> longCurve;
        map<string, QuantLib::ext::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(longCurveID);
        if (it != requiredYieldCurves_.end()) {
            longCurve = it->second;
        } else {
            QL_FAIL("The projection curve, " << longCurveID
                                             << ", required in the building "
                                                "of the curve, "
                                             << curveSpec_.name() << ", was not found.");
        }
        longIndex = longIndex->clone(longCurve->handle());
    }

    QL_REQUIRE(segment->pillarChoice() == QuantLib::Pillar::LastRelevantDate,
               "Tenor basis two swap segment does not support pillar choice " << segment->pillarChoice());
    auto basisSwapQuoteIDs = basisSwapSegment->quotes();
    for (Size i = 0; i < basisSwapQuoteIDs.size(); i++) {
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(basisSwapQuoteIDs[i], asofDate_);

        // Check that we have a valid basis swap quote
        QuantLib::ext::shared_ptr<BasisSwapQuote> basisSwapQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BASIS_SWAP,
                       "Market quote not of type basis swap.");
            basisSwapQuote = QuantLib::ext::dynamic_pointer_cast<BasisSwapQuote>(marketQuote);

            // Create a tenor basis swap helper if we do.
            Period basisSwapTenor = basisSwapQuote->maturity();
            QuantLib::ext::shared_ptr<RateHelper> basisSwapHelper(new BasisTwoSwapHelper(
                basisSwapQuote->quote(), basisSwapTenor, basisSwapConvention->calendar(),
                basisSwapConvention->longFixedFrequency(), basisSwapConvention->longFixedConvention(),
                basisSwapConvention->longFixedDayCounter(), longIndex, basisSwapConvention->shortFixedFrequency(),
                basisSwapConvention->shortFixedConvention(), basisSwapConvention->shortFixedDayCounter(), shortIndex,
                basisSwapConvention->longMinusShort(),
                discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>()));

            instruments.push_back(basisSwapHelper);
        }
    }
}

void YieldCurve::addBMABasisSwaps(const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                                  vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::BMABasisSwap,
               "Conventions ID does not give bma basis swap conventions.");
    QuantLib::ext::shared_ptr<BMABasisSwapConvention> bmaBasisSwapConvention =
        QuantLib::ext::dynamic_pointer_cast<BMABasisSwapConvention>(convention);

    QuantLib::ext::shared_ptr<SimpleYieldCurveSegment> bmaBasisSwapSegment =
        QuantLib::ext::dynamic_pointer_cast<SimpleYieldCurveSegment>(segment);
    QL_REQUIRE(bmaBasisSwapSegment, "BMA basis swap segment of " + curveSpec_.ccy() + "/" + curveSpec_.curveConfigID() +
                                        " did not successfully cast to a BMA basis swap yield curve segment!");

    // TODO: should be checking here whether or not the bma index is forwarding on this curve. either way, we make sure!
    QuantLib::ext::shared_ptr<BMAIndexWrapper> bmaIndex = bmaBasisSwapConvention->bmaIndex();
    bmaIndex = dynamic_pointer_cast<BMAIndexWrapper>(bmaIndex->clone(handle()));

    // If libor index projection curve ID is not this curve.
    string liborCurveID = bmaBasisSwapSegment->projectionCurveID();
    QuantLib::ext::shared_ptr<IborIndex> liborIndex = bmaBasisSwapConvention->liborIndex();
    liborCurveID = yieldCurveKey(currency_, liborCurveID, asofDate_);
    QuantLib::ext::shared_ptr<YieldCurve> liborCurve;
    map<string, QuantLib::ext::shared_ptr<YieldCurve>>::iterator it;
    it = requiredYieldCurves_.find(liborCurveID);
    if (it != requiredYieldCurves_.end()) {
        liborCurve = it->second;
    } else {
        QL_FAIL("The libor side projection curve, " << liborCurveID
                                                    << ", required in the building "
                                                       "of the curve, "
                                                    << curveSpec_.name() << ", was not found.");
    }
    liborIndex = liborIndex->clone(liborCurve->handle());

    QL_REQUIRE(segment->pillarChoice() == QuantLib::Pillar::LastRelevantDate,
               "BMA swap segment does not support pillar choice " << segment->pillarChoice());
    auto bmaBasisSwapQuoteIDs = bmaBasisSwapSegment->quotes();
    for (Size i = 0; i < bmaBasisSwapQuoteIDs.size(); i++) {
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(bmaBasisSwapQuoteIDs[i], asofDate_);

        // Check that we have a valid bma basis swap quote
        if (marketQuote) {
            QuantLib::ext::shared_ptr<BMASwapQuote> bmaBasisSwapQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BMA_SWAP,
                       "Market quote not of type bma swap.");
            QL_REQUIRE(marketQuote->quoteType() == MarketDatum::QuoteType::RATIO, "Market quote not of type ratio.");
            bmaBasisSwapQuote = QuantLib::ext::dynamic_pointer_cast<BMASwapQuote>(marketQuote);

            // Create bma basis swap helper if we do.
            QuantLib::ext::shared_ptr<RateHelper> bmaSwapHelper;
            bmaSwapHelper.reset(new BMASwapRateHelper(bmaBasisSwapQuote->quote(), bmaBasisSwapQuote->maturity(),
                                                      bmaIndex->fixingDays(), bmaIndex->fixingCalendar(),
                                                      bmaBasisSwapQuote->term(), bmaIndex->businessDayConvention(),
                                                      bmaIndex->dayCounter(), bmaIndex->bma(), liborIndex));
            instruments.push_back(bmaSwapHelper);
        }
    }
}

void YieldCurve::addFXForwards(const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                               vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::FX, "Conventions ID does not give FX forward conventions.");

    QuantLib::ext::shared_ptr<FXConvention> fxConvention = QuantLib::ext::dynamic_pointer_cast<FXConvention>(convention);

    QuantLib::ext::shared_ptr<CrossCcyYieldCurveSegment> fxForwardSegment =
        QuantLib::ext::dynamic_pointer_cast<CrossCcyYieldCurveSegment>(segment);

    /* Need to retrieve the discount curve in the other currency. These are called the known discount
       curve and known discount currency respectively. */
    Currency knownCurrency;
    if (currency_ == fxConvention->sourceCurrency()) {
        knownCurrency = fxConvention->targetCurrency();
    } else if (currency_ == fxConvention->targetCurrency()) {
        knownCurrency = fxConvention->sourceCurrency();
    } else {
        QL_FAIL("One of the currencies in the FX forward bootstrap "
                "instruments needs to match the yield curve currency.");
    }

    string knownDiscountID = fxForwardSegment->foreignDiscountCurveID();
    Handle<YieldTermStructure> knownDiscountCurve;

    if (!knownDiscountID.empty()) {
        knownDiscountID = yieldCurveKey(knownCurrency, knownDiscountID, asofDate_);
        auto it = requiredYieldCurves_.find(knownDiscountID);
        if (it != requiredYieldCurves_.end()) {
            knownDiscountCurve = it->second->handle();
        } else {
            QL_FAIL("The foreign discount curve, " << knownDiscountID
                << ", required in the building "
                "of the curve, "
                << curveSpec_.name() << ", was not found.");
        }
    } else {
        // fall back on the foreign discount curve if no index given
        // look up the inccy discount curve - falls back to defualt if no inccy
        DLOG("YieldCurve::addFXForwards No discount curve provided for building curve " << 
            curveSpec_.name() << ", looking up the inccy curve in the market.")
        knownDiscountCurve = market_->discountCurve(knownCurrency.code(), Market::inCcyConfiguration);
    }


    /* Need to retrieve the market FX spot rate */
    string spotRateID = fxForwardSegment->spotRateID();
    QuantLib::ext::shared_ptr<FXSpotQuote> fxSpotQuote = getFxSpotQuote(spotRateID);

    /* Create an FX spot quote from the retrieved FX spot rate */
    Currency fxSpotSourceCcy = parseCurrency(fxSpotQuote->unitCcy());
    Currency fxSpotTargetCcy = parseCurrency(fxSpotQuote->ccy());

    QL_REQUIRE(segment->pillarChoice() == QuantLib::Pillar::LastRelevantDate,
               "FX Forward segment does not support pillar choice " << segment->pillarChoice());
    DLOG("YieldCurve::addFXForwards(), create FX forward quotes and helpers");
    auto fxForwardQuoteIDs = fxForwardSegment->quotes();
    for (Size i = 0; i < fxForwardQuoteIDs.size(); i++) {
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(fxForwardQuoteIDs[i], asofDate_);

        // Check that we have a valid FX forward quote
        if (marketQuote) {
            QuantLib::ext::shared_ptr<FXForwardQuote> fxForwardQuote;
            Handle<Quote> spotFx;

            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::FX_FWD,
                       "Market quote not of type FX forward.");
            fxForwardQuote = QuantLib::ext::dynamic_pointer_cast<FXForwardQuote>(marketQuote);

            QL_REQUIRE(fxSpotQuote->unitCcy() == fxForwardQuote->unitCcy() &&
                           fxSpotQuote->ccy() == fxForwardQuote->ccy(),
                       "Currency mismatch between spot \"" << spotRateID << "\" and fwd \""
                                                           << fxForwardQuoteIDs[i].first << "\"");
                        
            // QL expects the FX Fwd quote to be per spot, not points. If the quote is an outright, handle conversion to points convention here.

            Handle<Quote> qlFXForwardQuote;
            if (fxForwardQuote->quoteType() == MarketDatum::QuoteType::PRICE) {
                auto m = [f = fxSpotQuote->quote()->value()](Real x) { return x - f; };
                qlFXForwardQuote =
                    Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(fxForwardQuote->quote(), m));
            } else {
                auto m = [p = fxConvention->pointsFactor()](Real x) { return x / p; };
                qlFXForwardQuote =
                    Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(fxForwardQuote->quote(), m));
            }

            Natural spotDays = fxConvention->spotDays();
            if (matchFxFwdStringTerm(fxForwardQuote->term(), FXForwardQuote::FxFwdString::ON)) {
                // Overnight rate is the spread over todays fx, for settlement on t+1. We need 'todays' rate in order
                // to use this to determine yield curve value at t+1.
                // If spotDays is 0 it is spread over Spot.
                // If spotDays is 1 we can subtract the ON spread from spot to get todays fx. 
                // If spotDays is 2 we also need Tomorrow next rate to get todays fx.
                // If spotDays is greater than 2 we can't use this.
                Real tnSpread = Null<Real>();
                Real totalSpread = 0.0;
                switch (spotDays) { 
                case 0:
                    spotFx = fxSpotQuote->quote();
                    break;
                case 1: {
                    // TODO: this isn't registeredWith the ON basis quote
                    auto m = [f = qlFXForwardQuote->value()](Real x) { return x - f; };
                    spotFx = Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(fxSpotQuote->quote(), m));
                    break;
                }
                case 2: {
                    // find the TN quote
                    for (auto q : loader_.loadQuotes(asofDate_)) {
                        if (q->instrumentType() == MarketDatum::InstrumentType::FX_FWD) {
                            auto fxq = QuantLib::ext::dynamic_pointer_cast<FXForwardQuote>(q);
                            if (fxq && fxSpotQuote->unitCcy() == fxq->unitCcy() && fxSpotQuote->ccy() == fxq->ccy() &&
                                matchFxFwdStringTerm(fxq->term(), FXForwardQuote::FxFwdString::TN)) {
                                tnSpread = fxq->quote()->value() / fxConvention->pointsFactor();
                                break;
                            }
                        }
                    }
                    if (tnSpread == Null<Real>()) {
                        WLOG("YieldCurve::AddFxForwards cannot use ON rate, when SpotDays are 2 we also require the TN "
                             "rate");
                        continue;
                    }
                    totalSpread = tnSpread + qlFXForwardQuote->value();
                    // TODO: this isn't registeredWith the ON or TN basis quote
                    auto m2 = [totalSpread](Real x) { return x - totalSpread; };
                    spotFx = Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m2)>>(fxSpotQuote->quote(), m2));
                    break;
                }
                default:
                    WLOG("YieldCurve::AddFxForwards cannot use ON rate, when SpotDays are " << spotDays << 
                        ", only valid for SpotDays of 0, 1 or 2.");
                    continue;
                }
            } else if (matchFxFwdStringTerm(fxForwardQuote->term(), FXForwardQuote::FxFwdString::TN)) {
                // TODO: this isn't registeredWith the TN basis quote
                auto m = [f = qlFXForwardQuote->value()](Real x) { return x - f; };
                spotFx = Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(fxSpotQuote->quote(), m));
            } else {
                spotFx = fxSpotQuote->quote();
            }

            Period fxForwardTenor = fxFwdQuoteTenor(fxForwardQuote->term());
            Period fxStartTenor = fxFwdQuoteStartTenor(fxForwardQuote->term(), fxConvention);
            bool isFxBaseCurrencyCollateralCurrency = knownCurrency == fxSpotSourceCcy;

            QuantLib::ext::shared_ptr<RateHelper> fxForwardHelper(new FxSwapRateHelper(
                qlFXForwardQuote, spotFx, fxForwardTenor, fxStartTenor.length(), fxConvention->advanceCalendar(), 
                fxConvention->convention(), fxConvention->endOfMonth(), isFxBaseCurrencyCollateralCurrency,
                knownDiscountCurve));

            instruments.push_back(fxForwardHelper);
        }
    }

    DLOG("YieldCurve::addFXForwards() done");
}

void YieldCurve::addCrossCcyBasisSwaps(const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                                       vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::CrossCcyBasis, "Conventions ID does not give cross currency "
                                                                      "basis swap conventions.");
    QuantLib::ext::shared_ptr<CrossCcyBasisSwapConvention> basisSwapConvention =
        QuantLib::ext::dynamic_pointer_cast<CrossCcyBasisSwapConvention>(convention);

    /* Is this yield curve on the flat side or spread side */
    bool onFlatSide = (currency_ == basisSwapConvention->flatIndex()->currency());

    QuantLib::ext::shared_ptr<CrossCcyYieldCurveSegment> basisSwapSegment =
        QuantLib::ext::dynamic_pointer_cast<CrossCcyYieldCurveSegment>(segment);

    /* Need to retrieve the market FX spot rate */
    string spotRateID = basisSwapSegment->spotRateID();
    QuantLib::ext::shared_ptr<FXSpotQuote> fxSpotQuote = getFxSpotQuote(spotRateID);

    /* Create an FX spot quote from the retrieved FX spot rate */
    Currency fxSpotSourceCcy = parseCurrency(fxSpotQuote->unitCcy());
    Currency fxSpotTargetCcy = parseCurrency(fxSpotQuote->ccy());

    /* Need to retrieve the discount curve in the other (foreign) currency. */
    string foreignDiscountID = basisSwapSegment->foreignDiscountCurveID();
    Currency foreignCcy = fxSpotSourceCcy == currency_ ? fxSpotTargetCcy : fxSpotSourceCcy;
    Handle<YieldTermStructure> foreignDiscountCurve;
    if (!foreignDiscountID.empty()) {
        foreignDiscountID = yieldCurveKey(foreignCcy, foreignDiscountID, asofDate_);
        auto it = requiredYieldCurves_.find(foreignDiscountID);
        if (it != requiredYieldCurves_.end()) {
            foreignDiscountCurve = it->second->handle();
        } else {
            QL_FAIL("The foreign discount curve, " << foreignDiscountID
                << ", required in the building "
                "of the curve, "
                << curveSpec_.name() << ", was not found.");
        }
    } else {
        // fall back on the foreign discount curve if no index given
        // look up the inccy discount curve - falls back to defualt if no inccy
        DLOG("YieldCurve::addCrossCcyBasisSwaps No discount curve provided for building curve "
             << curveSpec_.name() << ", looking up the inccy curve in the market.")
        foreignDiscountCurve = market_->discountCurve(foreignCcy.code(), Market::inCcyConfiguration);
    }

    /* Need to retrieve the foreign projection curve in the other currency. If its ID is empty,
       set it equal to the foreign discount curve. */
    string foreignProjectionCurveID = basisSwapSegment->foreignProjectionCurveID();
    QuantLib::ext::shared_ptr<IborIndex> foreignIndex =
        onFlatSide ? basisSwapConvention->spreadIndex() : basisSwapConvention->flatIndex();
    if (foreignProjectionCurveID.empty()) {
        foreignIndex = foreignIndex->clone(foreignDiscountCurve);
    } else {
        foreignProjectionCurveID = yieldCurveKey(foreignCcy, foreignProjectionCurveID, asofDate_);
        QuantLib::ext::shared_ptr<YieldCurve> foreignProjectionCurve;
        map<string, QuantLib::ext::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(foreignProjectionCurveID);
        if (it != requiredYieldCurves_.end()) {
            foreignProjectionCurve = it->second;
        } else {
            QL_FAIL("The foreign projection curve, " << foreignProjectionCurveID
                                                     << ", required in the building "
                                                        "of the curve, "
                                                     << curveSpec_.name() << ", was not found.");
        }
        foreignIndex = foreignIndex->clone(foreignProjectionCurve->handle());
    }

    // If domestic index projection curve ID is not this curve.
    string domesticProjectionCurveID = basisSwapSegment->domesticProjectionCurveID();
    QuantLib::ext::shared_ptr<IborIndex> domesticIndex =
        onFlatSide ? basisSwapConvention->flatIndex() : basisSwapConvention->spreadIndex();
    if (domesticProjectionCurveID != curveConfig_->curveID() && !domesticProjectionCurveID.empty()) {
        domesticProjectionCurveID = yieldCurveKey(currency_, domesticProjectionCurveID, asofDate_);
        QuantLib::ext::shared_ptr<YieldCurve> domesticProjectionCurve;
        map<string, QuantLib::ext::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(domesticProjectionCurveID);
        if (it != requiredYieldCurves_.end()) {
            domesticProjectionCurve = it->second;
        } else {
            QL_FAIL("The domestic projection curve, " << domesticProjectionCurveID
                                                      << ", required in the"
                                                         " building of the curve, "
                                                      << curveSpec_.name() << ", was not found.");
        }
        domesticIndex = domesticIndex->clone(domesticProjectionCurve->handle());
    }

    /* Arrange the discount curves and indices for use in the helper */
    RelinkableHandle<YieldTermStructure> flatDiscountCurve;
    RelinkableHandle<YieldTermStructure> spreadDiscountCurve;
    QuantLib::ext::shared_ptr<IborIndex> flatIndex;
    QuantLib::ext::shared_ptr<IborIndex> spreadIndex;
    if (onFlatSide) {
        if (discountCurve_) {
            flatDiscountCurve.linkTo(*discountCurve_->handle());
        }
        spreadDiscountCurve.linkTo(*foreignDiscountCurve);
        flatIndex = domesticIndex;
        spreadIndex = foreignIndex;
    } else {
        flatDiscountCurve.linkTo(*foreignDiscountCurve);
        if (discountCurve_) {
            spreadDiscountCurve.linkTo(*discountCurve_->handle());
        }
        flatIndex = foreignIndex;
        spreadIndex = domesticIndex;
    }

    Period flatTenor = basisSwapConvention->flatTenor();
    Period spreadTenor = basisSwapConvention->spreadTenor();

    QL_REQUIRE(segment->pillarChoice() == QuantLib::Pillar::LastRelevantDate,
               "XCcy basis segment does not support pillar choice " << segment->pillarChoice());
    auto basisSwapQuoteIDs = basisSwapSegment->quotes();
    for (Size i = 0; i < basisSwapQuoteIDs.size(); i++) {
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(basisSwapQuoteIDs[i], asofDate_);

        // Check that we have a valid basis swap quote
        if (marketQuote) {
            QuantLib::ext::shared_ptr<CrossCcyBasisSwapQuote> basisSwapQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::CC_BASIS_SWAP,
                       "Market quote not of type cross "
                       "currency basis swap.");
            basisSwapQuote = QuantLib::ext::dynamic_pointer_cast<CrossCcyBasisSwapQuote>(marketQuote);

            // Create a cross currency basis swap helper if we do.
            Period basisSwapTenor = basisSwapQuote->maturity();
            bool isResettableSwap = basisSwapConvention->isResettable();
            if (!isResettableSwap) {
                instruments.push_back(QuantLib::ext::make_shared<CrossCcyBasisSwapHelper>(
                    basisSwapQuote->quote(), fxSpotQuote->quote(), basisSwapConvention->settlementDays(),
                    basisSwapConvention->settlementCalendar(), basisSwapTenor, basisSwapConvention->rollConvention(),
                    flatIndex, spreadIndex, flatDiscountCurve, spreadDiscountCurve, basisSwapConvention->eom(),
                    flatIndex->currency().code() != fxSpotQuote->unitCcy(), flatTenor, spreadTenor, 0.0, 1.0, 1.0,
                    Calendar(), Calendar(), std::vector<Natural>(), std::vector<Calendar>(),
                    basisSwapConvention->paymentLag(), basisSwapConvention->flatPaymentLag(),
                    basisSwapConvention->includeSpread(), basisSwapConvention->lookback(),
                    basisSwapConvention->fixingDays(), basisSwapConvention->rateCutoff(),
                    basisSwapConvention->isAveraged(), basisSwapConvention->flatIncludeSpread(),
                    basisSwapConvention->flatLookback(), basisSwapConvention->flatFixingDays(),
                    basisSwapConvention->flatRateCutoff(), basisSwapConvention->flatIsAveraged(), true));
            } else { // the quote is for a cross currency basis swap with a resetting notional
                bool resetsOnFlatLeg = basisSwapConvention->flatIndexIsResettable();
                // the convention here is to call the resetting leg the "domestic leg",
                // and the constant notional leg the "foreign leg"
                bool spreadOnForeignCcy = resetsOnFlatLeg ? true : false;
                QuantLib::ext::shared_ptr<IborIndex> foreignIndex = resetsOnFlatLeg ? spreadIndex : flatIndex;
                Handle<YieldTermStructure> foreignDiscount = resetsOnFlatLeg ? spreadDiscountCurve : flatDiscountCurve;
                QuantLib::ext::shared_ptr<IborIndex> domesticIndex = resetsOnFlatLeg ? flatIndex : spreadIndex;
                Handle<YieldTermStructure> domesticDiscount = resetsOnFlatLeg ? flatDiscountCurve : spreadDiscountCurve;
                Handle<Quote> finalFxSpotQuote = fxSpotQuote->quote();
                // we might have to flip the given fx spot quote
                if (foreignIndex->currency().code() != fxSpotQuote->unitCcy()) {
                    auto m = [](Real x) { return 1.0 / x; };
                    finalFxSpotQuote =
                        Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(fxSpotQuote->quote(), m));
                }
                Period foreignTenor = resetsOnFlatLeg ? spreadTenor : flatTenor;
                Period domesticTenor = resetsOnFlatLeg ? flatTenor : spreadTenor;

                // Use foreign and dom discount curves for projecting FX forward rates (for e.g. resetting cashflows)
                instruments.push_back(QuantLib::ext::make_shared<CrossCcyBasisMtMResetSwapHelper>(
                    basisSwapQuote->quote(), finalFxSpotQuote, basisSwapConvention->settlementDays(),
                    basisSwapConvention->settlementCalendar(), basisSwapTenor, basisSwapConvention->rollConvention(),
                    foreignIndex, domesticIndex, foreignDiscount, domesticDiscount, Handle<YieldTermStructure>(),
                    Handle<YieldTermStructure>(), basisSwapConvention->eom(), spreadOnForeignCcy, foreignTenor,
                    domesticTenor, basisSwapConvention->paymentLag(), basisSwapConvention->flatPaymentLag(),
                    basisSwapConvention->includeSpread(), basisSwapConvention->lookback(),
                    basisSwapConvention->fixingDays(), basisSwapConvention->rateCutoff(),
                    basisSwapConvention->isAveraged(), basisSwapConvention->flatIncludeSpread(),
                    basisSwapConvention->flatLookback(), basisSwapConvention->flatFixingDays(),
                    basisSwapConvention->flatRateCutoff(), basisSwapConvention->flatIsAveraged(), true));
            }
        }
    }
}
void YieldCurve::addCrossCcyFixFloatSwaps(const QuantLib::ext::shared_ptr<YieldCurveSegment>& segment,
                                          vector<QuantLib::ext::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> convention = conventions->get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::CrossCcyFixFloat,
               "Conventions ID does not give cross currency fix float swap conventions.");
    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwapConvention> swapConvention =
        QuantLib::ext::dynamic_pointer_cast<CrossCcyFixFloatSwapConvention>(convention);

    QL_REQUIRE(swapConvention->fixedCurrency() == currency_,
               "The yield curve currency must "
                   << "equal the cross currency fix float swap's fixed leg currency");

    // Cast the segment
    QuantLib::ext::shared_ptr<CrossCcyYieldCurveSegment> swapSegment =
        QuantLib::ext::dynamic_pointer_cast<CrossCcyYieldCurveSegment>(segment);

    // Retrieve the discount curve on the float leg
    QuantLib::ext::shared_ptr<IborIndex> floatIndex = swapConvention->index();
    Currency floatLegCcy = floatIndex->currency();
    string foreignDiscountID = swapSegment->foreignDiscountCurveID();
    Handle<YieldTermStructure> floatLegDisc;

    QuantLib::ext::shared_ptr<YieldCurve> foreignDiscountCurve;
    if (!foreignDiscountID.empty()) {
        string floatLegDiscId = yieldCurveKey(floatLegCcy, foreignDiscountID, asofDate_);
        auto it = requiredYieldCurves_.find(floatLegDiscId);
        if (it != requiredYieldCurves_.end()) {
            floatLegDisc = it->second->handle();
        } else {
            QL_FAIL("The foreign discount curve, " << floatLegDiscId
                << ", required in the building "
                "of the curve, "
                << curveSpec_.name() << ", was not found.");
        }
    } else {
        // fall back on the foreign discount curve if no index given
        // look up the inccy discount curve - falls back to defualt if no inccy
        DLOG("YieldCurve::addCrossCcyFixFloatSwaps No discount curve provided for building curve "
             << curveSpec_.name() << ", looking up the inccy curve in the market.")
        floatLegDisc = market_->discountCurve(floatLegCcy.code(), Market::inCcyConfiguration);
    }

    // Retrieve the projection curve on the float leg. If empty, use discount curve.
    string floatLegProjId = swapSegment->foreignProjectionCurveID();
    if (floatLegProjId.empty()) {
        floatIndex = floatIndex->clone(floatLegDisc);
    } else {
        floatLegProjId = yieldCurveKey(floatLegCcy, floatLegProjId, asofDate_);
        auto it = requiredYieldCurves_.find(floatLegProjId);
        QL_REQUIRE(it != requiredYieldCurves_.end(), "The projection curve " << floatLegProjId
                                                                             << " required in the building of curve "
                                                                             << curveSpec_.name() << " was not found.");
        floatIndex = floatIndex->clone(it->second->handle());
    }

    // Create the FX spot quote for the helper. The quote needs to be number of units of fixed leg
    // currency for 1 unit of float leg currency. We convert the market quote here if needed.
    string fxSpotId = swapSegment->spotRateID();
    QuantLib::ext::shared_ptr<FXSpotQuote> fxSpotMd = getFxSpotQuote(fxSpotId);
    Currency mdUnitCcy = parseCurrency(fxSpotMd->unitCcy());
    Currency mdCcy = parseCurrency(fxSpotMd->ccy());
    Handle<Quote> fxSpotQuote;
    if (mdUnitCcy == floatLegCcy && mdCcy == currency_) {
        fxSpotQuote = fxSpotMd->quote();
    } else if (mdUnitCcy == currency_ && mdCcy == floatLegCcy) {
	auto m=[](Real x) { return 1.0/x;};
        fxSpotQuote = Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(fxSpotMd->quote(), m));
    } else {
        QL_FAIL("The FX spot market quote " << mdUnitCcy << "/" << mdCcy << " cannot be used "
                                            << "in the building of the curve " << curveSpec_.name() << ".");
    }

    // Create the helpers
    QL_REQUIRE(segment->pillarChoice() == QuantLib::Pillar::LastRelevantDate,
               "XCcy fix-float basis segment does not support pillar choice " << segment->pillarChoice());
    auto quoteIds = swapSegment->quotes();
    for (Size i = 0; i < quoteIds.size(); i++) {

        // Throws if quote not found
        QuantLib::ext::shared_ptr<MarketDatum> marketQuote = loader_.get(quoteIds[i], asofDate_);

        // Check that we have a valid basis swap quote
        if (marketQuote) {
            QuantLib::ext::shared_ptr<CrossCcyFixFloatSwapQuote> swapQuote =
                QuantLib::ext::dynamic_pointer_cast<CrossCcyFixFloatSwapQuote>(marketQuote);
            QL_REQUIRE(swapQuote, "Market quote should be of type 'CrossCcyFixFloatSwapQuote'");
            bool isResettableSwap = swapConvention->isResettable();
            QuantLib::ext::shared_ptr<RateHelper> helper;
            if (!isResettableSwap) {
                // Create the helper
                helper = QuantLib::ext::make_shared<CrossCcyFixFloatSwapHelper>(
                    swapQuote->quote(), fxSpotQuote, swapConvention->settlementDays(), swapConvention->settlementCalendar(),
                    swapConvention->settlementConvention(), swapQuote->maturity(), currency_,
                    swapConvention->fixedFrequency(), swapConvention->fixedConvention(), swapConvention->fixedDayCounter(),
                    floatIndex, floatLegDisc, Handle<Quote>(), swapConvention->eom());
            } else {
                bool resetsOnFloatLeg = swapConvention->floatIndexIsResettable();
                helper = QuantLib::ext::make_shared<CrossCcyFixFloatMtMResetSwapHelper>(
                    swapQuote->quote(), fxSpotQuote, swapConvention->settlementDays(), swapConvention->settlementCalendar(),
                    swapConvention->settlementConvention(), swapQuote->maturity(), currency_, swapConvention->fixedFrequency(), 
                    swapConvention->fixedConvention(), swapConvention->fixedDayCounter(), floatIndex, 
                    floatLegDisc, Handle<Quote>(), swapConvention->eom(), resetsOnFloatLeg);
            }
            instruments.push_back(helper);
        }
    }
}

QuantLib::ext::shared_ptr<FXSpotQuote> YieldCurve::getFxSpotQuote(string spotId) {
    // check the spot id, if like FX/RATE/CCY/CCY we go straight to the loader first
    std::vector<string> tokens;
    split(tokens, spotId, boost::is_any_of("/"));

    QuantLib::ext::shared_ptr<FXSpotQuote> fxSpotQuote;
    if (tokens.size() == 4 && tokens[0] == "FX" && tokens[1] == "RATE") {
        if (loader_.has(spotId, asofDate_)) {
            QuantLib::ext::shared_ptr<MarketDatum> fxSpotMarketQuote = loader_.get(spotId, asofDate_);

            if (fxSpotMarketQuote) {
                QL_REQUIRE(fxSpotMarketQuote->instrumentType() == MarketDatum::InstrumentType::FX_SPOT,
                           "Market quote not of type FX spot.");
                fxSpotQuote = QuantLib::ext::dynamic_pointer_cast<FXSpotQuote>(fxSpotMarketQuote);
                return fxSpotQuote;
            }
        }
    }

    // Try to use triangulation otherwise
    string unitCcy;
    string ccy;
    Handle<Quote> spot;
    if (tokens.size() > 1 && tokens[0] == "FX") {
        if (tokens.size() == 3) {
            unitCcy = tokens[1];
            ccy = tokens[2];
        } else if (tokens.size() == 4 && tokens[1] == "RATE") {
            unitCcy = tokens[2];
            ccy = tokens[3];
        } else {
            QL_FAIL("Invalid FX spot ID " << spotId);
        }
    } else if (tokens.size() == 1 && spotId.size() == 6) {
        unitCcy = spotId.substr(0, 3);
        ccy = spotId.substr(3);
    } else {
        QL_FAIL("Could not find quote for ID " << spotId << " with as of date " << io::iso_date(asofDate_) << ".");
    }
    spot = fxTriangulation_.getQuote(unitCcy + ccy);
    fxSpotQuote =
        QuantLib::ext::make_shared<FXSpotQuote>(spot->value(), asofDate_, spotId, MarketDatum::QuoteType::RATE, unitCcy, ccy);
    return fxSpotQuote;
}

} // namespace data
} // namespace ore
