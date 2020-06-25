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

#include <ql/currencies/exchangeratemanager.hpp>
#include <ql/indexes/ibor/usdlibor.hpp>
#include <ql/math/randomnumbers/haltonrsg.hpp>
#include <ql/pricingengines/bond/bondfunctions.hpp>
#include <ql/pricingengines/bond/discountingbondengine.hpp>
#include <ql/termstructures/yield/bondhelpers.hpp>
#include <ql/termstructures/yield/nonlinearfittingmethods.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/piecewisezerospreadedtermstructure.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/imm.hpp>
#include <ql/version.hpp>

#include <ql/indexes/ibor/all.hpp>
#include <ql/math/interpolations/convexmonotoneinterpolation.hpp>
#include <qle/indexes/ibor/brlcdi.hpp>
#include <qle/termstructures/averageoisratehelper.hpp>
#include <qle/termstructures/basistwoswaphelper.hpp>
#include <qle/termstructures/brlcdiratehelper.hpp>
#include <qle/termstructures/crossccybasismtmresetswaphelper.hpp>
#include <qle/termstructures/crossccybasisswaphelper.hpp>
#include <qle/termstructures/crossccyfixfloatswaphelper.hpp>
#include <qle/termstructures/discountratiomodifiedcurve.hpp>
#include <qle/termstructures/immfraratehelper.hpp>
#include <qle/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/oibasisswaphelper.hpp>
#include <qle/termstructures/oisratehelper.hpp>
#include <qle/termstructures/overnightindexfutureratehelper.hpp>
#include <qle/termstructures/subperiodsswaphelper.hpp>
#include <qle/termstructures/tenorbasisswaphelper.hpp>

#include <ored/marketdata/fittedbondcurvehelpermarket.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/envelope.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

// Temporary workaround to silence warnings on g++ until QL 1.17 is released with the
// pull request: https://github.com/lballabio/QuantLib/pull/679
#ifdef BOOST_MSVC
#define ATTR_UNUSED
#else
#define ATTR_UNUSED __attribute__((unused))
#endif

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
boost::shared_ptr<YieldTermStructure> buildYieldCurve(const vector<Date>& dates, const vector<QuantLib::Real>& rates,
                                                      const DayCounter& dayCounter,
                                                      YieldCurve::InterpolationMethod interpolationMethod) {

    boost::shared_ptr<YieldTermStructure> yieldts;
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
        yieldts.reset(new CurveType<QuantLib::ConvexMonotone>(dates, rates, dayCounter));
        break;
    default:
        QL_FAIL("Interpolation method not recognised.");
    }
    return yieldts;
}

boost::shared_ptr<YieldTermStructure> zerocurve(const vector<Date>& dates, const vector<Rate>& yields,
                                                const DayCounter& dayCounter,
                                                YieldCurve::InterpolationMethod interpolationMethod) {
    return buildYieldCurve<InterpolatedZeroCurve>(dates, yields, dayCounter, interpolationMethod);
}

boost::shared_ptr<YieldTermStructure> discountcurve(const vector<Date>& dates, const vector<DiscountFactor>& dfs,
                                                    const DayCounter& dayCounter,
                                                    YieldCurve::InterpolationMethod interpolationMethod) {
    return buildYieldCurve<InterpolatedDiscountCurve>(dates, dfs, dayCounter, interpolationMethod);
}

