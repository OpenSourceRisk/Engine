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

#include <ored/marketdata/defaultcurve.hpp>
#include <ored/utilities/log.hpp>

#include <qle/termstructures/defaultprobabilityhelpers.hpp>
#include <qle/termstructures/probabilitytraits.hpp>
#include <qle/termstructures/iterativebootstrap.hpp>

#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <algorithm>
#include <regex>

using namespace QuantLib;
using namespace std;

// Temporary workaround to silence warnings on g++ until QL 1.17 is released with the
// pull request: https://github.com/lballabio/QuantLib/pull/679
#ifdef BOOST_MSVC
#define ATTR_UNUSED
#else
#define ATTR_UNUSED __attribute__((unused))
#endif

namespace ore {
namespace data {

DefaultCurve::DefaultCurve(Date asof, DefaultCurveSpec spec, const Loader& loader,
    const CurveConfigurations& curveConfigs, const Conventions& conventions,
    map<string, boost::shared_ptr<YieldCurve>>& yieldCurves) {

    try {

        const boost::shared_ptr<DefaultCurveConfig>& config = curveConfigs.defaultCurveConfig(spec.curveConfigID());

        // Set the recovery rate if necessary
        recoveryRate_ = Null<Real>();
        if (!config->recoveryRateQuote().empty()) {
            QL_REQUIRE(loader.has(config->recoveryRateQuote(), asof),
                "There is no market data for the requested recovery rate " << config->recoveryRateQuote());
            recoveryRate_ = loader.get(config->recoveryRateQuote(), asof)->quote()->value();
        }

        // Build the default curve of the requested type
        switch (config->type()) {
        case DefaultCurveConfig::Type::SpreadCDS:
        case DefaultCurveConfig::Type::Price:
            buildCdsCurve(*config, asof, spec, loader, conventions, yieldCurves);
            break;
        case DefaultCurveConfig::Type::HazardRate:
            buildHazardRateCurve(*config, asof, spec, loader, conventions);
            break;
        case DefaultCurveConfig::Type::Benchmark:
            buildBenchmarkCurve(*config, asof, spec, loader, conventions, yieldCurves);
            break;
        default:
            QL_FAIL("The DefaultCurveConfig type " << static_cast<int>(config->type()) << " was not recognised");
        }

    } catch (exception& e) {
        QL_FAIL("default curve building failed for " << spec.curveConfigID() << ": " << e.what());
    } catch (...) {
        QL_FAIL("default curve building failed for " << spec.curveConfigID() << ": unknown error");
    }
}

void DefaultCurve::buildCdsCurve(DefaultCurveConfig& config, const Date& asof,
    const DefaultCurveSpec& spec, const Loader& loader, const Conventions& conventions,
    map<string, boost::shared_ptr<YieldCurve>>& yieldCurves) {

    LOG("Start building default curve of type SpreadCDS for curve " << config.curveID());

    QL_REQUIRE(
        config.type() == DefaultCurveConfig::Type::SpreadCDS ||
        config.type() == DefaultCurveConfig::Type::Price,
        "DefaultCurve::buildCdsCurve expected a default curve configuration with type SpreadCDS/Price");
    QL_REQUIRE(recoveryRate_ != Null<Real>(), "DefaultCurve: recovery rate needed to build SpreadCDS curve");

    // Get the CDS curve conventions
    QL_REQUIRE(conventions.has(config.conventionID()), "No conventions found with id " << config.conventionID());
    boost::shared_ptr<CdsConvention> cdsConv = boost::dynamic_pointer_cast<CdsConvention>(
        conventions.get(config.conventionID()));
    QL_REQUIRE(cdsConv, "SpreadCDS curves require CDS convention");

    // Get the discount curve for use in the CDS spread curve bootstrap
    auto it = yieldCurves.find(config.discountCurveID());
    QL_REQUIRE(it != yieldCurves.end(), "The discount curve, " << config.discountCurveID() <<
        ", required in the building of the curve, " << spec.name() << ", was not found.");
    Handle<YieldTermStructure> discountCurve = it->second->handle();

    // Get the CDS spread / price curve quotes
    map<Period, Real> quotes = getConfiguredQuotes(config, asof, loader);

    // 0M as a tenor should be allowable with CDS date generation but it is not yet supported so we 
    // remove any CDS pillars that have a Period with length == 0. For the erasing logic while iterating map:
    // https://stackoverflow.com/questions/8234779/how-to-remove-from-a-map-while-iterating-it
    for (auto it = quotes.cbegin(); it != quotes.cend();) {
        if (it->first.length() == 0) {
            WLOG("Removing CDS with tenor zero from curve " << spec.name() << " since it is not supported yet");
            it = quotes.erase(it);
        } else {
            ++it;
        }
    }

    // If date generation rule is DateGeneration::CDS2015, need to ensure that the CDS tenor is a multiple of 
    // 6M. The wrong CDS maturity date is generated if it is not. Additionally, you can end up with a curve that 
    // has multiple identical maturity dates which leads to a crash. Issue opened with QuantLib on this:
    // https://github.com/lballabio/QuantLib/issues/727
    if (cdsConv->rule() == DateGeneration::CDS2015) {
        // Erase any elements with a tenor that is not a multiple of 6 months. Intentionally getting rid of anything
        // here that is in units of days or weeks also.
        for (auto it = quotes.cbegin(); it != quotes.cend();) {
            if ((it->first.units() == Months && it->first.length() % 6 == 0) || it->first.units() == Years) {
                ++it;
            } else {
                WLOG("Removing CDS with tenor " << it->first << " from curve " << spec.name() <<
                    " since date generation rule is CDS2015 and the tenor is not a multiple of 6 months");
                it = quotes.erase(it);
            }
        }
    }

    // Create the CDS instrument helpers
    vector<boost::shared_ptr<QuantExt::DefaultProbabilityHelper>> helpers;
    if (config.type() == DefaultCurveConfig::Type::SpreadCDS) {
        for (auto quote : quotes) {
            helpers.push_back(boost::make_shared<QuantExt::SpreadCdsHelper>(
                quote.second, quote.first, cdsConv->settlementDays(), cdsConv->calendar(), cdsConv->frequency(),
                cdsConv->paymentConvention(), cdsConv->rule(), cdsConv->dayCounter(), recoveryRate_, discountCurve,
                config.startDate(), cdsConv->settlesAccrual(),
                cdsConv->paysAtDefaultTime() ? QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault
                                             : QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd));
        }
    } else {
        for (auto quote : quotes) {
            helpers.push_back(boost::make_shared<QuantExt::UpfrontCdsHelper>(
                quote.second, config.runningSpread(), quote.first, cdsConv->settlementDays(), cdsConv->calendar(),
                cdsConv->frequency(), cdsConv->paymentConvention(), cdsConv->rule(), cdsConv->dayCounter(),
                recoveryRate_, discountCurve, config.startDate(), cdsConv->settlesAccrual(),
                cdsConv->paysAtDefaultTime() ? QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault
                                             : QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd));
        }
    }

    // Get configuration values for bootstrap
    Real accuracy = config.bootstrapConfig().accuracy();
    Real globalAccuracy = config.bootstrapConfig().globalAccuracy();
    bool dontThrow = config.bootstrapConfig().dontThrow();
    Size maxAttempts = config.bootstrapConfig().maxAttempts();
    Real maxFactor = config.bootstrapConfig().maxFactor();
    Real minFactor = config.bootstrapConfig().minFactor();
    Size dontThrowSteps = config.bootstrapConfig().dontThrowSteps();

    // Create the default probability term structure
    typedef PiecewiseDefaultCurve<QuantExt::SurvivalProbability, LogLinear, QuantExt::IterativeBootstrap> SpCurve;
    ATTR_UNUSED typedef SpCurve::traits_type dummy;
    boost::shared_ptr<DefaultProbabilityTermStructure> tmp =
        boost::make_shared<SpCurve>(asof, helpers, config.dayCounter(), accuracy, LogLinear(),
            QuantExt::IterativeBootstrap<SpCurve>(globalAccuracy, dontThrow, maxAttempts,
                maxFactor, minFactor, dontThrowSteps));

    // As for yield curves we need to copy the piecewise curve because on eval date changes the relative date 
    // helpers with trigger a bootstrap.
    vector<Date> dates;
    vector<Real> survivalProbs;
    dates.push_back(asof);
    survivalProbs.push_back(1.0);
    TLOG("Survival probabilities for CDS curve " << config.curveID() << ":");
    TLOG(io::iso_date(dates.back()) << "," << fixed << setprecision(9) << survivalProbs.back());
    for (Size i = 0; i < helpers.size(); ++i) {
        if (helpers[i]->latestDate() > asof) {
            dates.push_back(helpers[i]->latestDate());
            survivalProbs.push_back(tmp->survivalProbability(dates.back()));
            TLOG(io::iso_date(dates.back()) << "," << fixed << setprecision(9) << survivalProbs.back());
        }
    }
    QL_REQUIRE(dates.size() >= 2, "Need at least 2 points to build the default curve");

    LOG("DefaultCurve: copy piecewise curve to interpolated survival probability curve");
    curve_ = boost::make_shared<InterpolatedSurvivalProbabilityCurve<LogLinear>>(
        dates, survivalProbs, config.dayCounter());
    if (config.extrapolation()) {
        curve_->enableExtrapolation();
        DLOG("DefaultCurve: Enabled Extrapolation");
    }

    // Force bootstrap so that errors are thrown during the build, not later
    curve_->survivalProbability(QL_EPSILON);

    LOG("Finished building default curve of type SpreadCDS for curve " << config.curveID());
}

void DefaultCurve::buildHazardRateCurve(DefaultCurveConfig& config, const Date& asof,
    const DefaultCurveSpec& spec, const Loader& loader, const Conventions& conventions) {

    LOG("Start building default curve of type HazardRate for curve " << config.curveID());

    QL_REQUIRE(config.type() == DefaultCurveConfig::Type::HazardRate,
        "DefaultCurve::buildHazardRateCurve expected a default curve configuration with type HazardRate");

    // Get the hazard rate curve conventions
    QL_REQUIRE(conventions.has(config.conventionID()), "No conventions found with id " << config.conventionID());
    boost::shared_ptr<CdsConvention> cdsConv = boost::dynamic_pointer_cast<CdsConvention>(
        conventions.get(config.conventionID()));
    QL_REQUIRE(cdsConv, "HazardRate curves require CDS convention");

    // Get the hazard rate quotes
    map<Period, Real> quotes = getConfiguredQuotes(config, asof, loader);

    // Build the hazard rate curve
    Calendar cal = cdsConv->calendar();
    vector<Date> dates;
    vector<Real> quoteValues;

    // If first term is not zero, add asof point
    if (quotes.begin()->first != 0 * Days) {
        LOG("DefaultCurve: add asof (" << asof << "), hazard rate " << quotes.begin()->second << ", as not given");
        dates.push_back(asof);
        quoteValues.push_back(quotes.begin()->second);
    }

    for (auto quote : quotes) {
        dates.push_back(cal.advance(asof, quote.first, Following, false));
        quoteValues.push_back(quote.second);
    }

    LOG("DefaultCurve: set up interpolated hazard rate curve");
    curve_ = boost::make_shared<InterpolatedHazardRateCurve<BackwardFlat>>(
        dates, quoteValues, config.dayCounter(), BackwardFlat());

    if (config.extrapolation()) {
        curve_->enableExtrapolation();
        DLOG("DefaultCurve: Enabled Extrapolation");
    }

    if (recoveryRate_ == Null<Real>()) {
        LOG("DefaultCurve: setting recovery rate to 0.0 for hazard rate curve, because none is given.");
        recoveryRate_ = 0.0;
    }

    // Force bootstrap so that errors are thrown during the build, not later
    curve_->survivalProbability(QL_EPSILON);

    LOG("Finished building default curve of type HazardRate for curve " << config.curveID());
}

void DefaultCurve::buildBenchmarkCurve(DefaultCurveConfig& config, const Date& asof, const DefaultCurveSpec& spec,
    const Loader& loader, const Conventions& conventions,
    map<string, boost::shared_ptr<YieldCurve>>& yieldCurves) {

    LOG("Start building default curve of type Benchmark for curve " << config.curveID());

    QL_REQUIRE(config.type() == DefaultCurveConfig::Type::Benchmark,
        "DefaultCurve::buildBenchmarkCurve expected a default curve configuration with type Benchmark");

    QL_REQUIRE(recoveryRate_ == Null<Real>(), "DefaultCurve: recovery rate must not be given "
        "for benchmark implied curve type, it is assumed to be 0.0");
    recoveryRate_ = 0.0;

    // Populate benchmark yield curve
    auto it = yieldCurves.find(config.benchmarkCurveID());
    QL_REQUIRE(it != yieldCurves.end(), "The benchmark curve, " << config.benchmarkCurveID() <<
        ", required in the building of the curve, " << spec.name() << ", was not found.");
    boost::shared_ptr<YieldCurve> benchmarkCurve = it->second;

    // Populate source yield curve
    it = yieldCurves.find(config.sourceCurveID());
    QL_REQUIRE(it != yieldCurves.end(), "The source curve, " << config.sourceCurveID() <<
        ", required in the building of the curve, " << spec.name() << ", was not found.");
    boost::shared_ptr<YieldCurve> sourceCurve = it->second;

    // Parameters from the configuration
    vector<Period> pillars = parseVectorOfValues<Period>(config.pillars(), &parsePeriod);
    Calendar cal = config.calendar();
    Size spotLag = config.spotLag();
    
    // Create the implied survival curve
    vector<Date> dates;
    vector<Real> impliedSurvProb;
    Date spot = cal.advance(asof, spotLag * Days);
    for (Size i = 0; i < pillars.size(); ++i) {
        dates.push_back(cal.advance(spot, pillars[i]));
        Real tmp = sourceCurve->handle()->discount(dates[i]) / benchmarkCurve->handle()->discount(dates[i]);
        impliedSurvProb.push_back(dates[i] == asof ? 1.0 : tmp);
    }
    QL_REQUIRE(dates.size() > 0, "DefaultCurve (Benchmark): no dates given");

    // Insert SP = 1.0 at asof if asof date is not in the pillars
    if (dates[0] != asof) {
        dates.insert(dates.begin(), asof);
        impliedSurvProb.insert(impliedSurvProb.begin(), 1.0);
    }

    LOG("DefaultCurve: set up interpolated surv prob curve as yield over benchmark");
    curve_ = boost::make_shared<InterpolatedSurvivalProbabilityCurve<LogLinear>>(
        dates, impliedSurvProb, config.dayCounter());

    if (config.extrapolation()) {
        curve_->enableExtrapolation();
        DLOG("DefaultCurve: Enabled Extrapolation");
    }

    // Force bootstrap so that errors are thrown during the build, not later
    curve_->survivalProbability(QL_EPSILON);

    LOG("Finished building default curve of type Benchmark for curve " << config.curveID());
}

map<Period, Real> DefaultCurve::getConfiguredQuotes(DefaultCurveConfig& config,
    const Date& asof, const Loader& loader) const {

    QL_REQUIRE(config.type() == DefaultCurveConfig::Type::SpreadCDS ||
        config.type() == DefaultCurveConfig::Type::Price ||
        config.type() == DefaultCurveConfig::Type::HazardRate,
        "DefaultCurve::getConfiguredQuotes expected a default curve configuration with type SpreadCDS, Price or HazardRate");
    QL_REQUIRE(!config.cdsQuotes().empty(), "No quotes configured for curve " << config.curveID());

    // Populate with quotes
    map<Period, Real> result;

    // We may have a _single_ regex quote or a list of explicit quotes. Check if we have single regex quote.
    bool haveRegexQuote = config.cdsQuotes()[0].first.find("*") != string::npos;

    if (haveRegexQuote) {
        QL_REQUIRE(config.cdsQuotes().size() == 1, "Wild card specified in " <<
            config.curveID() << " so only one quote should be provided.");
        return getRegexQuotes(config.cdsQuotes()[0].first, config.curveID(), config.type(), asof, loader);
    } else {
        return getExplicitQuotes(config.cdsQuotes(), config.curveID(), config.type(), asof, loader);
    }
}

map<Period, Real> DefaultCurve::getRegexQuotes(string strRegex, const string& configId,
    DefaultCurveConfig::Type type, const Date& asof, const Loader& loader) const {

    LOG("Loading regex quotes for default curve " << configId);

    // "*" is used as wildcard in strRegex in our quotes. Need to replace with ".*" here for regex to work.
    boost::replace_all(strRegex, "*", ".*");

    // Create the regular expression
    regex expression(strRegex);

    // Loop over the available market data and pick out quotes that match the expression
    map<Period, Real> result;
    for (const auto& md : loader.loadQuotes(asof)) {
        
        // Go to next quote if the market data point's date does not equal our asof
        if (md->asofDate() != asof)
            continue;

        // If we have a CDS spread or hazard rate quote, check it and populate tenor and value if it matches
        if (type == DefaultCurveConfig::Type::SpreadCDS && (
            md->instrumentType() == MarketDatum::InstrumentType::CDS &&
            md->quoteType() == MarketDatum::QuoteType::CREDIT_SPREAD)) {
            
            auto q = boost::dynamic_pointer_cast<CdsQuote>(md);
            if (regex_match(q->name(), expression)) {
                addQuote(result, q->term(), *md, configId);
            }

        } else if (type == DefaultCurveConfig::Type::Price && (
            md->instrumentType() == MarketDatum::InstrumentType::CDS &&
            md->quoteType() == MarketDatum::QuoteType::PRICE)) {

            auto q = boost::dynamic_pointer_cast<CdsQuote>(md);
            if (regex_match(q->name(), expression)) {
                addQuote(result, q->term(), *md, configId);
            }

        } else if (type == DefaultCurveConfig::Type::HazardRate && (
            md->instrumentType() == MarketDatum::InstrumentType::HAZARD_RATE &&
            md->quoteType() == MarketDatum::QuoteType::RATE)) {

            auto q = boost::dynamic_pointer_cast<HazardRateQuote>(md);
            if (regex_match(q->name(), expression)) {
                addQuote(result, q->term(), *md, configId);
            }

        }
    }

    QL_REQUIRE(!result.empty(), "No market points found for curve config " << configId);
    LOG("DefaultCurve " << configId << " loaded and using " << result.size() << " quotes.");

    return result;
}

map<Period, Real> DefaultCurve::getExplicitQuotes(const vector<pair<string, bool> >& quotes,
    const string& configId, DefaultCurveConfig::Type type, const Date& asof, const Loader& loader) const {

    LOG("Loading explicit quotes for default curve " << configId);
    
    map<Period, Real> result;
    for (const auto& p : quotes) {
        if (boost::shared_ptr<MarketDatum> md = loader.get(p, asof)) {
            Period tenor;
            if (type == DefaultCurveConfig::Type::SpreadCDS ||
                type == DefaultCurveConfig::Type::Price) {
                tenor = boost::dynamic_pointer_cast<CdsQuote>(md)->term();
            } else {
                tenor = boost::dynamic_pointer_cast<HazardRateQuote>(md)->term();
            }
            addQuote(result, tenor, *md, configId);
        }
    }

    QL_REQUIRE(!result.empty(), "No market points found for curve config " << configId);
    LOG("DefaultCurve " << configId << " using " << result.size() <<
        " default quotes of " << quotes.size() << " requested quotes.");

    return result;
}

void DefaultCurve::addQuote(map<Period, Real>& quotes, const Period& tenor,
    const MarketDatum& marketDatum, const string& configId) const {

    // Add to quotes, with a check that we have no duplicate tenors
    auto r = quotes.insert(make_pair(tenor, marketDatum.quote()->value()));
    QL_REQUIRE(r.second, "duplicate term in quotes found ("
        << tenor << ") while loading default curve " << configId);
    TLOG("Loaded quote " << marketDatum.name() << " for default curve " << configId);
}

} // namespace data
} // namespace ore
