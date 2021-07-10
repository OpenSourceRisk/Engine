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
#include <ored/marketdata/yieldcurve.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/wildcard.hpp>

#include <qle/termstructures/defaultprobabilityhelpers.hpp>
#include <qle/termstructures/interpolatedhazardratecurve.hpp>
#include <qle/termstructures/interpolatedsurvivalprobabilitycurve.hpp>
#include <qle/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/multisectiondefaultcurve.hpp>
#include <qle/termstructures/probabilitytraits.hpp>

#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <algorithm>
#include <set>

using namespace QuantLib;
using namespace std;

// Temporary workaround to silence warnings on g++ until QL 1.17 is released with the
// pull request: https://github.com/lballabio/QuantLib/pull/679
#ifdef BOOST_MSVC
#define ATTR_UNUSED
#else
#define ATTR_UNUSED __attribute__((unused))
#endif

namespace {

using namespace ore::data;

struct QuoteData {
    QuoteData() : value(Null<Real>()), runningSpread(Null<Real>()) {}

    QuoteData(const Period& t, Real v, Real rs = Null<Real>())
        : term(t), value(v), runningSpread(rs) {}

    Period term;
    Real value;
    Real runningSpread;
};

bool operator<(const QuoteData& lhs, const QuoteData& rhs)
{
    return lhs.term < rhs.term;
}

void addQuote(set<QuoteData>& quotes, const string& configId, const string& name, const Period& tenor,
    Real value, Real runningSpread = Null<Real>()) {

    // Add to quotes, with a check that we have no duplicate tenors
    auto r = quotes.insert(QuoteData(tenor, value, runningSpread));
    QL_REQUIRE(r.second, "duplicate term in quotes found (" << tenor << ") while loading default curve " << configId);
    TLOG("Loaded quote " << name << " for default curve " << configId);
}

set<QuoteData> getRegexQuotes(const Wildcard& wc, const string& configId, DefaultCurveConfig::Type type,
    const Date& asof, const Loader& loader) {

    using DCCT = DefaultCurveConfig::Type;
    using MDQT = MarketDatum::QuoteType;
    using MDIT = MarketDatum::InstrumentType;
    LOG("Loading regex quotes for default curve " << configId);

    // Loop over the available market data and pick out quotes that match the expression
    set<QuoteData> result;
    for (const auto& md : loader.loadQuotes(asof)) {

        // Go to next quote if the market data point's date does not equal our asof
        if (md->asofDate() != asof)
            continue;

        auto mdit = md->instrumentType();
        auto mdqt = md->quoteType();

        // If we have a CDS spread or hazard rate quote, check it and populate tenor and value if it matches
        if (type == DCCT::SpreadCDS && mdit == MDIT::CDS &&
            (mdqt == MDQT::CREDIT_SPREAD || mdqt == MDQT::CONV_CREDIT_SPREAD)) {

            auto q = boost::dynamic_pointer_cast<CdsQuote>(md);
            if (wc.matches(q->name())) {
                addQuote(result, configId, q->name(), q->term(), q->quote()->value(), q->runningSpread());
            }

        } else if (type == DCCT::Price && (mdit == MDIT::CDS && mdqt == MDQT::PRICE)) {

            auto q = boost::dynamic_pointer_cast<CdsQuote>(md);
            if (wc.matches(q->name())) {
                addQuote(result, configId, q->name(), q->term(), q->quote()->value(), q->runningSpread());
            }

        } else if (type == DCCT::HazardRate && (mdit == MDIT::HAZARD_RATE && mdqt == MDQT::RATE)) {

            auto q = boost::dynamic_pointer_cast<HazardRateQuote>(md);
            if (wc.matches(q->name())) {
                addQuote(result, configId, q->name(), q->term(), q->quote()->value());
            }
        }
    }

    // We don't check for an empty set of CDS quotes here. We check it later because under some circumstances,
    // it may be allowable to have no quotes.
    if (type != DCCT::SpreadCDS && type != DCCT::Price) {
        QL_REQUIRE(!result.empty(), "No market points found for curve config " << configId);
    }

    LOG("DefaultCurve " << configId << " loaded and using " << result.size() << " quotes.");

    return result;
}

set<QuoteData> getExplicitQuotes(const vector<pair<string, bool>>& quotes, const string& configId,
    DefaultCurveConfig::Type type, const Date& asof, const Loader& loader) {

    using DCCT = DefaultCurveConfig::Type;
    LOG("Loading explicit quotes for default curve " << configId);

    set<QuoteData> result;
    for (const auto& p : quotes) {
        if (boost::shared_ptr<MarketDatum> md = loader.get(p, asof)) {
            if (type == DCCT::SpreadCDS || type == DCCT::Price) {
                auto q = boost::dynamic_pointer_cast<CdsQuote>(md);
                QL_REQUIRE(q, "Quote " << p.first << " for config " << configId << " should be a CdsQuote");
                addQuote(result, configId, q->name(), q->term(), q->quote()->value(), q->runningSpread());
            } else {
                auto q = boost::dynamic_pointer_cast<HazardRateQuote>(md);
                QL_REQUIRE(q, "Quote " << p.first << " for config " << configId << " should be a HazardRateQuote");
                addQuote(result, configId, q->name(), q->term(), q->quote()->value());
            }
        }
    }

    // We don't check for an empty set of CDS quotes here. We check it later because under some circumstances,
    // it may be allowable to have no quotes.
    if (type != DCCT::SpreadCDS && type != DCCT::Price) {
        QL_REQUIRE(!result.empty(), "No market points found for curve config " << configId);
    }

    LOG("DefaultCurve " << configId << " using " << result.size() << " default quotes of " << quotes.size()
        << " requested quotes.");

    return result;
}

set<QuoteData> getConfiguredQuotes(DefaultCurveConfig& config, const Date& asof, const Loader& loader) {

    using DCCT = DefaultCurveConfig::Type;
    auto type = config.type();
    QL_REQUIRE(type == DCCT::SpreadCDS || type == DCCT::Price || type == DCCT::HazardRate,
        "getConfiguredQuotes expects a curve type of SpreadCDS, Price or HazardRate.");
    QL_REQUIRE(!config.cdsQuotes().empty(), "No quotes configured for curve " << config.curveID());

    // We may have a _single_ regex quote or a list of explicit quotes. Check if we have single regex quote.
    std::vector<std::string> tmp;
    std::transform(config.cdsQuotes().begin(), config.cdsQuotes().end(), std::back_inserter(tmp),
                   [](const std::pair<std::string, bool>& p) { return p.first; });
    auto wildcard = getUniqueWildcard(tmp);

    if (wildcard) {
        return getRegexQuotes(*wildcard, config.curveID(), config.type(), asof, loader);
    } else {
        return getExplicitQuotes(config.cdsQuotes(), config.curveID(), config.type(), asof, loader);
    }
}

}