boost::shared_ptr<YieldTermStructure> forwardcurve(const vector<Date>& dates, const vector<Rate>& forwards,
                                                   const DayCounter& dayCounter,
                                                   YieldCurve::InterpolationMethod interpolationMethod) {
    return buildYieldCurve<InterpolatedForwardCurve>(dates, forwards, dayCounter, interpolationMethod);
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

YieldCurve::YieldCurve(Date asof, YieldCurveSpec curveSpec, const CurveConfigurations& curveConfigs,
                       const Loader& loader, const Conventions& conventions,
                       const map<string, boost::shared_ptr<YieldCurve>>& requiredYieldCurves,
                       const FXTriangulation& fxTriangulation,
                       const boost::shared_ptr<ReferenceDataManager>& referenceData)
    : asofDate_(asof), curveSpec_(curveSpec), loader_(loader), conventions_(conventions),
      requiredYieldCurves_(requiredYieldCurves), fxTriangulation_(fxTriangulation), referenceData_(referenceData) {

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
            map<string, boost::shared_ptr<YieldCurve>>::iterator it;
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
        } else {
            DLOG("Bootstrapping YieldCurve " << curveSpec_);
            buildBootstrappedCurve();
        }

        h_.linkTo(p_);
        if (extrapolation_) {
            h_->enableExtrapolation();
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

    LOG("Yield curve " << curveSpec_.name() << " built");
}

boost::shared_ptr<YieldTermStructure>
YieldCurve::piecewisecurve(const vector<boost::shared_ptr<RateHelper>>& instruments) {

    // Get configuration values for bootstrap
    Real accuracy = curveConfig_->bootstrapConfig().accuracy();
    Real globalAccuracy = curveConfig_->bootstrapConfig().globalAccuracy();
    bool dontThrow = curveConfig_->bootstrapConfig().dontThrow();
    Size maxAttempts = curveConfig_->bootstrapConfig().maxAttempts();
    Real maxFactor = curveConfig_->bootstrapConfig().maxFactor();
    Real minFactor = curveConfig_->bootstrapConfig().minFactor();
    Size dontThrowSteps = curveConfig_->bootstrapConfig().dontThrowSteps();

    // See comment here: https://github.com/lballabio/QuantLib/pull/679#issuecomment-525208897
    // to explain all the typedefs below. Waiting on a pull request from QuantLib here.
    boost::shared_ptr<YieldTermStructure> yieldts;
    switch (interpolationVariable_) {
    case InterpolationVariable::Zero:
        switch (interpolationMethod_) {
        case InterpolationMethod::Linear: {
            typedef PiecewiseYieldCurve<ZeroYield, Linear, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, Linear(),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::LogLinear: {
            typedef PiecewiseYieldCurve<ZeroYield, LogLinear, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, LogLinear(),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::NaturalCubic: {
            typedef PiecewiseYieldCurve<ZeroYield, Cubic, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, Cubic(CubicInterpolation::Kruger, true),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::FinancialCubic: {
            typedef PiecewiseYieldCurve<ZeroYield, Cubic, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_,
                Cubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                      CubicInterpolation::FirstDerivative),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::ConvexMonotone: {
            typedef PiecewiseYieldCurve<ZeroYield, ConvexMonotone, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, ConvexMonotone(),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        default:
            QL_FAIL("Interpolation method not recognised.");
        }
        break;
    case InterpolationVariable::Discount:
        switch (interpolationMethod_) {
        case InterpolationMethod::Linear: {
            typedef PiecewiseYieldCurve<Discount, Linear, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, Linear(),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::LogLinear: {
            typedef PiecewiseYieldCurve<Discount, LogLinear, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, LogLinear(),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::NaturalCubic: {
            typedef PiecewiseYieldCurve<Discount, Cubic, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, Cubic(CubicInterpolation::Kruger, true),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::FinancialCubic: {
            typedef PiecewiseYieldCurve<Discount, Cubic, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_,
                Cubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                      CubicInterpolation::FirstDerivative),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::ConvexMonotone: {
            typedef PiecewiseYieldCurve<Discount, ConvexMonotone, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, ConvexMonotone(),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        default:
            QL_FAIL("Interpolation method not recognised.");
        }
        break;
    case InterpolationVariable::Forward:
        switch (interpolationMethod_) {
        case InterpolationMethod::Linear: {
            typedef PiecewiseYieldCurve<ForwardRate, Linear, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, Linear(),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::LogLinear: {
            typedef PiecewiseYieldCurve<ForwardRate, LogLinear, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, LogLinear(),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::NaturalCubic: {
            typedef PiecewiseYieldCurve<ForwardRate, Cubic, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, Cubic(CubicInterpolation::Kruger, true),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::FinancialCubic: {
            typedef PiecewiseYieldCurve<ForwardRate, Cubic, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_,
                Cubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                      CubicInterpolation::FirstDerivative),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        case InterpolationMethod::ConvexMonotone: {
            typedef PiecewiseYieldCurve<ForwardRate, ConvexMonotone, QuantExt::IterativeBootstrap> my_curve;
            ATTR_UNUSED typedef my_curve::traits_type dummy;
            yieldts = boost::make_shared<my_curve>(
                asofDate_, instruments, zeroDayCounter_, ConvexMonotone(),
                QuantExt::IterativeBootstrap<my_curve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                       minFactor, dontThrowSteps));
        } break;
        default:
            QL_FAIL("Interpolation method not recognised.");
        }
        break;
    default:
        QL_FAIL("Interpolation variable not recognised.");
    }

    // Build fixed zero/discount curve that matches the boostrapped curve
    // initially, but does NOT react to quote changes: This is a workaround
    // for a QuantLib problem, where a fixed reference date piecewise
    // yield curve reacts to evaluation date changes because the bootstrap
    // helper recompute their start date (because they are realtive date
    // helper for deposits, fras, swaps, etc.).
    vector<Date> dates(instruments.size() + 1, asofDate_);
    vector<Real> zeros(instruments.size() + 1, 0.0);
    vector<Real> discounts(instruments.size() + 1, 1.0);
    vector<Real> forwards(instruments.size() + 1, 0.0);

    if (extrapolation_) {
        yieldts->enableExtrapolation();
    }
    for (Size i = 0; i < instruments.size(); i++) {
        dates[i + 1] = instruments[i]->latestDate();
        zeros[i + 1] = yieldts->zeroRate(dates[i + 1], zeroDayCounter_, Continuous);
        discounts[i + 1] = yieldts->discount(dates[i + 1]);
        forwards[i + 1] = yieldts->forwardRate(dates[i + 1], dates[i + 1], zeroDayCounter_, Continuous);
    }
    zeros[0] = zeros[1];
    forwards[0] = forwards[1];
    if (interpolationVariable_ == InterpolationVariable::Zero)
        p_ = zerocurve(dates, zeros, zeroDayCounter_, interpolationMethod_);
    else if (interpolationVariable_ == InterpolationVariable::Discount)
        p_ = discountcurve(dates, discounts, zeroDayCounter_, interpolationMethod_);
    else if (interpolationVariable_ == InterpolationVariable::Forward)
        p_ = forwardcurve(dates, forwards, zeroDayCounter_, interpolationMethod_);
    else
        QL_FAIL("Interpolation variable not recognised.");

    return p_;
}

void YieldCurve::buildZeroCurve() {

    QL_REQUIRE(curveSegments_.size() <= 1, "More than one zero curve "
                                           "segment not supported yet.");
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::Zero, "The curve segment is not of type Zero.");

    // Fill a vector of zero quotes.
    vector<boost::shared_ptr<ZeroQuote>> zeroQuotes;
    boost::shared_ptr<DirectYieldCurveSegment> zeroCurveSegment =
        boost::dynamic_pointer_cast<DirectYieldCurveSegment>(curveSegments_[0]);
    auto zeroQuoteIDs = zeroCurveSegment->quotes();

    for (Size i = 0; i < zeroQuoteIDs.size(); ++i) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(zeroQuoteIDs[i], asofDate_);
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::ZERO,
                       "Market quote not of type zero.");
            boost::shared_ptr<ZeroQuote> zeroQuote = boost::dynamic_pointer_cast<ZeroQuote>(marketQuote);
            zeroQuotes.push_back(zeroQuote);
        }
    }

    // Create the (date, zero) pairs.
    map<Date, Rate> data;
    boost::shared_ptr<Convention> convention = conventions_.get(curveSegments_[0]->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << curveSegments_[0]->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::Zero, "Conventions ID does not give zero rate conventions.");
    boost::shared_ptr<ZeroRateConvention> zeroConvention = boost::dynamic_pointer_cast<ZeroRateConvention>(convention);
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
        LOG("Insert zero curve point at time zero for " << curveSpec_.name() << ": "
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
            zeroes.push_back(tempRate.equivalentRate(Continuous, Annual, t));
        }
        discounts.push_back(tempRate.discountFactor(t));
        LOG("Add zero curve point for " << curveSpec_.name() << ": " << io::iso_date(dates.back()) << " " << fixed
                                        << setprecision(4) << zeroes.back() << " / " << discounts.back());
    }

    QL_REQUIRE(dates.size() == zeroes.size(), "Date and zero vectors differ in size.");
    QL_REQUIRE(dates.size() == discounts.size(), "Date and discount vectors differ in size.");

    // Now build curve with requested conventions
    if (interpolationVariable_ == YieldCurve::InterpolationVariable::Zero) {
        boost::shared_ptr<YieldTermStructure> tempCurve =
            zerocurve(dates, zeroes, quoteDayCounter, interpolationMethod_);
        zeroes.clear();
        for (Size i = 0; i < dates.size(); ++i) {
            Rate zero = tempCurve->zeroRate(dates[i], zeroDayCounter_, Continuous);
            zeroes.push_back(zero);
        }
        p_ = zerocurve(dates, zeroes, zeroDayCounter_, interpolationMethod_);
    } else if (interpolationVariable_ == YieldCurve::InterpolationVariable::Discount) {
        boost::shared_ptr<YieldTermStructure> tempCurve =
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

    // Fill a vector of zero spread quotes.
    vector<boost::shared_ptr<ZeroQuote>> quotes;
    boost::shared_ptr<ZeroSpreadedYieldCurveSegment> segment =
        boost::dynamic_pointer_cast<ZeroSpreadedYieldCurveSegment>(curveSegments_[0]);
    auto quoteIDs = segment->quotes();

    Date today = Settings::instance().evaluationDate();
    vector<Date> dates;
    vector<Handle<Quote>> quoteHandles;
    for (Size i = 0; i < quoteIDs.size(); ++i) {
        if (boost::shared_ptr<MarketDatum> md = loader_.get(quoteIDs[i], asofDate_)) {
            QL_REQUIRE(md->instrumentType() == MarketDatum::InstrumentType::ZERO, "Market quote not of type zero.");
            QL_REQUIRE(md->quoteType() == MarketDatum::QuoteType::YIELD_SPREAD,
                       "Market quote not of type yield spread.");
            boost::shared_ptr<ZeroQuote> zeroQuote = boost::dynamic_pointer_cast<ZeroQuote>(md);
            quotes.push_back(zeroQuote);
            dates.push_back(zeroQuote->tenorBased() ? today + zeroQuote->tenor() : zeroQuote->date());
            quoteHandles.push_back(zeroQuote->quote());
        }
    }

    QL_REQUIRE(!quotes.empty(),
               "Cannot build curve with spec " << curveSpec_.name() << " because there are no spread quotes");

    string referenceCurveID = segment->referenceCurveID();
    boost::shared_ptr<YieldCurve> referenceCurve;
    if (referenceCurveID != curveConfig_->curveID() && !referenceCurveID.empty()) {
        referenceCurveID = yieldCurveKey(currency_, referenceCurveID, asofDate_);
        map<string, boost::shared_ptr<YieldCurve>>::iterator it;
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

    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::Zero, "Conventions ID does not give zero rate conventions.");
    boost::shared_ptr<ZeroRateConvention> zeroConvention = boost::dynamic_pointer_cast<ZeroRateConvention>(convention);
    DayCounter quoteDayCounter = zeroConvention->dayCounter();
    Compounding comp = zeroConvention->compounding();
    Frequency freq = zeroConvention->compoundingFrequency();

    p_ = boost::shared_ptr<YieldTermStructure>(new PiecewiseZeroSpreadedTermStructure(
        referenceCurve->handle(), quoteHandles, dates, comp, freq, quoteDayCounter));
}

void YieldCurve::buildDiscountCurve() {

    QL_REQUIRE(curveSegments_.size() <= 1, "More than one discount curve "
                                           "segment not supported yet.");
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::Discount,
               "The curve segment is not of type Discount.");

    // Create the (date, discount) pairs.
    map<Date, DiscountFactor> data;
    boost::shared_ptr<DirectYieldCurveSegment> discountCurveSegment =
        boost::dynamic_pointer_cast<DirectYieldCurveSegment>(curveSegments_[0]);
    auto discountQuoteIDs = discountCurveSegment->quotes();

    for (Size i = 0; i < discountQuoteIDs.size(); ++i) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(discountQuoteIDs[i], asofDate_);
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::DISCOUNT,
                       "Market quote not of type Discount.");
            boost::shared_ptr<DiscountQuote> discountQuote = boost::dynamic_pointer_cast<DiscountQuote>(marketQuote);
            data[discountQuote->date()] = discountQuote->quote()->value();
        }
    }

    QL_REQUIRE(data.size() > 0, "No market data found for curve spec " << curveSpec_.name() << " with as of date "
                                                                       << io::iso_date(asofDate_));

    if (data.begin()->first > asofDate_) {
        LOG("Insert discount curve point at time zero for " << curveSpec_.name());
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
        LOG("Add discount curve point for " << curveSpec_.name() << ": " << io::iso_date(dates.back()) << " "
                                            << discounts.back());
    }

    QL_REQUIRE(dates.size() == discounts.size(), "Date and discount vectors differ in size.");

    boost::shared_ptr<YieldTermStructure> tempDiscCurve =
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

    /* Loop over segments and add helpers. */
    vector<boost::shared_ptr<RateHelper>> instruments;
    for (Size i = 0; i < curveSegments_.size(); i++) {
        switch (curveSegments_[i]->type()) {
        case YieldCurveSegment::Type::Deposit:
            addDeposits(curveSegments_[i], instruments);
            break;
        case YieldCurveSegment::Type::FRA:
            addFras(curveSegments_[i], instruments);
            break;
        case YieldCurveSegment::Type::Future:
            addFutures(curveSegments_[i], instruments);
            break;
        case YieldCurveSegment::Type::OIS:
            addOISs(curveSegments_[i], instruments);
            break;
        case YieldCurveSegment::Type::Swap:
            addSwaps(curveSegments_[i], instruments);
            break;
        case YieldCurveSegment::Type::AverageOIS:
            addAverageOISs(curveSegments_[i], instruments);
            break;
        case YieldCurveSegment::Type::TenorBasis:
            addTenorBasisSwaps(curveSegments_[i], instruments);
            break;
        case YieldCurveSegment::Type::TenorBasisTwo:
            addTenorBasisTwoSwaps(curveSegments_[i], instruments);
            break;
        case YieldCurveSegment::Type::BMABasis:
            addBMABasisSwaps(curveSegments_[i], instruments);
            break;
        case YieldCurveSegment::Type::FXForward:
            addFXForwards(curveSegments_[i], instruments);
            break;
        case YieldCurveSegment::Type::CrossCcyBasis:
            addCrossCcyBasisSwaps(curveSegments_[i], instruments);
            break;
        case YieldCurveSegment::Type::CrossCcyFixFloat:
            addCrossCcyFixFloatSwaps(curveSegments_[i], instruments);
            break;
        default:
            QL_FAIL("Yield curve segment type not recognized.");
            break;
        }
    }

    DLOG("Bootstrapping with " << instruments.size() << " instruments");

    /* Build the bootstrapped curve from the instruments. */
    QL_REQUIRE(instruments.size() > 0,
               "Empty instrument list for date = " << io::iso_date(asofDate_) << " and curve = " << curveSpec_.name());
    p_ = piecewisecurve(instruments);
}

void YieldCurve::buildDiscountRatioCurve() {
    QL_REQUIRE(curveSegments_.size() == 1, "A discount ratio curve must contain exactly one segment");
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::DiscountRatio,
               "The curve segment is not of type 'DiscountRatio'.");

    boost::shared_ptr<DiscountRatioYieldCurveSegment> segment =
        boost::dynamic_pointer_cast<DiscountRatioYieldCurveSegment>(curveSegments_[0]);

    // Find the underlying curves in the reference curves
    boost::shared_ptr<YieldCurve> baseCurve = getYieldCurve(segment->baseCurveCurrency(), segment->baseCurveId());
    QL_REQUIRE(baseCurve, "The base curve '" << segment->baseCurveId() << "' cannot be empty");

    boost::shared_ptr<YieldCurve> numCurve =
        getYieldCurve(segment->numeratorCurveCurrency(), segment->numeratorCurveId());
    QL_REQUIRE(numCurve, "The numerator curve '" << segment->numeratorCurveId() << "' cannot be empty");

    boost::shared_ptr<YieldCurve> denCurve =
        getYieldCurve(segment->denominatorCurveCurrency(), segment->denominatorCurveId());
    QL_REQUIRE(denCurve, "The denominator curve '" << segment->denominatorCurveId() << "' cannot be empty");

    p_ = boost::make_shared<DiscountRatioModifiedCurve>(baseCurve->handle(), numCurve->handle(), denCurve->handle());
}

boost::shared_ptr<YieldCurve> YieldCurve::getYieldCurve(const string& ccy, const string& id) const {
    if (id != curveConfig_->curveID() && !id.empty()) {
        string idLookup = yieldCurveKey(parseCurrency(ccy), id, asofDate_);
        map<string, boost::shared_ptr<YieldCurve>>::const_iterator it = requiredYieldCurves_.find(idLookup);
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

    boost::shared_ptr<FittedBondYieldCurveSegment> curveSegment =
        boost::dynamic_pointer_cast<FittedBondYieldCurveSegment>(curveSegments_[0]);
    QL_REQUIRE(curveSegment != nullptr, "could not cast to FittedBondYieldCurveSegment, this is unexpected");

    // build vector of bond helpers

    auto quoteIDs = curveSegment->quotes();
    std::vector<boost::shared_ptr<QuantLib::Bond>> bonds;
    std::vector<boost::shared_ptr<BondHelper>> helpers;
    std::vector<Real> marketPrices;
    std::vector<std::string> securityIDs;
    Date lastMaturity = Date::minDate(), firstMaturity = Date::maxDate();

    // not really relevant, we just need a working engine configuration so that the bond can be built
    // the pricing engine here is _not_ used during the curve fitting, for this a local engine is
    // set up within FittedBondDiscountCurve
    auto engineData = boost::make_shared<EngineData>();
    engineData->model("Bond") = "DiscountedCashflows";
    engineData->engine("Bond") = "DiscountingRiskyBondEngine";
    engineData->engineParameters("Bond") = {{"TimestepPeriod", "6M"}};

    std::map<std::string, Handle<YieldTermStructure>> iborCurveMapping;
    for (auto const& c : curveSegment->iborIndexCurves()) {
        auto y = requiredYieldCurves_.find(yieldCurveKey(currency_, c.first, asofDate_));
        QL_REQUIRE(y != requiredYieldCurves_.end(),
                   "required yield curve '" << c.first << "' not provided for fitted bond curve");
        iborCurveMapping[c.first] = y->second->handle();
    }

    auto engineFactory = boost::make_shared<EngineFactory>(
        engineData, boost::make_shared<FittedBondCurveHelperMarket>(iborCurveMapping, conventions_),
        std::map<MarketContext, string>(), std::vector<boost::shared_ptr<EngineBuilder>>(),
        std::vector<boost::shared_ptr<LegBuilder>>(), referenceData_);

    for (Size i = 0; i < quoteIDs.size(); ++i) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(quoteIDs[i], asofDate_);
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BOND &&
                           marketQuote->quoteType() == MarketDatum::QuoteType::PRICE,
                       "Market quote not of type Bond / Price.");
            boost::shared_ptr<BondPriceQuote> bondQuote = boost::dynamic_pointer_cast<BondPriceQuote>(marketQuote);
            QL_REQUIRE(bondQuote, "market quote has type bond quote, but can not be casted, this is unexpected.");
            Handle<Quote> rescaledBondQuote(boost::make_shared<SimpleQuote>((bondQuote->quote()->value()) * 100));
            string securityID = bondQuote->securityID();

            QL_REQUIRE(referenceData_ != nullptr && referenceData_->hasData("Bond", securityID),
                       "bond reference data for '" << securityID << "' required to build fitted bond curve");
            ore::data::Bond bond(Envelope(), BondData(securityID, 1.0));
            bond.build(engineFactory);
            auto qlInstr = boost::dynamic_pointer_cast<QuantLib::Bond>(bond.instrument()->qlInstrument());
            QL_REQUIRE(qlInstr != nullptr, "could not cast to QuantLib::Bond, this is unexpected");

            // skip bonds with settlement date <= curve reference date or which are otherwise non-tradeable
            if (qlInstr->settlementDate() > asofDate_ && QuantLib::BondFunctions::isTradable(*qlInstr)) {
                bonds.push_back(qlInstr);
                helpers.push_back(boost::make_shared<BondHelper>(rescaledBondQuote, qlInstr));
                Date thisMaturity = qlInstr->maturityDate();
                lastMaturity = std::max(lastMaturity, thisMaturity);
                firstMaturity = std::min(firstMaturity, thisMaturity);
                DLOG("added bond " << securityID << ", maturity = " << QuantLib::io::iso_date(thisMaturity)
                                   << ", clean price = " << rescaledBondQuote->value() << ", yield (cont,act/act) = "
                                   << qlInstr->yield(rescaledBondQuote->value(), ActualActual(), Continuous,
                                                     NoFrequency));
                marketPrices.push_back(bondQuote->quote()->value());
                securityIDs.push_back(securityID);
            } else {
                DLOG("skipped bond " << securityID << " with settlement date "
                                     << QuantLib::io::iso_date(qlInstr->settlementDate()) << ", isTradable = "
                                     << std::boolalpha << QuantLib::BondFunctions::isTradable(*qlInstr));
            }
        }
    }

    // fit bond curve to helpers

    QL_REQUIRE(helpers.size() >= 1, "no bonds for fitting bond curve");
    DLOG("Fitting bond curve with " << helpers.size() << " bonds.");

    Real minCutoffTime = 0.0, maxCutoffTime = QL_MAX_REAL;
    if (curveSegment->extrapolateFlat()) {
        minCutoffTime = zeroDayCounter_.yearFraction(asofDate_, firstMaturity);
        maxCutoffTime = zeroDayCounter_.yearFraction(asofDate_, lastMaturity);
        DLOG("extrapolate flat outside " << minCutoffTime << "," << maxCutoffTime);
    }

    boost::shared_ptr<FittedBondDiscountCurve::FittingMethod> method;

#if QL_HEX_VERSION >= 0x01190000 || defined(QL_ORE_PATCH)
    // will work in QL 1.19
    switch (interpolationMethod_) {
    case InterpolationMethod::ExponentialSplines:
        method = boost::make_shared<ExponentialSplinesFitting>(true, Array(), ext::shared_ptr<OptimizationMethod>(),
                                                               Array(), minCutoffTime, maxCutoffTime);
        break;
    case InterpolationMethod::NelsonSiegel:
        method = boost::make_shared<NelsonSiegelFitting>(Array(), ext::shared_ptr<OptimizationMethod>(), Array(),
                                                         minCutoffTime, maxCutoffTime);
        break;
    case InterpolationMethod::Svensson:
        method = boost::make_shared<SvenssonFitting>(Array(), ext::shared_ptr<OptimizationMethod>(), Array(),
                                                     minCutoffTime, maxCutoffTime);
        break;
    default:
        QL_FAIL("unknown fitting method");
    }
#else
#pragma message("yieldcurve.cpp uses fitting method constructors without min/max cutoff time parameters")
    switch (interpolationMethod_) {
    case InterpolationMethod::ExponentialSplines:
        method = boost::make_shared<ExponentialSplinesFitting>(true, Array(), ext::shared_ptr<OptimizationMethod>(),
                                                               Array());
        break;
    case InterpolationMethod::NelsonSiegel:
        method = boost::make_shared<NelsonSiegelFitting>(Array(), ext::shared_ptr<OptimizationMethod>(), Array());
        break;
    case InterpolationMethod::Svensson:
        method = boost::make_shared<SvenssonFitting>(Array(), ext::shared_ptr<OptimizationMethod>(), Array());
        break;
    default:
        QL_FAIL("unknown fitting method");
    }
#endif

    boost::shared_ptr<FittedBondDiscountCurve> tmp, current;
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
        if (i > 0) {
            auto seq = halton.nextSequence();
            guess = Array(seq.value.begin(), seq.value.end());
            if (interpolationMethod_ == InterpolationMethod::NelsonSiegel) {
                guess[0] = guess[0] * 0.10 - 0.05; // long term yield
                guess[1] = guess[1] * 0.10 - 0.05; // short term component
                guess[2] = guess[2] * 0.10 - 0.05; // medium term component
                guess[3] = guess[3] * 5.0;         // decay factor
            } else {
                QL_FAIL("randomised optimisation seed not implemented");
            }
        }
        current = boost::make_shared<FittedBondDiscountCurve>(asofDate_, helpers, zeroDayCounter_, *method, 1.0e-10,
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
    auto engine = boost::make_shared<DiscountingBondEngine>(Handle<YieldTermStructure>(tmp));
    for (Size i = 0; i < bonds.size(); ++i) {
        bonds[i]->setPricingEngine(engine);
        DLOG("bond " << securityIDs[i] << ", model clean price = " << bonds[i]->cleanPrice()
                     << ", yield (cont,actact) = "
                     << bonds[i]->yield(bonds[i]->cleanPrice(), ActualActual(), Continuous, NoFrequency)
                     << ", NPV = " << bonds[i]->NPV());
    }

    Real tolerance = curveConfig_->bootstrapConfig().globalAccuracy() == Null<Real>()
                         ? curveConfig_->bootstrapConfig().accuracy()
                         : curveConfig_->bootstrapConfig().globalAccuracy();
    QL_REQUIRE(curveConfig_->bootstrapConfig().dontThrow() || minError < tolerance,
               "Fitted Bond Curve cost value (" << minError << ") exceeds tolerance (" << tolerance << ")");

    if (extrapolation_)
        tmp->enableExtrapolation();

    p_ = tmp;
}

void YieldCurve::addDeposits(const boost::shared_ptr<YieldCurveSegment>& segment,
                             vector<boost::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::Deposit,
               "Conventions ID does not give deposit rate conventions.");
    boost::shared_ptr<DepositConvention> depositConvention = boost::dynamic_pointer_cast<DepositConvention>(convention);

    boost::shared_ptr<SimpleYieldCurveSegment> depositSegment =
        boost::dynamic_pointer_cast<SimpleYieldCurveSegment>(segment);
    auto depositQuoteIDs = depositSegment->quotes();

    for (Size i = 0; i < depositQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(depositQuoteIDs[i], asofDate_);

        // Check that we have a valid deposit quote
        if (marketQuote) {
            boost::shared_ptr<MoneyMarketQuote> depositQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::MM,
                       "Market quote not of type Deposit.");
            depositQuote = boost::dynamic_pointer_cast<MoneyMarketQuote>(marketQuote);

            // Create a deposit helper if we do.
            boost::shared_ptr<RateHelper> depositHelper;
            Period depositTerm = depositQuote->term();
            Period fwdStart = depositQuote->fwdStart();
            Natural fwdStartDays = static_cast<Natural>(fwdStart.length());
            Handle<Quote> hQuote(depositQuote->quote());

            if (depositConvention->indexBased()) {
                // indexName will have the form ccy-name so examples would be:
                // EUR-EONIA, USD-FedFunds, EUR-EURIBOR, USD-LIBOR, etc.
                string indexName = depositConvention->index();
                boost::shared_ptr<IborIndex> index;
                if (isOvernightIndex(indexName, conventions_)) {
                    // No need for the term here
                    index = parseIborIndex(indexName, Handle<YieldTermStructure>(),
                                           conventions_.has(indexName, Convention::Type::OvernightIndex)
                                               ? conventions_.get(indexName)
                                               : nullptr);
                } else {
                    // Note that a depositTerm with units Days here could end up as a string with another unit
                    // For example:
                    // 7 * Days will go to ccy-tenor-1W - CNY IR index is a case
                    // 28 * Days will go to ccy-tenor-4W - MXN TIIE is a case
                    // parseIborIndex should be able to handle this for appropriate depositTerm values
                    stringstream ss;
                    ss << indexName << "-" << io::short_period(depositTerm);
                    indexName = ss.str();
                    index = parseIborIndex(indexName, Handle<YieldTermStructure>(),
                                           conventions_.has(indexName, Convention::Type::IborIndex)
                                               ? conventions_.get(indexName)
                                               : nullptr);
                }
                depositHelper = boost::make_shared<DepositRateHelper>(hQuote, index);
            } else {
                QL_REQUIRE(fwdStart.units() == Days, "The forward start time unit for deposits "
                                                     "must be expressed in days.");
                depositHelper.reset(new DepositRateHelper(
                    hQuote, depositTerm, fwdStartDays, depositConvention->calendar(), depositConvention->convention(),
                    depositConvention->eom(), depositConvention->dayCounter()));
            }
            instruments.push_back(depositHelper);
        }
    }
}

