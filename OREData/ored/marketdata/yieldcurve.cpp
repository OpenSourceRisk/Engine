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

#include <ql/time/daycounters/actualactual.hpp>
#include <ql/termstructures/yield/piecewisezerospreadedtermstructure.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/currencies/exchangeratemanager.hpp>
#include <ql/time/imm.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/indexes/ibor/usdlibor.hpp>

#include <qle/termstructures/crossccybasisswaphelper.hpp>
#include <qle/termstructures/oisratehelper.hpp>
#include <qle/termstructures/averageoisratehelper.hpp>
#include <qle/termstructures/tenorbasisswaphelper.hpp>
#include <qle/termstructures/basistwoswaphelper.hpp>
#include <ql/indexes/ibor/all.hpp>

#include <ored/marketdata/yieldcurve.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/indexparser.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace {
/* Helper function to return the key required to look up the map in the YieldCurve ctor */
string yieldCurveKey(const Currency& curveCcy, const string& curveID, const Date&) {
    ore::data::YieldCurveSpec tempSpec(curveCcy.code(), curveID);
    return tempSpec.name();
}
}

namespace ore {
namespace data {

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
    else
        QL_FAIL("Yield curve interpolation method " << s << " not recognized");
};

YieldCurve::InterpolationVariable parseYieldCurveInterpolationVariable(const string& s) {
    if (s == "Zero")
        return YieldCurve::InterpolationVariable::Zero;
    else if (s == "Discount")
        return YieldCurve::InterpolationVariable::Discount;
    else
        QL_FAIL("Yield curve interpolation variable " << s << " not recognized");
};

YieldCurve::YieldCurve(Date asof, YieldCurveSpec curveSpec, const CurveConfigurations& curveConfigs,
                       const Loader& loader, const Conventions& conventions,
                       const map<string, boost::shared_ptr<YieldCurve>>& requiredYieldCurves)
    : asofDate_(asof), curveSpec_(curveSpec), loader_(loader), conventions_(conventions),
      requiredYieldCurves_(requiredYieldCurves) {

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
                QL_FAIL("The discount curve, " << discountCurveID << ", required in the building "
                                                                     "of the curve, " << curveSpec_.name()
                                               << ", was not found.");
            }
        }

        curveSegments_ = curveConfig_->curveSegments();
        interpolationMethod_ = parseYieldCurveInterpolationMethod(curveConfig_->interpolationMethod());
        interpolationVariable_ = parseYieldCurveInterpolationVariable(curveConfig_->interpolationVariable());
        zeroDayCounter_ = parseDayCounter(curveConfig_->zeroDayCounter());
        accuracy_ = curveConfig_->tolerance();
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
        } else {
            DLOG("Bootstrapping YieldCurve " << curveSpec_);
            buildBootstrappedCurve();
        }

        h_.linkTo(p_);
        if (extrapolation_) {
            h_->enableExtrapolation();
        }
    } catch (QuantLib::Error& e) {
        QL_FAIL("yield curve building failed :" << e.what());
    } catch (std::exception& e) {
        QL_FAIL(e.what());
    } catch (...) {
        QL_FAIL("unknown error");
    }

    LOG("Yield curve " << curveSpec_.name() << " built");
}