namespace ore {
namespace data {

DefaultCurve::DefaultCurve(Date asof, DefaultCurveSpec spec, const Loader& loader,
                           const CurveConfigurations& curveConfigs, const Conventions& conventions,
                           map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                           map<string, boost::shared_ptr<DefaultCurve>>& defaultCurves) {

    try {

        const boost::shared_ptr<DefaultCurveConfig>& config = curveConfigs.defaultCurveConfig(spec.curveConfigID());

        // Set the recovery rate if necessary
        recoveryRate_ = Null<Real>();
        if (!config->recoveryRateQuote().empty()) {
            // for some curve types (Benchmark, MutliSection) a numeric recovery rate is allowed, which we handle here
            if (!tryParseReal(config->recoveryRateQuote(), recoveryRate_)) {
                QL_REQUIRE(loader.has(config->recoveryRateQuote(), asof),
                           "There is no market data for the requested recovery rate " << config->recoveryRateQuote());
                recoveryRate_ = loader.get(config->recoveryRateQuote(), asof)->quote()->value();
            }
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
        case DefaultCurveConfig::Type::MultiSection:
            buildMultiSectionCurve(*config, asof, spec, loader, conventions, defaultCurves);
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

void DefaultCurve::buildCdsCurve(DefaultCurveConfig& config, const Date& asof, const DefaultCurveSpec& spec,
                                 const Loader& loader, const Conventions& conventions,
                                 map<string, boost::shared_ptr<YieldCurve>>& yieldCurves) {

    LOG("Start building default curve of type SpreadCDS for curve " << config.curveID());

    QL_REQUIRE(config.type() == DefaultCurveConfig::Type::SpreadCDS || config.type() == DefaultCurveConfig::Type::Price,
               "DefaultCurve::buildCdsCurve expected a default curve configuration with type SpreadCDS/Price");
    QL_REQUIRE(recoveryRate_ != Null<Real>(), "DefaultCurve: recovery rate needed to build SpreadCDS curve");

    // Get the CDS curve conventions
    QL_REQUIRE(conventions.has(config.conventionID()), "No conventions found with id " << config.conventionID());
    boost::shared_ptr<CdsConvention> cdsConv =
        boost::dynamic_pointer_cast<CdsConvention>(conventions.get(config.conventionID()));
    QL_REQUIRE(cdsConv, "SpreadCDS curves require CDS convention");

    // Get the discount curve for use in the CDS spread curve bootstrap
    auto it = yieldCurves.find(config.discountCurveID());
    QL_REQUIRE(it != yieldCurves.end(), "The discount curve, " << config.discountCurveID()
                                                               << ", required in the building of the curve, "
                                                               << spec.name() << ", was not found.");
    Handle<YieldTermStructure> discountCurve = it->second->handle();

    // Get the CDS spread / price curve quotes
    set<QuoteData> quotes = getConfiguredQuotes(config, asof, loader);

    // If the configuration instructs us to imply a default from the market data, we do it here.
    if (config.implyDefaultFromMarket() && *config.implyDefaultFromMarket()) {
        if (recoveryRate_ != Null<Real>() && quotes.empty()) {
            // Assume entity is in default, between event determination date and auction date. Build a survivial
            // probability curve with value 0.0 tomorrow to approximate this and allow dependent instruments to price.
            // Need to use small but positive numbers to avoid downstream issues with log linear survivals e.g. below
            // and in places like ScenarioSimMarket.
            vector<Date> dates{asof, asof + 1 * Years, asof + 10 * Years};
            vector<Real> survivalProbs{1.0, 1e-16, 1e-18};
            curve_ = boost::make_shared<QuantExt::InterpolatedSurvivalProbabilityCurve<LogLinear>>(
                dates, survivalProbs, config.dayCounter(), Calendar(), std::vector<Handle<Quote>>(),
                std::vector<Date>(), LogLinear());
            curve_->enableExtrapolation();
            WLOG("DefaultCurve: recovery rate found but no CDS quotes for "
                 << config.curveID() << " and "
                 << "ImplyDefaultFromMarket is true. Curve built that gives default immediately.");
            return;
        }
    } else {
        QL_REQUIRE(!quotes.empty(), "No market points found for CDS curve config " << config.curveID());
    }

    // Create the CDS instrument helpers
    vector<boost::shared_ptr<QuantExt::DefaultProbabilityHelper>> helpers;
    QuantExt::CreditDefaultSwap::ProtectionPaymentTime ppt = cdsConv->paysAtDefaultTime() ?
        QuantExt::CreditDefaultSwap::atDefault : QuantExt::CreditDefaultSwap::atPeriodEnd;

    if (config.type() == DefaultCurveConfig::Type::SpreadCDS) {
        for (auto quote : quotes) {
            helpers.push_back(boost::make_shared<QuantExt::SpreadCdsHelper>(
                quote.value, quote.term, cdsConv->settlementDays(), cdsConv->calendar(), cdsConv->frequency(),
                cdsConv->paymentConvention(), cdsConv->rule(), cdsConv->dayCounter(), recoveryRate_, discountCurve,
                config.startDate(), cdsConv->settlesAccrual(), ppt, cdsConv->lastPeriodDayCounter()));
        }
    } else {
        for (auto quote : quotes) {

            // If there is no running spread encoded in the quote, the config must have one.
            Real runningSpread = quote.runningSpread;
            if (runningSpread == Null<Real>()) {
                QL_REQUIRE(config.runningSpread() != Null<Real>(), "A running spread was not provided in the quote " <<
                    "string so it must be provided in the config for CDS upfront curve " << config.curveID());
                runningSpread = config.runningSpread();
            }

            helpers.push_back(boost::make_shared<QuantExt::UpfrontCdsHelper>(
                quote.value, runningSpread, quote.term, cdsConv->settlementDays(), cdsConv->calendar(),
                cdsConv->frequency(), cdsConv->paymentConvention(), cdsConv->rule(), cdsConv->dayCounter(),
                recoveryRate_, discountCurve, config.startDate(), cdsConv->upfrontSettlementDays(),
                cdsConv->settlesAccrual(), ppt, cdsConv->lastPeriodDayCounter()));
        }
    }

    // Ensure that the helpers are sorted. This is done in IterativeBootstrap, but we need
    // a sorted instruments vector in the code here as well.
    std::sort(helpers.begin(), helpers.end(), QuantLib::detail::BootstrapHelperSorter());

    // Get configuration values for bootstrap
    Real accuracy = config.bootstrapConfig().accuracy();
    Real globalAccuracy = config.bootstrapConfig().globalAccuracy();
    bool dontThrow = config.bootstrapConfig().dontThrow();
    Size maxAttempts = config.bootstrapConfig().maxAttempts();
    Real maxFactor = config.allowNegativeRates() ? config.bootstrapConfig().maxFactor() : 1.0;
    Real minFactor = config.bootstrapConfig().minFactor();
    Size dontThrowSteps = config.bootstrapConfig().dontThrowSteps();

    // Create the default probability term structure
    typedef PiecewiseDefaultCurve<QuantExt::SurvivalProbability, LogLinear, QuantExt::IterativeBootstrap> SpCurve;
    ATTR_UNUSED typedef SpCurve::traits_type dummy;
    boost::shared_ptr<DefaultProbabilityTermStructure> tmp = boost::make_shared<SpCurve>(
        asof, helpers, config.dayCounter(), LogLinear(),
        QuantExt::IterativeBootstrap<SpCurve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor, minFactor,
                                              dontThrowSteps));

    // As for yield curves we need to copy the piecewise curve because on eval date changes the relative date
    // helpers with trigger a bootstrap.
    vector<Date> dates;
    vector<Real> survivalProbs;
    dates.push_back(asof);
    survivalProbs.push_back(1.0);
    TLOG("Survival probabilities for CDS curve " << config.curveID() << ":");
    TLOG(io::iso_date(dates.back()) << "," << fixed << setprecision(9) << survivalProbs.back());

    bool gotAliveHelper = false;

    for (Size i = 0; i < helpers.size(); ++i) {
        if (helpers[i]->latestDate() > asof) {
            gotAliveHelper = true;

            Date pillarDate = helpers[i]->pillarDate();
            Probability sp = tmp->survivalProbability(pillarDate);

            // In some cases the bootstrapped survival probability at one tenor will be `close` to that at a previous
            // tenor. Here we don't add that survival probability and date to avoid issues when creating the
            // InterpolatedSurvivalProbabilityCurve below.
            if (!survivalProbs.empty() && close(survivalProbs.back(), sp)) {
                DLOG("Survival probability for curve " << spec.name() << " at date " << io::iso_date(pillarDate)
                                                       << " is the same as that at previous date "
                                                       << io::iso_date(dates.back()) << " so skipping it.");
                continue;
            }

            dates.push_back(pillarDate);
            survivalProbs.push_back(sp);
            TLOG(io::iso_date(pillarDate) << "," << fixed << setprecision(9) << sp);
        }
    }
    QL_REQUIRE(gotAliveHelper, "Need at least one alive helper to build the default curve");
    if (dates.size() == 1) {
        // We might have removed points above. To make the interpolation work, we need at least two points though.
        dates.push_back(dates.back() + 1);
        survivalProbs.push_back(survivalProbs.back());
    }

    LOG("DefaultCurve: copy piecewise curve to interpolated survival probability curve");
    curve_ = boost::make_shared<QuantExt::InterpolatedSurvivalProbabilityCurve<LogLinear>>(
        dates, survivalProbs, config.dayCounter(), Calendar(), std::vector<Handle<Quote>>(), std::vector<Date>(),
        LogLinear(), config.allowNegativeRates());
    if (config.extrapolation()) {
        curve_->enableExtrapolation();
        DLOG("DefaultCurve: Enabled Extrapolation");
    }

    // Force bootstrap so that errors are thrown during the build, not later
    curve_->survivalProbability(QL_EPSILON);

    LOG("Finished building default curve of type SpreadCDS for curve " << config.curveID());
}

void DefaultCurve::buildHazardRateCurve(DefaultCurveConfig& config, const Date& asof, const DefaultCurveSpec& spec,
                                        const Loader& loader, const Conventions& conventions) {

    LOG("Start building default curve of type HazardRate for curve " << config.curveID());

    QL_REQUIRE(config.type() == DefaultCurveConfig::Type::HazardRate,
               "DefaultCurve::buildHazardRateCurve expected a default curve configuration with type HazardRate");

    // Get the hazard rate curve conventions
    QL_REQUIRE(conventions.has(config.conventionID()), "No conventions found with id " << config.conventionID());
    boost::shared_ptr<CdsConvention> cdsConv =
        boost::dynamic_pointer_cast<CdsConvention>(conventions.get(config.conventionID()));
    QL_REQUIRE(cdsConv, "HazardRate curves require CDS convention");

    // Get the hazard rate quotes
    set<QuoteData> quotes = getConfiguredQuotes(config, asof, loader);

    // Build the hazard rate curve
    Calendar cal = cdsConv->calendar();
    vector<Date> dates;
    vector<Real> quoteValues;

    // If first term is not zero, add asof point
    if (quotes.begin()->term != 0 * Days) {
        LOG("DefaultCurve: add asof (" << asof << "), hazard rate " << quotes.begin()->value << ", as not given");
        dates.push_back(asof);
        quoteValues.push_back(quotes.begin()->value);
    }

    for (auto quote : quotes) {
        dates.push_back(cal.advance(asof, quote.term, Following, false));
        quoteValues.push_back(quote.value);
    }

    LOG("DefaultCurve: set up interpolated hazard rate curve");
    curve_ = boost::make_shared<QuantExt::InterpolatedHazardRateCurve<BackwardFlat>>(
        dates, quoteValues, config.dayCounter(), BackwardFlat(), config.allowNegativeRates());

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

    if (recoveryRate_ == Null<Real>())
        recoveryRate_ = 0.0;

    // Populate benchmark yield curve
    auto it = yieldCurves.find(config.benchmarkCurveID());
    QL_REQUIRE(it != yieldCurves.end(), "The benchmark curve, " << config.benchmarkCurveID()
                                                                << ", required in the building of the curve, "
                                                                << spec.name() << ", was not found.");
    boost::shared_ptr<YieldCurve> benchmarkCurve = it->second;

    // Populate source yield curve
    it = yieldCurves.find(config.sourceCurveID());
    QL_REQUIRE(it != yieldCurves.end(), "The source curve, " << config.sourceCurveID()
                                                             << ", required in the building of the curve, "
                                                             << spec.name() << ", was not found.");
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
        Real tmp = dates[i] == asof
                       ? 1.0
                       : sourceCurve->handle()->discount(dates[i]) / benchmarkCurve->handle()->discount(dates[i]);
        // if a non-zero recovery rate is given, we adjust the implied surv probability according to a market value
        // recovery model (see the documentation of the benchmark curve in the user guide for more details)
        impliedSurvProb.push_back(std::pow(tmp, 1.0 / (1.0 - recoveryRate_)));
    }
    QL_REQUIRE(dates.size() > 0, "DefaultCurve (Benchmark): no dates given");

    // Insert SP = 1.0 at asof if asof date is not in the pillars
    if (dates[0] != asof) {
        dates.insert(dates.begin(), asof);
        impliedSurvProb.insert(impliedSurvProb.begin(), 1.0);
    }

    LOG("DefaultCurve: set up interpolated surv prob curve as yield over benchmark");
    curve_ = boost::make_shared<QuantExt::InterpolatedSurvivalProbabilityCurve<LogLinear>>(
        dates, impliedSurvProb, config.dayCounter(), Calendar(), std::vector<Handle<Quote>>(), std::vector<Date>(),
        LogLinear(), config.allowNegativeRates());

    if (config.extrapolation()) {
        curve_->enableExtrapolation();
        DLOG("DefaultCurve: Enabled Extrapolation");
    }

    // Force bootstrap so that errors are thrown during the build, not later
    curve_->survivalProbability(QL_EPSILON);

    LOG("Finished building default curve of type Benchmark for curve " << config.curveID());
}

void DefaultCurve::buildMultiSectionCurve(DefaultCurveConfig& config, const Date& asof, const DefaultCurveSpec& spec,
                                          const Loader& loader, const Conventions& conventions,
                                          map<string, boost::shared_ptr<DefaultCurve>>& defaultCurves) {
    LOG("Start building default curve of type MultiSection for curve " << config.curveID());

    std::vector<Handle<DefaultProbabilityTermStructure>> curves;
    std::vector<Handle<Quote>> recoveryRates;
    std::vector<Date> switchDates;

    for (auto const& s : config.multiSectionSourceCurveIds()) {
        auto it = defaultCurves.find(s);
        QL_REQUIRE(it != defaultCurves.end(),
                   "The multi section source curve " << s << " required for " << spec.name() << " was not found.");
        curves.push_back(Handle<DefaultProbabilityTermStructure>(it->second->defaultTermStructure()));
        recoveryRates.push_back(Handle<Quote>(boost::make_shared<SimpleQuote>(it->second->recoveryRate())));
    }

    for (auto const& d : config.multiSectionSwitchDates()) {
        switchDates.push_back(parseDate(d));
    }

    Handle<Quote> recoveryRate(boost::make_shared<SimpleQuote>(recoveryRate_));
    LOG("DefaultCurve: set up multi section curve with " << curves.size() << " sections");
    curve_ = boost::make_shared<QuantExt::MultiSectionDefaultCurve>(curves, recoveryRates, switchDates, recoveryRate,
                                                                    config.dayCounter(), config.extrapolation());

    LOG("Finished building default curve of type MultiSection for curve " << config.curveID());
}

} // namespace data
} // namespace ore