void YieldCurve::addFutures(const boost::shared_ptr<YieldCurveSegment>& segment,
                            vector<boost::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::Future,
               "Conventions ID does not give deposit rate conventions.");
    boost::shared_ptr<FutureConvention> futureConvention = boost::dynamic_pointer_cast<FutureConvention>(convention);

    boost::shared_ptr<SimpleYieldCurveSegment> futureSegment =
        boost::dynamic_pointer_cast<SimpleYieldCurveSegment>(segment);
    auto futureQuoteIDs = futureSegment->quotes();

    for (Size i = 0; i < futureQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(futureQuoteIDs[i], asofDate_);

        // Check that we have a valid future quote
        if (marketQuote) {
            if (auto on = boost::dynamic_pointer_cast<OvernightIndex>(futureConvention->index())) {
                // Overnight Index Future
                boost::shared_ptr<OIFutureQuote> futureQuote;
                QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::OI_FUTURE,
                           "Market quote not of type Overnight Index Future.");
                futureQuote = boost::dynamic_pointer_cast<OIFutureQuote>(marketQuote);

                // Create a Overnight index future helper
                Date refEnd = Date(1, futureQuote->expiryMonth(), futureQuote->expiryYear());
                Date refStart = refEnd - futureQuote->tenor();
                Date startDate = IMM::nextDate(refStart);
                Date endDate = IMM::nextDate(refEnd);
                boost::shared_ptr<RateHelper> futureHelper(
                    new OvernightIndexFutureRateHelper(futureQuote->quote(), startDate, endDate, on));
                instruments.push_back(futureHelper);
            } else {
                // MM Future
                boost::shared_ptr<MMFutureQuote> futureQuote;
                QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::MM_FUTURE,
                           "Market quote not of type Money Market Future.");
                futureQuote = boost::dynamic_pointer_cast<MMFutureQuote>(marketQuote);

                // Create a MM future helper
                Date refDate(1, futureQuote->expiryMonth(), futureQuote->expiryYear());
                Date immDate = IMM::nextDate(refDate, false);
                boost::shared_ptr<RateHelper> futureHelper(
                    new FuturesRateHelper(futureQuote->quote(), immDate, futureConvention->index()));

                instruments.push_back(futureHelper);
            }
        }
    }
}