boost::shared_ptr<YieldTermStructure>
YieldCurve::piecewisecurve(const vector<boost::shared_ptr<RateHelper>>& instruments) {

    boost::shared_ptr<YieldTermStructure> yieldts;
    switch (interpolationVariable_) {
    case InterpolationVariable::Zero:
        switch (interpolationMethod_) {
        case InterpolationMethod::Linear:
            yieldts.reset(new PiecewiseYieldCurve<ZeroYield, QuantLib::Linear>(asofDate_, instruments, zeroDayCounter_,
                                                                               accuracy_));
            break;
        case InterpolationMethod::LogLinear:
            yieldts.reset(new PiecewiseYieldCurve<ZeroYield, QuantLib::LogLinear>(asofDate_, instruments,
                                                                                  zeroDayCounter_, accuracy_));
            break;
        case InterpolationMethod::NaturalCubic:
            yieldts.reset(new PiecewiseYieldCurve<ZeroYield, Cubic>(asofDate_, instruments, zeroDayCounter_, accuracy_,
                                                                    Cubic(CubicInterpolation::Kruger, true)));
            break;
        case InterpolationMethod::FinancialCubic:
            yieldts.reset(new PiecewiseYieldCurve<ZeroYield, Cubic>(asofDate_, instruments, zeroDayCounter_, accuracy_,
                                                                    Cubic(CubicInterpolation::Kruger, true,
                                                                          CubicInterpolation::SecondDerivative, 0.0,
                                                                          CubicInterpolation::FirstDerivative)));
            break;
        default:
            QL_FAIL("Interpolation method not recognised.");
        }
        break;
    case InterpolationVariable::Discount:
        switch (interpolationMethod_) {
        case InterpolationMethod::Linear:
            yieldts.reset(new PiecewiseYieldCurve<QuantLib::Discount, QuantLib::Linear>(asofDate_, instruments,
                                                                                        zeroDayCounter_, accuracy_));
            break;
        case InterpolationMethod::LogLinear:
            yieldts.reset(new PiecewiseYieldCurve<QuantLib::Discount, QuantLib::LogLinear>(asofDate_, instruments,
                                                                                           zeroDayCounter_, accuracy_));
            break;
        case InterpolationMethod::NaturalCubic:
            yieldts.reset(new PiecewiseYieldCurve<QuantLib::Discount, Cubic>(
                asofDate_, instruments, zeroDayCounter_, accuracy_, Cubic(CubicInterpolation::Kruger, true)));
            break;
        case InterpolationMethod::FinancialCubic:
            yieldts.reset(new PiecewiseYieldCurve<QuantLib::Discount, Cubic>(
                asofDate_, instruments, zeroDayCounter_, accuracy_,
                Cubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                      CubicInterpolation::FirstDerivative)));
            break;
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
    for (Size i = 0; i < instruments.size(); i++) {
        dates[i + 1] = instruments[i]->latestDate();
        zeros[i + 1] = yieldts->zeroRate(dates[i + 1], zeroDayCounter_, Continuous);
        discounts[i + 1] = yieldts->discount(dates[i + 1]);
    }
    zeros[0] = zeros[1];
    if (interpolationVariable_ == InterpolationVariable::Zero)
        p_ = zerocurve(dates, zeros, zeroDayCounter_);
    else if (interpolationVariable_ == InterpolationVariable::Discount)
        p_ = discountcurve(dates, discounts, zeroDayCounter_);
    else
        QL_FAIL("Interpolation variable not recognised.");

    return p_;
}

boost::shared_ptr<YieldTermStructure> YieldCurve::zerocurve(const vector<Date>& dates, const vector<Rate>& yields,
                                                            const DayCounter& dayCounter) {

    boost::shared_ptr<YieldTermStructure> yieldts;
    switch (interpolationMethod_) {
    case InterpolationMethod::Linear:
        yieldts.reset(new InterpolatedZeroCurve<QuantLib::Linear>(dates, yields, dayCounter, QuantLib::Linear()));
        break;
    case InterpolationMethod::LogLinear:
        yieldts.reset(new InterpolatedZeroCurve<QuantLib::LogLinear>(dates, yields, dayCounter, QuantLib::LogLinear()));
        break;
    case InterpolationMethod::NaturalCubic:
        yieldts.reset(new InterpolatedZeroCurve<QuantLib::Cubic>(dates, yields, dayCounter,
                                                                 QuantLib::Cubic(CubicInterpolation::Kruger, true)));
        break;
    case InterpolationMethod::FinancialCubic:
        yieldts.reset(new InterpolatedZeroCurve<QuantLib::Cubic>(
            dates, yields, dayCounter,
            QuantLib::Cubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                            CubicInterpolation::FirstDerivative)));
        break;
    default:
        QL_FAIL("Interpolation method not recognised.");
    }
    return yieldts;
}