void YieldCurve::addFras(const boost::shared_ptr<YieldCurveSegment>& segment,
                         vector<boost::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::FRA, "Conventions ID does not give FRA conventions.");
    boost::shared_ptr<FraConvention> fraConvention = boost::dynamic_pointer_cast<FraConvention>(convention);

    boost::shared_ptr<SimpleYieldCurveSegment> fraSegment =
        boost::dynamic_pointer_cast<SimpleYieldCurveSegment>(segment);
    auto fraQuoteIDs = fraSegment->quotes();

    for (Size i = 0; i < fraQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(fraQuoteIDs[i], asofDate_);

        // Check that we have a valid FRA quote
        if (marketQuote) {
            QL_REQUIRE((marketQuote->instrumentType() == MarketDatum::InstrumentType::FRA) ||
                           (marketQuote->instrumentType() == MarketDatum::InstrumentType::IMM_FRA),
                       "Market quote not of type FRA.");

            // Create a FRA helper if we do.

            boost::shared_ptr<RateHelper> fraHelper;

            if (marketQuote->instrumentType() == MarketDatum::InstrumentType::IMM_FRA) {
                boost::shared_ptr<ImmFraQuote> immFraQuote;
                immFraQuote = boost::dynamic_pointer_cast<ImmFraQuote>(marketQuote);
                Size imm1 = immFraQuote->imm1();
                Size imm2 = immFraQuote->imm2();
                fraHelper =
                    boost::make_shared<ImmFraRateHelper>(immFraQuote->quote(), imm1, imm2, fraConvention->index());
            } else if (marketQuote->instrumentType() == MarketDatum::InstrumentType::FRA) {
                boost::shared_ptr<FRAQuote> fraQuote;
                fraQuote = boost::dynamic_pointer_cast<FRAQuote>(marketQuote);
                Period periodToStart = fraQuote->fwdStart();
                fraHelper = boost::make_shared<FraRateHelper>(fraQuote->quote(), periodToStart, fraConvention->index());
            } else {
                QL_FAIL("Market quote not of type FRA.");
            }

            instruments.push_back(fraHelper);
        }
    }
}

void YieldCurve::addOISs(const boost::shared_ptr<YieldCurveSegment>& segment,
                         vector<boost::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::OIS, "Conventions ID does not give OIS conventions.");
    boost::shared_ptr<OisConvention> oisConvention = boost::dynamic_pointer_cast<OisConvention>(convention);

    boost::shared_ptr<SimpleYieldCurveSegment> oisSegment =
        boost::dynamic_pointer_cast<SimpleYieldCurveSegment>(segment);

    // If projection curve ID is not the this curve.
    string projectionCurveID = oisSegment->projectionCurveID();
    boost::shared_ptr<OvernightIndex> onIndex = oisConvention->index();
    if (projectionCurveID != curveConfig_->curveID() && !projectionCurveID.empty()) {
        projectionCurveID = yieldCurveKey(currency_, projectionCurveID, asofDate_);
        boost::shared_ptr<YieldCurve> projectionCurve;
        map<string, boost::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(projectionCurveID);
        if (it != requiredYieldCurves_.end()) {
            projectionCurve = it->second;
        } else {
            QL_FAIL("The projection curve, " << projectionCurveID
                                             << ", required in the building "
                                                "of the curve, "
                                             << curveSpec_.name() << ", was not found.");
        }
        onIndex = boost::dynamic_pointer_cast<OvernightIndex>(onIndex->clone(projectionCurve->handle()));
    }

    // BRL CDI overnight needs a specialised rate helper so we use this below to switch
    boost::shared_ptr<BRLCdi> brlCdiIndex = boost::dynamic_pointer_cast<BRLCdi>(onIndex);

    auto oisQuoteIDs = oisSegment->quotes();
    for (Size i = 0; i < oisQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(oisQuoteIDs[i], asofDate_);

        // Check that we have a valid OIS quote
        if (marketQuote) {
            boost::shared_ptr<SwapQuote> oisQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::IR_SWAP,
                       "Market quote (" << marketQuote->name() << ") not of type swap.");
            oisQuote = boost::dynamic_pointer_cast<SwapQuote>(marketQuote);

            // Create a swap helper if we do.
            Period oisTenor = oisQuote->term();
            boost::shared_ptr<RateHelper> oisHelper;
            if (brlCdiIndex) {
                oisHelper = boost::make_shared<BRLCdiRateHelper>(
                    oisTenor, oisQuote->quote(), brlCdiIndex,
                    discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>(), true);
            } else {
                oisHelper = boost::make_shared<QuantExt::OISRateHelper>(
                    oisConvention->spotLag(), oisTenor, oisQuote->quote(), onIndex, oisConvention->fixedDayCounter(),
                    oisConvention->paymentLag(), oisConvention->eom(), oisConvention->fixedFrequency(),
                    oisConvention->fixedConvention(), oisConvention->fixedPaymentConvention(), oisConvention->rule(),
                    discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>(), true);
            }

            instruments.push_back(oisHelper);
        }
    }
}

void YieldCurve::addSwaps(const boost::shared_ptr<YieldCurveSegment>& segment,
                          vector<boost::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::Swap, "Conventions ID does not give swap conventions.");
    boost::shared_ptr<IRSwapConvention> swapConvention = boost::dynamic_pointer_cast<IRSwapConvention>(convention);

    boost::shared_ptr<SimpleYieldCurveSegment> swapSegment =
        boost::dynamic_pointer_cast<SimpleYieldCurveSegment>(segment);
    if (swapSegment->projectionCurveID() != curveConfig_->curveID() && !swapSegment->projectionCurveID().empty()) {
        QL_FAIL("Solving for discount curve given the projection"
                " curve is not implemented yet");
    }
    auto swapQuoteIDs = swapSegment->quotes();

    for (Size i = 0; i < swapQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(swapQuoteIDs[i], asofDate_);

        // Check that we have a valid swap quote
        if (marketQuote) {
            boost::shared_ptr<SwapQuote> swapQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::IR_SWAP,
                       "Market quote not of type swap.");
            swapQuote = boost::dynamic_pointer_cast<SwapQuote>(marketQuote);

            // Create a swap helper if we do.
            Period swapTenor = swapQuote->term();
            boost::shared_ptr<RateHelper> swapHelper;
            if (swapConvention->hasSubPeriod()) {
                swapHelper = boost::make_shared<SubPeriodsSwapHelper>(
                    swapQuote->quote(), swapTenor, Period(swapConvention->fixedFrequency()),
                    swapConvention->fixedCalendar(), swapConvention->fixedDayCounter(),
                    swapConvention->fixedConvention(), Period(swapConvention->floatFrequency()),
                    swapConvention->index(), swapConvention->index()->dayCounter(),
                    discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>(),
                    swapConvention->subPeriodsCouponType());
            } else {
                swapHelper = boost::make_shared<SwapRateHelper>(
                    swapQuote->quote(), swapTenor, swapConvention->fixedCalendar(), swapConvention->fixedFrequency(),
                    swapConvention->fixedConvention(), swapConvention->fixedDayCounter(), swapConvention->index(),
                    Handle<Quote>(), 0 * Days,
                    discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>());
            }

            instruments.push_back(swapHelper);
        }
    }
}

void YieldCurve::addAverageOISs(const boost::shared_ptr<YieldCurveSegment>& segment,
                                vector<boost::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::AverageOIS,
               "Conventions ID does not give average OIS conventions.");
    boost::shared_ptr<AverageOisConvention> averageOisConvention =
        boost::dynamic_pointer_cast<AverageOisConvention>(convention);

    boost::shared_ptr<AverageOISYieldCurveSegment> averageOisSegment =
        boost::dynamic_pointer_cast<AverageOISYieldCurveSegment>(segment);

    // If projection curve ID is not the this curve.
    string projectionCurveID = averageOisSegment->projectionCurveID();
    boost::shared_ptr<OvernightIndex> onIndex = averageOisConvention->index();
    if (projectionCurveID != curveConfig_->curveID() && !projectionCurveID.empty()) {
        projectionCurveID = yieldCurveKey(currency_, projectionCurveID, asofDate_);
        boost::shared_ptr<YieldCurve> projectionCurve;
        map<string, boost::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(projectionCurveID);
        if (it != requiredYieldCurves_.end()) {
            projectionCurve = it->second;
        } else {
            QL_FAIL("The projection curve, " << projectionCurveID
                                             << ", required in the building "
                                                "of the curve, "
                                             << curveSpec_.name() << ", was not found.");
        }
        onIndex = boost::dynamic_pointer_cast<OvernightIndex>(onIndex->clone(projectionCurve->handle()));
    }

    auto averageOisQuoteIDs = averageOisSegment->quotes();
    for (Size i = 0; i < averageOisQuoteIDs.size(); i += 2) {
        // we are assuming i = spread, i+1 = rate
        QL_REQUIRE(i % 2 == 0, "i is not even");
        /* An average OIS quote is a composite of a swap quote and a basis
           spread quote. Check first that we have these. */
        // Firstly, the rate quote.
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(averageOisQuoteIDs[i], asofDate_);
        boost::shared_ptr<SwapQuote> swapQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::IR_SWAP,
                       "Market quote not of type swap.");
            swapQuote = boost::dynamic_pointer_cast<SwapQuote>(marketQuote);

            // Secondly, the basis spread quote.
            marketQuote = loader_.get(averageOisQuoteIDs[i + 1], asofDate_);
            boost::shared_ptr<BasisSwapQuote> basisQuote;
            if (marketQuote) {
                QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BASIS_SWAP,
                           "Market quote not of type basis swap.");
                basisQuote = boost::dynamic_pointer_cast<BasisSwapQuote>(marketQuote);

                // Create an average OIS helper if we do.
                Period AverageOisTenor = swapQuote->term();
                QL_REQUIRE(AverageOisTenor == basisQuote->maturity(),
                           "The swap "
                           "and basis swap components of the Average OIS must "
                           "have the same maturity.");
                boost::shared_ptr<RateHelper> averageOisHelper(new QuantExt::AverageOISRateHelper(
                    swapQuote->quote(), averageOisConvention->spotLag() * Days, AverageOisTenor,
                    averageOisConvention->fixedTenor(), averageOisConvention->fixedDayCounter(),
                    averageOisConvention->fixedCalendar(), averageOisConvention->fixedConvention(),
                    averageOisConvention->fixedPaymentConvention(), onIndex, averageOisConvention->onTenor(),
                    basisQuote->quote(), averageOisConvention->rateCutoff(),
                    discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>()));

                instruments.push_back(averageOisHelper);
            }
        }
    }
}