boost::shared_ptr<YieldTermStructure>
YieldCurve::discountcurve(const vector<Date>& dates, const vector<DiscountFactor>& dfs, const DayCounter& dayCounter) {

    boost::shared_ptr<YieldTermStructure> yieldts;
    switch (interpolationMethod_) {
    case InterpolationMethod::Linear:
        yieldts.reset(new InterpolatedDiscountCurve<QuantLib::Linear>(dates, dfs, dayCounter, QuantLib::Linear()));
        break;
    case InterpolationMethod::LogLinear:
        yieldts.reset(
            new InterpolatedDiscountCurve<QuantLib::LogLinear>(dates, dfs, dayCounter, QuantLib::LogLinear()));
        break;
    case InterpolationMethod::NaturalCubic:
        yieldts.reset(new InterpolatedDiscountCurve<QuantLib::Cubic>(
            dates, dfs, dayCounter, QuantLib::Cubic(CubicInterpolation::Kruger, true)));
        break;
    case InterpolationMethod::FinancialCubic:
        yieldts.reset(new InterpolatedDiscountCurve<QuantLib::Cubic>(
            dates, dfs, dayCounter,
            QuantLib::Cubic(CubicInterpolation::Kruger, true, CubicInterpolation::SecondDerivative, 0.0,
                            CubicInterpolation::FirstDerivative)));
        break;
    default:
        QL_FAIL("Interpolation method not recognised.");
    }
    return yieldts;
}

void YieldCurve::buildZeroCurve() {

    QL_REQUIRE(curveSegments_.size() <= 1, "More than one zero curve "
                                           "segment not supported yet.");
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::Zero, "The curve segment is not of type Zero.");

    // Fill a vector of zero quotes.
    vector<boost::shared_ptr<ZeroQuote>> zeroQuotes;
    boost::shared_ptr<DirectYieldCurveSegment> zeroCurveSegment =
        boost::dynamic_pointer_cast<DirectYieldCurveSegment>(curveSegments_[0]);
    vector<string> zeroQuoteIDs = zeroCurveSegment->quotes();

    for (Size i = 0; i < zeroQuoteIDs.size(); ++i) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(zeroQuoteIDs[i], asofDate_);
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::ZERO,
                       "Market quote not of type zero.");
            boost::shared_ptr<ZeroQuote> zeroQuote = boost::dynamic_pointer_cast<ZeroQuote>(marketQuote);
            zeroQuotes.push_back(zeroQuote);
        } else {
            QL_FAIL("Could not find quote for ID " << zeroQuoteIDs[i] << " with as of date " << io::iso_date(asofDate_)
                                                   << ".");
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
        boost::shared_ptr<YieldTermStructure> tempCurve = zerocurve(dates, zeroes, quoteDayCounter);
        zeroes.clear();
        for (Size i = 0; i < dates.size(); ++i) {
            Rate zero = tempCurve->zeroRate(dates[i], zeroDayCounter_, Continuous);
            zeroes.push_back(zero);
        }
        p_ = zerocurve(dates, zeroes, zeroDayCounter_);
    } else if (interpolationVariable_ == YieldCurve::InterpolationVariable::Discount) {
        boost::shared_ptr<YieldTermStructure> tempCurve = discountcurve(dates, discounts, quoteDayCounter);
        discounts.clear();
        for (Size i = 0; i < dates.size(); ++i) {
            DiscountFactor discount = tempCurve->discount(dates[i]);
            discounts.push_back(discount);
        }
        p_ = discountcurve(dates, discounts, zeroDayCounter_);
    } else {
        QL_FAIL("Unknown yield curve interpolation variable.");
    }
}