void YieldCurve::addTenorBasisSwaps(const boost::shared_ptr<YieldCurveSegment>& segment,
                                    vector<boost::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::TenorBasisSwap,
               "Conventions ID does not give tenor basis swap conventions.");
    boost::shared_ptr<TenorBasisSwapConvention> basisSwapConvention =
        boost::dynamic_pointer_cast<TenorBasisSwapConvention>(convention);

    boost::shared_ptr<TenorBasisYieldCurveSegment> basisSwapSegment =
        boost::dynamic_pointer_cast<TenorBasisYieldCurveSegment>(segment);

    // If short index projection curve ID is not this curve.
    string shortCurveID = basisSwapSegment->shortProjectionCurveID();
    boost::shared_ptr<IborIndex> shortIndex = basisSwapConvention->shortIndex();
    if (shortCurveID != curveConfig_->curveID() && !shortCurveID.empty()) {
        shortCurveID = yieldCurveKey(currency_, shortCurveID, asofDate_);
        boost::shared_ptr<YieldCurve> shortCurve;
        map<string, boost::shared_ptr<YieldCurve>>::iterator it;
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
    string longCurveID = basisSwapSegment->longProjectionCurveID();
    boost::shared_ptr<IborIndex> longIndex = basisSwapConvention->longIndex();
    if (longCurveID != curveConfig_->curveID() && !longCurveID.empty()) {
        longCurveID = yieldCurveKey(currency_, longCurveID, asofDate_);
        boost::shared_ptr<YieldCurve> longCurve;
        map<string, boost::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(longCurveID);
        if (it != requiredYieldCurves_.end()) {
            longCurve = it->second;
        } else {
            QL_FAIL("The long side projection curve, " << longCurveID
                                                       << ", required in the building "
                                                          "of the curve, "
                                                       << curveSpec_.name() << ", was not found.");
        }
        longIndex = longIndex->clone(longCurve->handle());
    }

    auto basisSwapQuoteIDs = basisSwapSegment->quotes();
    for (Size i = 0; i < basisSwapQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(basisSwapQuoteIDs[i], asofDate_);

        // Check that we have a valid basis swap quote
        if (marketQuote) {
            boost::shared_ptr<BasisSwapQuote> basisSwapQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BASIS_SWAP,
                       "Market quote not of type basis swap.");
            basisSwapQuote = boost::dynamic_pointer_cast<BasisSwapQuote>(marketQuote);

            // Create a tenor basis swap helper if we do.
            Period basisSwapTenor = basisSwapQuote->maturity();
            boost::shared_ptr<RateHelper> basisSwapHelper;
            if (boost::dynamic_pointer_cast<OvernightIndex>(shortIndex) != nullptr) {
                // is it OIS vs Libor...
                basisSwapHelper.reset(
                    new OIBSHelper(longIndex->fixingDays(), basisSwapTenor, basisSwapQuote->quote(),
                                   boost::static_pointer_cast<OvernightIndex>(shortIndex), longIndex,
                                   discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>()));
            } else {
                // ...or Libor vs Libor?
                basisSwapHelper.reset(
                    new TenorBasisSwapHelper(basisSwapQuote->quote(), basisSwapTenor, longIndex, shortIndex,
                                             basisSwapConvention->shortPayTenor(),
                                             discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>(),
                                             basisSwapConvention->spreadOnShort(), basisSwapConvention->includeSpread(),
                                             basisSwapConvention->subPeriodsCouponType()));
            }
            instruments.push_back(basisSwapHelper);
        }
    }
}

void YieldCurve::addTenorBasisTwoSwaps(const boost::shared_ptr<YieldCurveSegment>& segment,
                                       vector<boost::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::TenorBasisTwoSwap,
               "Conventions ID does not give tenor basis two swap conventions.");
    boost::shared_ptr<TenorBasisTwoSwapConvention> basisSwapConvention =
        boost::dynamic_pointer_cast<TenorBasisTwoSwapConvention>(convention);

    boost::shared_ptr<TenorBasisYieldCurveSegment> basisSwapSegment =
        boost::dynamic_pointer_cast<TenorBasisYieldCurveSegment>(segment);

    // If short index projection curve ID is not this curve.
    string shortCurveID = basisSwapSegment->shortProjectionCurveID();
    boost::shared_ptr<IborIndex> shortIndex = basisSwapConvention->shortIndex();
    if (shortCurveID != curveConfig_->curveID() && !shortCurveID.empty()) {
        shortCurveID = yieldCurveKey(currency_, shortCurveID, asofDate_);
        boost::shared_ptr<YieldCurve> shortCurve;
        map<string, boost::shared_ptr<YieldCurve>>::iterator it;
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
    string longCurveID = basisSwapSegment->longProjectionCurveID();
    boost::shared_ptr<IborIndex> longIndex = basisSwapConvention->longIndex();
    if (longCurveID != curveConfig_->curveID() && !longCurveID.empty()) {
        longCurveID = yieldCurveKey(currency_, longCurveID, asofDate_);
        boost::shared_ptr<YieldCurve> longCurve;
        map<string, boost::shared_ptr<YieldCurve>>::iterator it;
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

    auto basisSwapQuoteIDs = basisSwapSegment->quotes();
    for (Size i = 0; i < basisSwapQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(basisSwapQuoteIDs[i], asofDate_);

        // Check that we have a valid basis swap quote
        boost::shared_ptr<BasisSwapQuote> basisSwapQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BASIS_SWAP,
                       "Market quote not of type basis swap.");
            basisSwapQuote = boost::dynamic_pointer_cast<BasisSwapQuote>(marketQuote);

            // Create a tenor basis swap helper if we do.
            Period basisSwapTenor = basisSwapQuote->maturity();
            boost::shared_ptr<RateHelper> basisSwapHelper(new BasisTwoSwapHelper(
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

void YieldCurve::addBMABasisSwaps(const boost::shared_ptr<YieldCurveSegment>& segment,
                                  vector<boost::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::BMABasisSwap,
               "Conventions ID does not give bma basis swap conventions.");
    boost::shared_ptr<BMABasisSwapConvention> bmaBasisSwapConvention =
        boost::dynamic_pointer_cast<BMABasisSwapConvention>(convention);

    boost::shared_ptr<SimpleYieldCurveSegment> bmaBasisSwapSegment =
        boost::dynamic_pointer_cast<SimpleYieldCurveSegment>(segment);
    QL_REQUIRE(bmaBasisSwapSegment, "BMA basis swap segment of " + curveSpec_.ccy() + "/" + curveSpec_.curveConfigID() +
                                        " did not successfully cast to a BMA basis swap yield curve segment!");

    // TODO: should be checking here whether or not the bma index is forwarding on this curve. either way, we make sure!
    boost::shared_ptr<BMAIndexWrapper> bmaIndex = bmaBasisSwapConvention->bmaIndex();
    bmaIndex = dynamic_pointer_cast<BMAIndexWrapper>(bmaIndex->clone(handle()));

    // If libor index projection curve ID is not this curve.
    string liborCurveID = bmaBasisSwapSegment->projectionCurveID();
    boost::shared_ptr<IborIndex> liborIndex = bmaBasisSwapConvention->liborIndex();
    liborCurveID = yieldCurveKey(currency_, liborCurveID, asofDate_);
    boost::shared_ptr<YieldCurve> liborCurve;
    map<string, boost::shared_ptr<YieldCurve>>::iterator it;
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

    auto bmaBasisSwapQuoteIDs = bmaBasisSwapSegment->quotes();
    for (Size i = 0; i < bmaBasisSwapQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(bmaBasisSwapQuoteIDs[i], asofDate_);

        // Check that we have a valid bma basis swap quote
        if (marketQuote) {
            boost::shared_ptr<BMASwapQuote> bmaBasisSwapQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BMA_SWAP,
                       "Market quote not of type bma swap.");
            QL_REQUIRE(marketQuote->quoteType() == MarketDatum::QuoteType::RATIO, "Market quote not of type ratio.");
            bmaBasisSwapQuote = boost::dynamic_pointer_cast<BMASwapQuote>(marketQuote);

            // Create bma basis swap helper if we do.
            boost::shared_ptr<RateHelper> bmaSwapHelper;
            bmaSwapHelper.reset(new BMASwapRateHelper(bmaBasisSwapQuote->quote(), bmaBasisSwapQuote->maturity(),
                                                      bmaIndex->fixingDays(), bmaIndex->fixingCalendar(),
                                                      bmaBasisSwapQuote->term(), bmaIndex->businessDayConvention(),
                                                      bmaIndex->dayCounter(), bmaIndex->bma(), liborIndex));
            instruments.push_back(bmaSwapHelper);
        }
    }
}

void YieldCurve::addFXForwards(const boost::shared_ptr<YieldCurveSegment>& segment,
                               vector<boost::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // LOG("YieldCurve::addFXForwards() called");
    // Get the conventions associated with the segment.
    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::FX, "Conventions ID does not give FX forward conventions.");

    // LOG("YieldCurve::addFXForwards(), get fxConvention");
    boost::shared_ptr<FXConvention> fxConvention = boost::dynamic_pointer_cast<FXConvention>(convention);

    // LOG("YieldCurve::addFXForwards(), cast to CrossCcyYieldCurveSegment");
    boost::shared_ptr<CrossCcyYieldCurveSegment> fxForwardSegment =
        boost::dynamic_pointer_cast<CrossCcyYieldCurveSegment>(segment);

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

    // LOG("YieldCurve::addFXForwards(), retrieve known discount curve");
    string knownDiscountID = fxForwardSegment->foreignDiscountCurveID();
    knownDiscountID = yieldCurveKey(knownCurrency, knownDiscountID, asofDate_);
    boost::shared_ptr<YieldCurve> knownDiscountCurve;
    auto it = requiredYieldCurves_.find(knownDiscountID);
    if (it != requiredYieldCurves_.end()) {
        knownDiscountCurve = it->second;
    } else {
        QL_FAIL("The foreign discount curve, " << knownDiscountID
                                               << ", required in the building "
                                                  "of the curve, "
                                               << curveSpec_.name() << ", was not found.");
    }

    /* Need to retrieve the market FX spot rate */
    // LOG("YieldCurve::addFXForwards(), retrieve FX rate");
    string spotRateID = fxForwardSegment->spotRateID();
    boost::shared_ptr<FXSpotQuote> fxSpotQuote = getFxSpotQuote(spotRateID);

    /* Create an FX spot quote from the retrieved FX spot rate */
    Currency fxSpotSourceCcy = parseCurrency(fxSpotQuote->unitCcy());
    Currency fxSpotTargetCcy = parseCurrency(fxSpotQuote->ccy());

    LOG("YieldCurve::addFXForwards(), create FX forward quotes and helpers");
    auto fxForwardQuoteIDs = fxForwardSegment->quotes();
    for (Size i = 0; i < fxForwardQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(fxForwardQuoteIDs[i], asofDate_);

        // Check that we have a valid FX forward quote
        if (marketQuote) {
            boost::shared_ptr<FXForwardQuote> fxForwardQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::FX_FWD,
                       "Market quote not of type FX forward.");
            fxForwardQuote = boost::dynamic_pointer_cast<FXForwardQuote>(marketQuote);

            QL_REQUIRE(fxSpotQuote->unitCcy() == fxForwardQuote->unitCcy() &&
                           fxSpotQuote->ccy() == fxForwardQuote->ccy(),
                       "Currency mismatch between spot \"" << spotRateID << "\" and fwd \""
                                                           << fxForwardQuoteIDs[i].first << "\"");

            // QL expects the FX Fwd quote to be per spot, not points.
            Handle<Quote> qlFXForwardQuote(
                boost::make_shared<SimpleQuote>(fxForwardQuote->quote()->value() / fxConvention->pointsFactor()));

            // Create an FX forward helper
            Period fxForwardTenor = fxForwardQuote->term();
            bool endOfMonth = false;
            bool isFxBaseCurrencyCollateralCurrency = knownCurrency == fxSpotSourceCcy;

            // TODO: spotRelative

            // the fx swap rate helper interprets the fxSpot as of the spot date, our fx spot here
            // is as of today, therefore we set up the fx spot helper with zero settlement days
            // and compute the tenor such that the correct maturity date is still matched
            Date spotDate = fxConvention->advanceCalendar().advance(asofDate_, fxConvention->spotDays() * Days);
            Date endDate = fxConvention->advanceCalendar().advance(spotDate, fxForwardTenor);

            boost::shared_ptr<RateHelper> fxForwardHelper(new FxSwapRateHelper(
                qlFXForwardQuote, fxSpotQuote->quote(), (endDate - asofDate_) * Days, 0, NullCalendar(), Unadjusted,
                endOfMonth, isFxBaseCurrencyCollateralCurrency, knownDiscountCurve->handle()));

            instruments.push_back(fxForwardHelper);
        }
    }

    LOG("YieldCurve::addFXForwards() done");
}

void YieldCurve::addCrossCcyBasisSwaps(const boost::shared_ptr<YieldCurveSegment>& segment,
                                       vector<boost::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment.
    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::CrossCcyBasis, "Conventions ID does not give cross currency "
                                                                      "basis swap conventions.");
    boost::shared_ptr<CrossCcyBasisSwapConvention> basisSwapConvention =
        boost::dynamic_pointer_cast<CrossCcyBasisSwapConvention>(convention);

    /* Is this yield curve on the flat side or spread side */
    bool onFlatSide = (currency_ == basisSwapConvention->flatIndex()->currency());

    boost::shared_ptr<CrossCcyYieldCurveSegment> basisSwapSegment =
        boost::dynamic_pointer_cast<CrossCcyYieldCurveSegment>(segment);

    /* Need to retrieve the market FX spot rate */
    string spotRateID = basisSwapSegment->spotRateID();
    boost::shared_ptr<FXSpotQuote> fxSpotQuote = getFxSpotQuote(spotRateID);

    /* Create an FX spot quote from the retrieved FX spot rate */
    Currency fxSpotSourceCcy = parseCurrency(fxSpotQuote->unitCcy());
    Currency fxSpotTargetCcy = parseCurrency(fxSpotQuote->ccy());

    /* Need to retrieve the discount curve in the other (foreign) currency. */
    string foreignDiscountID = basisSwapSegment->foreignDiscountCurveID();
    Currency foreignCcy = fxSpotSourceCcy == currency_ ? fxSpotTargetCcy : fxSpotSourceCcy;
    foreignDiscountID = yieldCurveKey(foreignCcy, foreignDiscountID, asofDate_);
    boost::shared_ptr<YieldCurve> foreignDiscountCurve;
    auto it = requiredYieldCurves_.find(foreignDiscountID);
    if (it != requiredYieldCurves_.end()) {
        foreignDiscountCurve = it->second;
    } else {
        QL_FAIL("The foreign discount curve, " << foreignDiscountID
                                               << ", required in the building "
                                                  "of the curve, "
                                               << curveSpec_.name() << ", was not found.");
    }

    /* Need to retrieve the foreign projection curve in the other currency. If its ID is empty,
       set it equal to the foreign discount curve. */
    string foreignProjectionCurveID = basisSwapSegment->foreignProjectionCurveID();
    boost::shared_ptr<IborIndex> foreignIndex =
        onFlatSide ? basisSwapConvention->spreadIndex() : basisSwapConvention->flatIndex();
    if (foreignProjectionCurveID.empty()) {
        foreignIndex = foreignIndex->clone(foreignDiscountCurve->handle());
    } else {
        foreignProjectionCurveID = yieldCurveKey(foreignCcy, foreignProjectionCurveID, asofDate_);
        boost::shared_ptr<YieldCurve> foreignProjectionCurve;
        map<string, boost::shared_ptr<YieldCurve>>::iterator it;
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
    boost::shared_ptr<IborIndex> domesticIndex =
        onFlatSide ? basisSwapConvention->flatIndex() : basisSwapConvention->spreadIndex();
    if (domesticProjectionCurveID != curveConfig_->curveID() && !domesticProjectionCurveID.empty()) {
        domesticProjectionCurveID = yieldCurveKey(currency_, domesticProjectionCurveID, asofDate_);
        boost::shared_ptr<YieldCurve> domesticProjectionCurve;
        map<string, boost::shared_ptr<YieldCurve>>::iterator it;
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
    boost::shared_ptr<IborIndex> flatIndex;
    boost::shared_ptr<IborIndex> spreadIndex;
    if (onFlatSide) {
        if (discountCurve_) {
            flatDiscountCurve.linkTo(*discountCurve_->handle());
        }
        spreadDiscountCurve.linkTo(*foreignDiscountCurve->handle());
        flatIndex = domesticIndex;
        spreadIndex = foreignIndex;
    } else {
        flatDiscountCurve.linkTo(*foreignDiscountCurve->handle());
        if (discountCurve_) {
            spreadDiscountCurve.linkTo(*discountCurve_->handle());
        }
        flatIndex = foreignIndex;
        spreadIndex = domesticIndex;
    }

    Period flatTenor = basisSwapConvention->flatTenor();
    Period spreadTenor = basisSwapConvention->spreadTenor();

    auto basisSwapQuoteIDs = basisSwapSegment->quotes();
    for (Size i = 0; i < basisSwapQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(basisSwapQuoteIDs[i], asofDate_);

        // Check that we have a valid basis swap quote
        if (marketQuote) {
            boost::shared_ptr<CrossCcyBasisSwapQuote> basisSwapQuote;
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::CC_BASIS_SWAP,
                       "Market quote not of type cross "
                       "currency basis swap.");
            basisSwapQuote = boost::dynamic_pointer_cast<CrossCcyBasisSwapQuote>(marketQuote);

            // Create a cross currency basis swap helper if we do.
            Period basisSwapTenor = basisSwapQuote->maturity();
            bool isResettableSwap = basisSwapConvention->isResettable();
            if (!isResettableSwap) {
                boost::shared_ptr<RateHelper> basisSwapHelper(new CrossCcyBasisSwapHelper(
                    basisSwapQuote->quote(), fxSpotQuote->quote(), basisSwapConvention->settlementDays(),
                    basisSwapConvention->settlementCalendar(), basisSwapTenor, basisSwapConvention->rollConvention(),
                    flatIndex, spreadIndex, flatDiscountCurve, spreadDiscountCurve, basisSwapConvention->eom(),
                    flatIndex->currency().code() != fxSpotQuote->unitCcy(), flatTenor, spreadTenor));
                instruments.push_back(basisSwapHelper);
            } else { // the quote is for a cross currency basis swap with a resetting notional
                bool resetsOnFlatLeg = basisSwapConvention->flatIndexIsResettable();
                // the convention here is to call the resetting leg the "domestic leg",
                // and the constant notional leg the "foreign leg"
                bool spreadOnForeignCcy = resetsOnFlatLeg ? true : false;
                boost::shared_ptr<IborIndex> foreignIndex = resetsOnFlatLeg ? spreadIndex : flatIndex;
                Handle<YieldTermStructure> foreignDiscount = resetsOnFlatLeg ? spreadDiscountCurve : flatDiscountCurve;
                boost::shared_ptr<IborIndex> domesticIndex = resetsOnFlatLeg ? flatIndex : spreadIndex;
                Handle<YieldTermStructure> domesticDiscount = resetsOnFlatLeg ? flatDiscountCurve : spreadDiscountCurve;
                bool invertFxQuote = (foreignIndex->currency().code() !=
                                      fxSpotQuote->unitCcy()); // set to true if the spotFXQuote is DOM/FOR
                Period foreignTenor = resetsOnFlatLeg ? spreadTenor : flatTenor;
                Period domesticTenor = resetsOnFlatLeg ? flatTenor : spreadTenor;

                // Use foreign and dom discount curves for projecting FX forward rates (for e.g. resetting cashflows)
                boost::shared_ptr<RateHelper> basisSwapHelper(new CrossCcyBasisMtMResetSwapHelper(
                    basisSwapQuote->quote(), fxSpotQuote->quote(), basisSwapConvention->settlementDays(),
                    basisSwapConvention->settlementCalendar(), basisSwapTenor, basisSwapConvention->rollConvention(),
                    foreignIndex, domesticIndex, foreignDiscount, domesticDiscount, Handle<YieldTermStructure>(),
                    Handle<YieldTermStructure>(), basisSwapConvention->eom(), spreadOnForeignCcy, invertFxQuote,
                    foreignTenor, domesticTenor));
                instruments.push_back(basisSwapHelper);
            }
        }
    }
}
void YieldCurve::addCrossCcyFixFloatSwaps(const boost::shared_ptr<YieldCurveSegment>& segment,
                                          vector<boost::shared_ptr<RateHelper>>& instruments) {

    DLOG("Adding Segment " << segment->typeID() << " with conventions \"" << segment->conventionsID() << "\"");

    // Get the conventions associated with the segment
    boost::shared_ptr<Convention> convention = conventions_.get(segment->conventionsID());
    QL_REQUIRE(convention, "No conventions found with ID: " << segment->conventionsID());
    QL_REQUIRE(convention->type() == Convention::Type::CrossCcyFixFloat,
               "Conventions ID does not give cross currency fix float swap conventions.");
    boost::shared_ptr<CrossCcyFixFloatSwapConvention> swapConvention =
        boost::dynamic_pointer_cast<CrossCcyFixFloatSwapConvention>(convention);

    QL_REQUIRE(swapConvention->fixedCurrency() == currency_,
               "The yield curve currency must "
                   << "equal the cross currency fix float swap's fixed leg currency");

    // Cast the segment
    boost::shared_ptr<CrossCcyYieldCurveSegment> swapSegment =
        boost::dynamic_pointer_cast<CrossCcyYieldCurveSegment>(segment);

    // Retrieve the discount curve on the float leg
    boost::shared_ptr<IborIndex> floatIndex = swapConvention->index();
    Currency floatLegCcy = floatIndex->currency();
    string floatLegDiscId = yieldCurveKey(floatLegCcy, swapSegment->foreignDiscountCurveID(), asofDate_);
    auto it = requiredYieldCurves_.find(floatLegDiscId);
    QL_REQUIRE(it != requiredYieldCurves_.end(), "The discount curve " << floatLegDiscId
                                                                       << " required in the building of curve "
                                                                       << curveSpec_.name() << " was not found.");
    Handle<YieldTermStructure> floatLegDisc = it->second->handle();

    // Retrieve the projection curve on the float leg. If empty, use discount curve.
    string floatLegProjId = swapSegment->foreignProjectionCurveID();
    if (floatLegProjId.empty()) {
        floatIndex = floatIndex->clone(floatLegDisc);
    } else {
        floatLegProjId = yieldCurveKey(floatLegCcy, floatLegProjId, asofDate_);
        it = requiredYieldCurves_.find(floatLegProjId);
        QL_REQUIRE(it != requiredYieldCurves_.end(), "The projection curve " << floatLegProjId
                                                                             << " required in the building of curve "
                                                                             << curveSpec_.name() << " was not found.");
        floatIndex = floatIndex->clone(it->second->handle());
    }

    // Create the FX spot quote for the helper. The quote needs to be number of units of fixed leg
    // currency for 1 unit of float leg currency. We convert the market quote here if needed.
    string fxSpotId = swapSegment->spotRateID();
    boost::shared_ptr<FXSpotQuote> fxSpotMd = getFxSpotQuote(fxSpotId);
    Currency mdUnitCcy = parseCurrency(fxSpotMd->unitCcy());
    Currency mdCcy = parseCurrency(fxSpotMd->ccy());
    Handle<Quote> fxSpotQuote;
    if (mdUnitCcy == floatLegCcy && mdCcy == currency_) {
        fxSpotQuote = fxSpotMd->quote();
    } else if (mdUnitCcy == currency_ && mdCcy == floatLegCcy) {
        fxSpotQuote = Handle<Quote>(boost::make_shared<SimpleQuote>(1.0 / fxSpotMd->quote()->value()));
    } else {
        QL_FAIL("The FX spot market quote " << mdUnitCcy << "/" << mdCcy << " cannot be used "
                                            << "in the building of the curve " << curveSpec_.name() << ".");
    }

    // Create the helpers
    auto quoteIds = swapSegment->quotes();
    for (Size i = 0; i < quoteIds.size(); i++) {

        // Throws if quote not found
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(quoteIds[i], asofDate_);

        // Check that we have a valid basis swap quote
        if (marketQuote) {
            boost::shared_ptr<CrossCcyFixFloatSwapQuote> swapQuote =
                boost::dynamic_pointer_cast<CrossCcyFixFloatSwapQuote>(marketQuote);
            QL_REQUIRE(swapQuote, "Market quote should be of type 'CrossCcyFixFloatSwapQuote'");

            // Create the helper
            boost::shared_ptr<RateHelper> helper = boost::make_shared<CrossCcyFixFloatSwapHelper>(
                swapQuote->quote(), fxSpotQuote, swapConvention->settlementDays(), swapConvention->settlementCalendar(),
                swapConvention->settlementConvention(), swapQuote->maturity(), currency_,
                swapConvention->fixedFrequency(), swapConvention->fixedConvention(), swapConvention->fixedDayCounter(),
                floatIndex, floatLegDisc, Handle<Quote>(), swapConvention->eom());

            instruments.push_back(helper);
        }
    }
}