void YieldCurve::buildZeroSpreadedCurve() {
    QL_REQUIRE(curveSegments_.size() <= 1, "More than one zero spreaded curve "
                                           "segment not supported yet.");
    QL_REQUIRE(curveSegments_[0]->type() == YieldCurveSegment::Type::ZeroSpread,
               "The curve segment is not of type Zero.");

    // Fill a vector of zero spread quotes.
    vector<boost::shared_ptr<ZeroQuote>> quotes;
    boost::shared_ptr<ZeroSpreadedYieldCurveSegment> segment =
        boost::dynamic_pointer_cast<ZeroSpreadedYieldCurveSegment>(curveSegments_[0]);
    vector<string> quoteIDs = segment->quotes();

    Date today = Settings::instance().evaluationDate();
    vector<Date> dates(quoteIDs.size());
    vector<Handle<Quote>> quoteHandles(quoteIDs.size());
    for (Size i = 0; i < quoteIDs.size(); ++i) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(quoteIDs[i], asofDate_);
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::ZERO,
                       "Market quote not of type zero.");
            QL_REQUIRE(marketQuote->quoteType() == MarketDatum::QuoteType::YIELD_SPREAD,
                       "Market quote not of type yield spread.");
            boost::shared_ptr<ZeroQuote> zeroQuote = boost::dynamic_pointer_cast<ZeroQuote>(marketQuote);
            quotes.push_back(zeroQuote);
            dates[i] = zeroQuote->tenorBased() ? today + zeroQuote->tenor() : zeroQuote->date();
            quoteHandles[i] = zeroQuote->quote();
        } else {
            QL_FAIL("Could not find quote for ID " << quoteIDs[i] << " with as of date " << io::iso_date(asofDate_)
                                                   << ".");
        }
    }

    string referenceCurveID = segment->referenceCurveID();
    boost::shared_ptr<YieldCurve> referenceCurve;
    if (referenceCurveID != curveConfig_->curveID() && !referenceCurveID.empty()) {
        referenceCurveID = yieldCurveKey(currency_, referenceCurveID, asofDate_);
        map<string, boost::shared_ptr<YieldCurve>>::iterator it;
        it = requiredYieldCurves_.find(referenceCurveID);
        if (it != requiredYieldCurves_.end()) {
            referenceCurve = it->second;
        } else {
            QL_FAIL("The reference curve, " << referenceCurveID << ", required in the building "
                                                                   "of the curve, " << curveSpec_.name()
                                            << ", was not found.");
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
    vector<string> discountQuoteIDs = discountCurveSegment->quotes();

    for (Size i = 0; i < discountQuoteIDs.size(); ++i) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(discountQuoteIDs[i], asofDate_);
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::DISCOUNT,
                       "Market quote not of type Discount.");
            boost::shared_ptr<DiscountQuote> discountQuote = boost::dynamic_pointer_cast<DiscountQuote>(marketQuote);
            data[discountQuote->date()] = discountQuote->quote()->value();
        } else {
            QL_FAIL("Could not find quote for ID " << discountQuoteIDs[i] << " with as of date "
                                                   << io::iso_date(asofDate_) << ".");
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

    boost::shared_ptr<YieldTermStructure> tempDiscCurve = discountcurve(dates, discounts, zeroDayCounter_);

    // Now build curve with requested conventions
    if (interpolationVariable_ == YieldCurve::InterpolationVariable::Discount) {
        p_ = tempDiscCurve;
    } else if (interpolationVariable_ == YieldCurve::InterpolationVariable::Zero) {
        vector<Rate> zeroes;
        for (Size i = 0; i < dates.size(); ++i) {
            Rate zero = tempDiscCurve->zeroRate(dates[i], zeroDayCounter_, Continuous);
            zeroes.push_back(zero);
        }
        p_ = zerocurve(dates, zeroes, zeroDayCounter_);
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
        case YieldCurveSegment::Type::FXForward:
            addFXForwards(curveSegments_[i], instruments);
            break;
        case YieldCurveSegment::Type::CrossCcyBasis:
            addCrossCcyBasisSwaps(curveSegments_[i], instruments);
            break;
        default:
            QL_FAIL("Yield curve segment type not recognized.");
            break;
        }
    }

    DLOG("Bootstrapping with " << instruments.size() << " instruments");

    /* Build the bootstrapped curve from the instruments. */
    QL_REQUIRE(instruments.size() > 0, "Empty instrument list for date = " << io::iso_date(asofDate_)
                                                                           << " and curve = " << curveSpec_.name());
    p_ = piecewisecurve(instruments);
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
    vector<string> depositQuoteIDs = depositSegment->quotes();

    for (Size i = 0; i < depositQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(depositQuoteIDs[i], asofDate_);

        // Check that we have a valid deposit quote
        boost::shared_ptr<MoneyMarketQuote> depositQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::MM,
                       "Market quote not of type Deposit.");
            depositQuote = boost::dynamic_pointer_cast<MoneyMarketQuote>(marketQuote);
        } else {
            QL_FAIL("Could not find quote for ID " << depositQuoteIDs[i] << " with as of date "
                                                   << io::iso_date(asofDate_) << ".");
        }

        // Create a deposit helper if we do.
        boost::shared_ptr<RateHelper> depositHelper;
        Period depositTerm = depositQuote->term();
        Period fwdStart = depositQuote->fwdStart();
        Natural fwdStartDays = static_cast<Natural>(fwdStart.length());
        Handle<Quote> hQuote(depositQuote->quote());
        if (depositConvention->indexBased()) {
            string indexName;
            boost::shared_ptr<IborIndex> index;
            if (depositTerm.units() == Days) {
                // TODO: what is this about?
                /* To avoid problems constructing daily tenor indices. This works fine for overnight
                   indices also since the last token, i.e. 1W, is ignored in the IborParser */
                indexName = depositConvention->index() + "-1D";
                try {
                    index = parseIborIndex(indexName);
                } catch (...) {
                    indexName = depositConvention->index() + "-1W";
                    index = parseIborIndex(indexName);
                }
                depositHelper.reset(new DepositRateHelper(hQuote, depositTerm, fwdStartDays, index->fixingCalendar(),
                                                          index->businessDayConvention(), index->endOfMonth(),
                                                          index->dayCounter()));
            } else {
                stringstream ss;
                ss << depositConvention->index() << "-" << io::short_period(depositTerm);
                indexName = ss.str();
                index = parseIborIndex(indexName);
                depositHelper.reset(new DepositRateHelper(hQuote, index));
            }
        } else {
            QL_REQUIRE(fwdStart.units() == Days, "The forward start time unit for deposits "
                                                 "must be expressed in days.");
            depositHelper.reset(new DepositRateHelper(hQuote, depositTerm, fwdStartDays, depositConvention->calendar(),
                                                      depositConvention->convention(), depositConvention->eom(),
                                                      depositConvention->dayCounter()));
        }
        instruments.push_back(depositHelper);
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
    vector<string> futureQuoteIDs = futureSegment->quotes();

    for (Size i = 0; i < futureQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(futureQuoteIDs[i], asofDate_);

        // Check that we have a valid future quote
        boost::shared_ptr<MMFutureQuote> futureQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::MM_FUTURE,
                       "Market quote not of type Future.");
            futureQuote = boost::dynamic_pointer_cast<MMFutureQuote>(marketQuote);
        } else {
            QL_FAIL("Could not find quote for ID " << futureQuoteIDs[i] << " with as of date "
                                                   << io::iso_date(asofDate_) << ".");
        }

        // Create a future helper if we do.
        Date refDate(1, futureQuote->expiryMonth(), futureQuote->expiryYear());
        Date immDate = IMM::nextDate(refDate, false);
        boost::shared_ptr<RateHelper> futureHelper(
            new FuturesRateHelper(futureQuote->quote(), immDate, futureConvention->index()));

        instruments.push_back(futureHelper);
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
    vector<string> fraQuoteIDs = fraSegment->quotes();

    for (Size i = 0; i < fraQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(fraQuoteIDs[i], asofDate_);

        // Check that we have a valid FRA quote
        boost::shared_ptr<FRAQuote> fraQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::FRA,
                       "Market quote not of type FRA.");
            fraQuote = boost::dynamic_pointer_cast<FRAQuote>(marketQuote);
        } else {
            QL_FAIL("Could not find quote for ID " << fraQuoteIDs[i] << " with as of date " << io::iso_date(asofDate_)
                                                   << ".");
        }

        // Create a FRA helper if we do.
        Period periodToStart = fraQuote->fwdStart();
        boost::shared_ptr<RateHelper> fraHelper(
            new FraRateHelper(fraQuote->quote(), periodToStart, fraConvention->index()));

        instruments.push_back(fraHelper);
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
            QL_FAIL("The projection curve, " << projectionCurveID << ", required in the building "
                                                                     "of the curve, " << curveSpec_.name()
                                             << ", was not found.");
        }
        onIndex = boost::dynamic_pointer_cast<OvernightIndex>(onIndex->clone(projectionCurve->handle()));
    }

    vector<string> oisQuoteIDs = oisSegment->quotes();
    for (Size i = 0; i < oisQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(oisQuoteIDs[i], asofDate_);

        // Check that we have a valid OIS quote
        boost::shared_ptr<SwapQuote> oisQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::IR_SWAP,
                       "Market quote (" << marketQuote->name() << ") not of type swap.");
            oisQuote = boost::dynamic_pointer_cast<SwapQuote>(marketQuote);
        } else {
            QL_FAIL("Could not find quote for ID " << oisQuoteIDs[i] << " with as of date " << io::iso_date(asofDate_)
                                                   << ".");
        }

        // Create a swap helper if we do.
        Period oisTenor = oisQuote->term();
        boost::shared_ptr<RateHelper> oisHelper(new QuantExt::OISRateHelper(
            oisConvention->spotLag(), oisTenor, oisQuote->quote(), onIndex, oisConvention->fixedDayCounter(),
            oisConvention->paymentLag(), oisConvention->eom(), oisConvention->fixedFrequency(),
            oisConvention->fixedConvention(), oisConvention->fixedPaymentConvention(), oisConvention->rule(),
            discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>()));

        instruments.push_back(oisHelper);
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
    vector<string> swapQuoteIDs = swapSegment->quotes();

    for (Size i = 0; i < swapQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(swapQuoteIDs[i], asofDate_);

        // Check that we have a valid swap quote
        boost::shared_ptr<SwapQuote> swapQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::IR_SWAP,
                       "Market quote not of type swap.");
            swapQuote = boost::dynamic_pointer_cast<SwapQuote>(marketQuote);
        } else {
            QL_FAIL("Could not find quote for ID " << swapQuoteIDs[i] << " with as of date " << io::iso_date(asofDate_)
                                                   << ".");
        }

        // Create a swap helper if we do.
        Period swapTenor = swapQuote->term();
        boost::shared_ptr<RateHelper> swapHelper(new SwapRateHelper(
            swapQuote->quote(), swapTenor, swapConvention->fixedCalendar(), swapConvention->fixedFrequency(),
            swapConvention->fixedConvention(), swapConvention->fixedDayCounter(), swapConvention->index(),
            Handle<Quote>(), 0 * Days, discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>()));

        instruments.push_back(swapHelper);
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
            QL_FAIL("The projection curve, " << projectionCurveID << ", required in the building "
                                                                     "of the curve, " << curveSpec_.name()
                                             << ", was not found.");
        }
        onIndex = boost::dynamic_pointer_cast<OvernightIndex>(onIndex->clone(projectionCurve->handle()));
    }

    vector<pair<string, string>> averageOisQuoteIDs = averageOisSegment->quotes();
    for (Size i = 0; i < averageOisQuoteIDs.size(); i++) {
        /* An average OIS quote is a composite of a swap quote and a basis
           spread quote. Check first that we have these. */
        // Firstly, the rate quote.
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(averageOisQuoteIDs[i].first, asofDate_);
        boost::shared_ptr<SwapQuote> swapQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::IR_SWAP,
                       "Market quote not of type swap.");
            swapQuote = boost::dynamic_pointer_cast<SwapQuote>(marketQuote);
        } else {
            QL_FAIL("Could not find quote for ID " << averageOisQuoteIDs[i].first << " with as of date "
                                                   << io::iso_date(asofDate_) << ".");
        }
        // Secondly, the basis spread quote.
        marketQuote = loader_.get(averageOisQuoteIDs[i].second, asofDate_);
        boost::shared_ptr<BasisSwapQuote> basisQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BASIS_SWAP,
                       "Market quote not of type basis swap.");
            basisQuote = boost::dynamic_pointer_cast<BasisSwapQuote>(marketQuote);
        } else {
            QL_FAIL("Could not find quote for ID " << averageOisQuoteIDs[i].second << " with as of date "
                                                   << io::iso_date(asofDate_) << ".");
        }

        // Create an average OIS helper if we do.
        Period AverageOisTenor = swapQuote->term();
        QL_REQUIRE(AverageOisTenor == basisQuote->maturity(), "The swap "
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
            QL_FAIL("The short side projection curve, " << shortCurveID << ", required in the building "
                                                                           "of the curve, " << curveSpec_.name()
                                                        << ", was not found.");
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
            QL_FAIL("The long side projection curve, " << longCurveID << ", required in the building "
                                                                         "of the curve, " << curveSpec_.name()
                                                       << ", was not found.");
        }
        longIndex = longIndex->clone(longCurve->handle());
    }

    vector<string> basisSwapQuoteIDs = basisSwapSegment->quotes();
    for (Size i = 0; i < basisSwapQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(basisSwapQuoteIDs[i], asofDate_);

        // Check that we have a valid basis swap quote
        boost::shared_ptr<BasisSwapQuote> basisSwapQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BASIS_SWAP,
                       "Market quote not of type basis swap.");
            basisSwapQuote = boost::dynamic_pointer_cast<BasisSwapQuote>(marketQuote);
        } else {
            QL_FAIL("Could not find quote for ID " << basisSwapQuoteIDs[i] << " with as of date "
                                                   << io::iso_date(asofDate_) << ".");
        }

        // Create a tenor basis swap helper if we do.
        Period basisSwapTenor = basisSwapQuote->maturity();
        boost::shared_ptr<RateHelper> basisSwapHelper(new TenorBasisSwapHelper(
            basisSwapQuote->quote(), basisSwapTenor, longIndex, shortIndex, basisSwapConvention->shortPayTenor(),
            discountCurve_ ? discountCurve_->handle() : Handle<YieldTermStructure>(),
            basisSwapConvention->spreadOnShort(), basisSwapConvention->includeSpread(),
            basisSwapConvention->subPeriodsCouponType()));

        instruments.push_back(basisSwapHelper);
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
            QL_FAIL("The short side projection curve, " << shortCurveID << ", required in the building "
                                                                           "of the curve, " << curveSpec_.name()
                                                        << ", was not found.");
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
            QL_FAIL("The projection curve, " << longCurveID << ", required in the building "
                                                               "of the curve, " << curveSpec_.name()
                                             << ", was not found.");
        }
        longIndex = longIndex->clone(longCurve->handle());
    }

    vector<string> basisSwapQuoteIDs = basisSwapSegment->quotes();
    for (Size i = 0; i < basisSwapQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(basisSwapQuoteIDs[i], asofDate_);

        // Check that we have a valid basis swap quote
        boost::shared_ptr<BasisSwapQuote> basisSwapQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::BASIS_SWAP,
                       "Market quote not of type basis swap.");
            basisSwapQuote = boost::dynamic_pointer_cast<BasisSwapQuote>(marketQuote);
        } else {
            QL_FAIL("Could not find quote for ID " << basisSwapQuoteIDs[i] << " with as of date "
                                                   << io::iso_date(asofDate_) << ".");
        }

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
        QL_FAIL("The foreign discount curve, " << knownDiscountID << ", required in the building "
                                                                     "of the curve, " << curveSpec_.name()
                                               << ", was not found.");
    }

    /* Need to retrieve the market FX spot rate */
    // LOG("YieldCurve::addFXForwards(), retrieve FX rate");
    string spotRateID = fxForwardSegment->spotRateID();
    boost::shared_ptr<MarketDatum> fxSpotMarketQuote = loader_.get(spotRateID, asofDate_);
    boost::shared_ptr<FXSpotQuote> fxSpotQuote;
    if (fxSpotMarketQuote) {
        QL_REQUIRE(fxSpotMarketQuote->instrumentType() == MarketDatum::InstrumentType::FX_SPOT,
                   "Market quote not of type FX spot.");
        fxSpotQuote = boost::dynamic_pointer_cast<FXSpotQuote>(fxSpotMarketQuote);
    } else {
        QL_FAIL("Could not find quote for ID " << spotRateID << " with as of date " << io::iso_date(asofDate_) << ".");
    }

    /* Create an FX spot quote from the retrieved FX spot rate */
    Currency fxSpotSourceCcy = parseCurrency(fxSpotQuote->unitCcy());
    Currency fxSpotTargetCcy = parseCurrency(fxSpotQuote->ccy());

    LOG("YieldCurve::addFXForwards(), create FX forward quotes and helpers");
    vector<string> fxForwardQuoteIDs = fxForwardSegment->quotes();
    for (Size i = 0; i < fxForwardQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(fxForwardQuoteIDs[i], asofDate_);

        // Check that we have a valid FX forward quote
        boost::shared_ptr<FXForwardQuote> fxForwardQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::FX_FWD,
                       "Market quote not of type FX forward.");
            fxForwardQuote = boost::dynamic_pointer_cast<FXForwardQuote>(marketQuote);
        } else {
            QL_FAIL("Could not find quote for ID " << fxForwardQuoteIDs[i] << " with as of date "
                                                   << io::iso_date(asofDate_) << ".");
        }

        QL_REQUIRE(fxSpotQuote->unitCcy() == fxForwardQuote->unitCcy() && fxSpotQuote->ccy() == fxForwardQuote->ccy(),
                   "Currency mismatch between spot \"" << spotRateID << "\" and fwd \"" << fxForwardQuoteIDs[i]
                                                       << "\"");

        // QL expects the FX Fwd quote to be per spot, not points.
        Handle<Quote> qlFXForwardQuote(
            boost::make_shared<SimpleQuote>(fxForwardQuote->quote()->value() / fxConvention->pointsFactor()));

        // Create an FX forward helper
        Period fxForwardTenor = fxForwardQuote->term();
        bool endOfMonth = false;
        bool isFxBaseCurrencyCollateralCurrency = knownCurrency == fxConvention->sourceCurrency(); // TODO 50/50 guess

        // TODO: spotRelative
        boost::shared_ptr<RateHelper> fxForwardHelper(
            new FxSwapRateHelper(qlFXForwardQuote, fxSpotQuote->quote(), fxForwardTenor, fxConvention->spotDays(),
                                 fxConvention->advanceCalendar(), Following, endOfMonth,
                                 isFxBaseCurrencyCollateralCurrency, knownDiscountCurve->handle()));

        instruments.push_back(fxForwardHelper);
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
    boost::shared_ptr<MarketDatum> fxSpotMarketQuote = loader_.get(spotRateID, asofDate_);
    boost::shared_ptr<FXSpotQuote> fxSpotQuote;
    if (fxSpotMarketQuote) {
        QL_REQUIRE(fxSpotMarketQuote->instrumentType() == MarketDatum::InstrumentType::FX_SPOT,
                   "Market quote not of type FX spot.");
        fxSpotQuote = boost::dynamic_pointer_cast<FXSpotQuote>(fxSpotMarketQuote);
    } else {
        QL_FAIL("Could not find quote for ID " << spotRateID << " with as of date " << io::iso_date(asofDate_) << ".");
    }

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
        QL_FAIL("The foreign discount curve, " << foreignDiscountID << ", required in the building "
                                                                       "of the curve, " << curveSpec_.name()
                                               << ", was not found.");
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
            QL_FAIL("The foreign projection curve, " << foreignProjectionCurveID << ", required in the building "
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
            QL_FAIL("The domestic projection curve, " << domesticProjectionCurveID << ", required in the"
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

    vector<string> basisSwapQuoteIDs = basisSwapSegment->quotes();
    for (Size i = 0; i < basisSwapQuoteIDs.size(); i++) {
        boost::shared_ptr<MarketDatum> marketQuote = loader_.get(basisSwapQuoteIDs[i], asofDate_);

        // Check that we have a valid basis swap quote
        boost::shared_ptr<CrossCcyBasisSwapQuote> basisSwapQuote;
        if (marketQuote) {
            QL_REQUIRE(marketQuote->instrumentType() == MarketDatum::InstrumentType::CC_BASIS_SWAP,
                       "Market quote not of type cross "
                       "currency basis swap.");
            basisSwapQuote = boost::dynamic_pointer_cast<CrossCcyBasisSwapQuote>(marketQuote);
        } else {
            QL_FAIL("Could not find quote for ID " << basisSwapQuoteIDs[i] << " with as of date "
                                                   << io::iso_date(asofDate_) << ".");
        }

        // Create a cross currency basis swap helper if we do.
        Period basisSwapTenor = basisSwapQuote->maturity();
        boost::shared_ptr<RateHelper> basisSwapHelper(new CrossCcyBasisSwapHelper(
            basisSwapQuote->quote(), fxSpotQuote->quote(), basisSwapConvention->settlementDays(),
            basisSwapConvention->settlementCalendar(), basisSwapTenor, basisSwapConvention->rollConvention(), flatIndex,
            spreadIndex, flatDiscountCurve, spreadDiscountCurve, basisSwapConvention->eom(),
            flatIndex->currency().code() == fxSpotQuote->unitCcy()));

        instruments.push_back(basisSwapHelper);
    }
}
}
}