boost::shared_ptr<FXSpotQuote> YieldCurve::getFxSpotQuote(string spotId) {
    // check the spot id, if like FX/RATE/CCY/CCY we go straight to the loader first
    std::vector<string> tokens;
    split(tokens, spotId, boost::is_any_of("/"));

    boost::shared_ptr<FXSpotQuote> fxSpotQuote;
    if (tokens.size() == 4 && tokens[0] == "FX" && tokens[1] == "RATE") {
        if (loader_.has(spotId, asofDate_)) {
            boost::shared_ptr<MarketDatum> fxSpotMarketQuote = loader_.get(spotId, asofDate_);

            if (fxSpotMarketQuote) {
                QL_REQUIRE(fxSpotMarketQuote->instrumentType() == MarketDatum::InstrumentType::FX_SPOT,
                           "Market quote not of type FX spot.");
                fxSpotQuote = boost::dynamic_pointer_cast<FXSpotQuote>(fxSpotMarketQuote);
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
        boost::make_shared<FXSpotQuote>(spot->value(), asofDate_, spotId, MarketDatum::QuoteType::RATE, unitCcy, ccy);
    return fxSpotQuote;
}

// Get Pillar Dates
// we have to try to cast and then call dates() on the subclasses, a bit messy
template <class T> inline void getPillarDates(const boost::shared_ptr<YieldTermStructure>& p, vector<Date>& d) {
    if (d.size() == 0) {
        boost::shared_ptr<T> ptr = boost::dynamic_pointer_cast<T>(p);
        if (ptr)
            d = ptr->dates();
    }
}

vector<Date> pillarDates(const Handle<YieldTermStructure>& h) {
    const boost::shared_ptr<YieldTermStructure>& p = h.currentLink();
    vector<Date> d;

    getPillarDates<InterpolatedDiscountCurve<QuantLib::Linear>>(p, d);
    getPillarDates<InterpolatedDiscountCurve<QuantLib::LogLinear>>(p, d);
    getPillarDates<InterpolatedDiscountCurve<QuantLib::Cubic>>(p, d);
    getPillarDates<InterpolatedDiscountCurve<QuantLib::ConvexMonotone>>(p, d);
    getPillarDates<InterpolatedForwardCurve<QuantLib::Linear>>(p, d);
    getPillarDates<InterpolatedForwardCurve<QuantLib::LogLinear>>(p, d);
    getPillarDates<InterpolatedForwardCurve<QuantLib::Cubic>>(p, d);
    getPillarDates<InterpolatedForwardCurve<QuantLib::ConvexMonotone>>(p, d);
    getPillarDates<InterpolatedZeroCurve<QuantLib::Linear>>(p, d);
    getPillarDates<InterpolatedZeroCurve<QuantLib::LogLinear>>(p, d);
    getPillarDates<InterpolatedZeroCurve<QuantLib::Cubic>>(p, d);
    getPillarDates<InterpolatedZeroCurve<QuantLib::ConvexMonotone>>(p, d);
    getPillarDates<PiecewiseYieldCurve<ZeroYield, QuantLib::Linear, QuantExt::IterativeBootstrap>>(p, d);
    getPillarDates<PiecewiseYieldCurve<ZeroYield, QuantLib::LogLinear, QuantExt::IterativeBootstrap>>(p, d);
    getPillarDates<PiecewiseYieldCurve<ZeroYield, Cubic, QuantExt::IterativeBootstrap>>(p, d);
    getPillarDates<PiecewiseYieldCurve<ZeroYield, ConvexMonotone, QuantExt::IterativeBootstrap>>(p, d);
    getPillarDates<PiecewiseYieldCurve<QuantLib::Discount, QuantLib::Linear, QuantExt::IterativeBootstrap>>(p, d);
    getPillarDates<PiecewiseYieldCurve<QuantLib::Discount, QuantLib::LogLinear, QuantExt::IterativeBootstrap>>(p, d);
    getPillarDates<PiecewiseYieldCurve<QuantLib::Discount, Cubic, QuantExt::IterativeBootstrap>>(p, d);
    getPillarDates<PiecewiseYieldCurve<QuantLib::Discount, ConvexMonotone, QuantExt::IterativeBootstrap>>(p, d);
    getPillarDates<PiecewiseYieldCurve<QuantLib::ForwardRate, QuantLib::Linear, QuantExt::IterativeBootstrap>>(p, d);
    getPillarDates<PiecewiseYieldCurve<QuantLib::ForwardRate, QuantLib::LogLinear, QuantExt::IterativeBootstrap>>(p, d);
    getPillarDates<PiecewiseYieldCurve<QuantLib::ForwardRate, Cubic, QuantExt::IterativeBootstrap>>(p, d);
    getPillarDates<PiecewiseYieldCurve<QuantLib::ForwardRate, ConvexMonotone, QuantExt::IterativeBootstrap>>(p, d);

    return d;
}

} // namespace data
} // namespace ore
