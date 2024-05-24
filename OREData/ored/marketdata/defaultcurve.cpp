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

#include <qle/termstructures/generatordefaulttermstructure.hpp>
#include <qle/termstructures/interpolatedhazardratecurve.hpp>
#include <qle/termstructures/interpolatedsurvivalprobabilitycurve.hpp>
#include <qle/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/multisectiondefaultcurve.hpp>
#include <qle/termstructures/probabilitytraits.hpp>
#include <qle/termstructures/terminterpolateddefaultcurve.hpp>
#include <qle/utilities/interpolation.hpp>
#include <qle/utilities/time.hpp>

#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/termstructures/credit/defaultprobabilityhelpers.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <algorithm>
#include <set>

using namespace QuantLib;
using namespace std;

namespace {

using namespace ore::data;

struct QuoteData {
    QuoteData() : value(Null<Real>()), runningSpread(Null<Real>()) {}

    QuoteData(const Period& t, Real v, string s, string c, string d, Real rs = Null<Real>())
        : term(t), value(v), seniority(s), ccy(c), docClause(d), runningSpread(rs) {}

    Period term;
    Real value;
    string seniority;
    string ccy;
    string docClause;
    Real runningSpread;
};

bool operator<(const QuoteData& lhs, const QuoteData& rhs) { return lhs.term < rhs.term; }

void addQuote(set<QuoteData>& quotes, const string& configId, const string& name, const Period& tenor, Real value,
              string seniority, string ccy, string docClause, Real runningSpread = Null<Real>()) {

    // Add to quotes, with a check that we have no duplicate tenors
    auto r = quotes.insert(QuoteData(tenor, value, seniority, ccy, docClause, runningSpread));
    QL_REQUIRE(r.second, "duplicate term in quotes found (" << tenor << ") while loading default curve " << configId);
    TLOG("Loaded quote " << name << " for default curve " << configId);
}

set<QuoteData> getRegexQuotes(const Wildcard& wc, const string& configId, DefaultCurveConfig::Config::Type type,
                              const Date& asof, const Loader& loader) {

    using DCCT = DefaultCurveConfig::Config::Type;
    using MDQT = MarketDatum::QuoteType;
    using MDIT = MarketDatum::InstrumentType;
    LOG("Loading regex quotes for default curve " << configId);

    // Loop over the available market data and pick out quotes that match the expression
    set<QuoteData> result;

    std::ostringstream ss1;
    ss1 << MDIT::CDS << "/*";
    Wildcard w1(ss1.str());
    auto data1 = loader.get(w1, asof);

    std::ostringstream ss2;
    ss2 << MDIT::HAZARD_RATE << "/*";
    Wildcard w2(ss2.str());
    auto data2 = loader.get(w2, asof);

    data1.merge(data2);

    for (const auto& md : data1) {

        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

        auto mdit = md->instrumentType();
        auto mdqt = md->quoteType();

        // If we have a CDS spread or hazard rate quote, check it and populate tenor and value if it matches
        if (type == DCCT::SpreadCDS && mdit == MDIT::CDS &&
            (mdqt == MDQT::CREDIT_SPREAD || mdqt == MDQT::CONV_CREDIT_SPREAD)) {

            auto q = QuantLib::ext::dynamic_pointer_cast<CdsQuote>(md);
            QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to CdsQuote");
            if (wc.matches(q->name())) {
                addQuote(result, configId, q->name(), q->term(), q->quote()->value(), q->seniority(), q->ccy(),
                         q->docClause(), q->runningSpread());
            }

        } else if (type == DCCT::Price && (mdit == MDIT::CDS && mdqt == MDQT::PRICE)) {

            auto q = QuantLib::ext::dynamic_pointer_cast<CdsQuote>(md);
            QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to CdsQuote");
            if (wc.matches(q->name())) {
                addQuote(result, configId, q->name(), q->term(), q->quote()->value(), q->seniority(), q->ccy(),
                         q->docClause(), q->runningSpread());
            }

        } else if (type == DCCT::HazardRate && (mdit == MDIT::HAZARD_RATE && mdqt == MDQT::RATE)) {

            auto q = QuantLib::ext::dynamic_pointer_cast<HazardRateQuote>(md);
            QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to HazardRateQuote");
            if (wc.matches(q->name())) {
                addQuote(result, configId, q->name(), q->term(), q->quote()->value(), q->seniority(), q->ccy(),
                         q->docClause());
            }
        }
    }

    // check whether the set of quotes contains more than one seniority, ccy or doc clause,
    // which probably means that the wildcard has pulled in too many quotes

    std::set<string> seniorities, ccys, docClauses;
    for (auto const& q : result) {
        if (!q.seniority.empty())
            seniorities.insert(q.seniority);
        if (!q.ccy.empty())
            ccys.insert(q.ccy);
        if (!q.docClause.empty())
            docClauses.insert(q.docClause);
    }

    QL_REQUIRE(seniorities.size() < 2, "More than one seniority found in wildcard quotes.");
    QL_REQUIRE(ccys.size() < 2, "More than one seniority found in wildcard quotes.");
    QL_REQUIRE(docClauses.size() < 2, "More than one seniority found in wildcard quotes.");

    // We don't check for an empty set of CDS quotes here. We check it later because under some circumstances,
    // it may be allowable to have no quotes.
    if (type != DCCT::SpreadCDS && type != DCCT::Price) {
        QL_REQUIRE(!result.empty(), "No market points found for curve config " << configId);
    }

    LOG("DefaultCurve " << configId << " loaded and using " << result.size() << " quotes.");

    return result;
}

set<QuoteData> getExplicitQuotes(const vector<pair<string, bool>>& quotes, const string& configId,
                                 DefaultCurveConfig::Config::Type type, const Date& asof, const Loader& loader) {

    using DCCT = DefaultCurveConfig::Config::Type;
    LOG("Loading explicit quotes for default curve " << configId);

    set<QuoteData> result;
    for (const auto& p : quotes) {
        if (QuantLib::ext::shared_ptr<MarketDatum> md = loader.get(p, asof)) {
            if (type == DCCT::SpreadCDS || type == DCCT::Price) {
                auto q = QuantLib::ext::dynamic_pointer_cast<CdsQuote>(md);
                QL_REQUIRE(q, "Quote " << p.first << " for config " << configId << " should be a CdsQuote");
                addQuote(result, configId, q->name(), q->term(), q->quote()->value(), q->seniority(), q->ccy(),
                         q->docClause(), q->runningSpread());
            } else {
                auto q = QuantLib::ext::dynamic_pointer_cast<HazardRateQuote>(md);
                QL_REQUIRE(q, "Quote " << p.first << " for config " << configId << " should be a HazardRateQuote");
                addQuote(result, configId, q->name(), q->term(), q->quote()->value(), q->seniority(), q->ccy(),
                         q->docClause());
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

set<QuoteData> getConfiguredQuotes(const std::string& curveID, const DefaultCurveConfig::Config& config,
                                   const Date& asof, const Loader& loader) {

    using DCCT = DefaultCurveConfig::Config::Type;
    auto type = config.type();
    QL_REQUIRE(type == DCCT::SpreadCDS || type == DCCT::Price || type == DCCT::HazardRate,
               "getConfiguredQuotes expects a curve type of SpreadCDS, Price or HazardRate.");
    QL_REQUIRE(!config.cdsQuotes().empty(), "No quotes configured for curve " << curveID);

    // We may have a _single_ regex quote or a list of explicit quotes. Check if we have single regex quote.
    std::vector<std::string> tmp;
    std::transform(config.cdsQuotes().begin(), config.cdsQuotes().end(), std::back_inserter(tmp),
                   [](const std::pair<std::string, bool>& p) { return p.first; });
    auto wildcard = getUniqueWildcard(tmp);

    if (wildcard) {
        return getRegexQuotes(*wildcard, curveID, config.type(), asof, loader);
    } else {
        return getExplicitQuotes(config.cdsQuotes(), curveID, config.type(), asof, loader);
    }
}

} // namespace

namespace ore {
namespace data {

DefaultCurve::DefaultCurve(Date asof, DefaultCurveSpec spec, const Loader& loader,
                           const CurveConfigurations& curveConfigs,
                           map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                           map<string, QuantLib::ext::shared_ptr<DefaultCurve>>& defaultCurves) {
    const QuantLib::ext::shared_ptr<DefaultCurveConfig>& configs = curveConfigs.defaultCurveConfig(spec.curveConfigID());
    bool built = false;
    std::string errors;
    for (auto const& config : configs->configs()) {
        try {
            recoveryRate_ = Null<Real>();
            if (!config.second.recoveryRateQuote().empty()) {
                // handle case where the recovery rate is hardcoded in the curve config
                if (!tryParseReal(config.second.recoveryRateQuote(), recoveryRate_)) {
                    Wildcard wc(config.second.recoveryRateQuote());
                    if (wc.hasWildcard()) {
                        for (auto const& q : loader.get(wc, asof)) {
                            if (wc.matches(q->name())) {
                                QL_REQUIRE(recoveryRate_ == Null<Real>(),
                                           "There is more than one recovery rate matching the pattern '" << wc.pattern()
                                                                                                         << "'.");
                                recoveryRate_ = q->quote()->value();
                            }
                        }
                    } else {
                        QL_REQUIRE(loader.has(config.second.recoveryRateQuote(), asof),
                                   "There is no market data for the requested recovery rate "
                                       << config.second.recoveryRateQuote());
                        recoveryRate_ = loader.get(config.second.recoveryRateQuote(), asof)->quote()->value();
                    }
                }
            }
            // Build the default curve of the requested type
            switch (config.second.type()) {
            case DefaultCurveConfig::Config::Type::SpreadCDS:
            case DefaultCurveConfig::Config::Type::Price:
                buildCdsCurve(configs->curveID(), config.second, asof, spec, loader, yieldCurves);
                break;
            case DefaultCurveConfig::Config::Type::HazardRate:
                buildHazardRateCurve(configs->curveID(), config.second, asof, spec, loader);
                break;
            case DefaultCurveConfig::Config::Type::Benchmark:
                buildBenchmarkCurve(configs->curveID(), config.second, asof, spec, loader, yieldCurves);
                break;
            case DefaultCurveConfig::Config::Type::MultiSection:
                buildMultiSectionCurve(configs->curveID(), config.second, asof, spec, loader, defaultCurves);
                break;
            case DefaultCurveConfig::Config::Type::TransitionMatrix:
                buildTransitionMatrixCurve(configs->curveID(), config.second, asof, spec, loader, defaultCurves);
                break;
            case DefaultCurveConfig::Config::Type::Null:
                buildNullCurve(configs->curveID(), config.second, asof, spec);
                break;
            default:
                QL_FAIL("The DefaultCurveConfig type " << static_cast<int>(config.second.type())
                                                       << " was not recognised");
            }
            built = true;
            break;
        } catch (exception& e) {
            std::ostringstream message;
            message << "build attempt failed for " << configs->curveID() << " using config with priority "
                    << config.first << ": " << e.what();
            DLOG(message.str());
            if (!errors.empty())
                errors += ", ";
            errors += message.str();
        }
    }
    QL_REQUIRE(built, "default curve building failed for " << spec.curveConfigID() << ": " << errors);
}

void DefaultCurve::buildCdsCurve(const std::string& curveID, const DefaultCurveConfig::Config& config, const Date& asof,
                                 const DefaultCurveSpec& spec, const Loader& loader,
                                 map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves) {

    LOG("Start building default curve of type SpreadCDS for curve " << curveID);

    QL_REQUIRE(config.type() == DefaultCurveConfig::Config::Type::SpreadCDS ||
                   config.type() == DefaultCurveConfig::Config::Type::Price,
               "DefaultCurve::buildCdsCurve expected a default curve configuration with type SpreadCDS/Price");
    QL_REQUIRE(recoveryRate_ != Null<Real>(), "DefaultCurve: recovery rate needed to build SpreadCDS curve");

    // Get the CDS curve conventions
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QL_REQUIRE(conventions->has(config.conventionID()), "No conventions found with id " << config.conventionID());
    QuantLib::ext::shared_ptr<CdsConvention> cdsConv =
        QuantLib::ext::dynamic_pointer_cast<CdsConvention>(conventions->get(config.conventionID()));
    QL_REQUIRE(cdsConv, "SpreadCDS curves require CDS convention");

    // Get the discount curve for use in the CDS spread curve bootstrap
    auto it = yieldCurves.find(config.discountCurveID());
    QL_REQUIRE(it != yieldCurves.end(), "The discount curve, " << config.discountCurveID()
                                                               << ", required in the building of the curve, "
                                                               << spec.name() << ", was not found.");
    Handle<YieldTermStructure> discountCurve = it->second->handle();

    // Get the CDS spread / price curve quotes
    set<QuoteData> quotes = getConfiguredQuotes(curveID, config, asof, loader);

    // Set up ref data for the curve, except runningSpread which is set below
    QuantExt::CreditCurve::RefData refData;
    refData.indexTerm = config.indexTerm();
    refData.startDate = config.startDate();
    refData.tenor = Period(cdsConv->frequency());
    refData.calendar = cdsConv->calendar();
    refData.convention = cdsConv->paymentConvention();
    refData.termConvention = cdsConv->paymentConvention();
    refData.rule = cdsConv->rule();
    refData.payConvention = cdsConv->paymentConvention();
    refData.dayCounter = cdsConv->dayCounter();
    refData.lastPeriodDayCounter = cdsConv->lastPeriodDayCounter();
    refData.cashSettlementDays = cdsConv->upfrontSettlementDays();

    // If the configuration instructs us to imply a default from the market data, we do it here.
    if (config.implyDefaultFromMarket() && *config.implyDefaultFromMarket()) {
        if (recoveryRate_ != Null<Real>() && quotes.empty()) {
            // Assume entity is in default, between event determination date and auction date. Build a survival
            // probability curve with value 0.0 tomorrow to approximate this and allow dependent instruments to price.
            // Need to use small but positive numbers to avoid downstream issues with log linear survivals e.g. below
            // and in places like ScenarioSimMarket.
            vector<Date> dates{asof, asof + 1 * Years, asof + 10 * Years};
            vector<Real> survivalProbs{1.0, 1e-16, 1e-18};
            curve_ = QuantLib::ext::make_shared<QuantExt::CreditCurve>(
                Handle<DefaultProbabilityTermStructure>(
                    QuantLib::ext::make_shared<QuantExt::InterpolatedSurvivalProbabilityCurve<LogLinear>>(
                        dates, survivalProbs, config.dayCounter(), Calendar(), std::vector<Handle<Quote>>(),
                        std::vector<Date>(), LogLinear())),
                discountCurve, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(recoveryRate_)), refData);
            curve_->curve()->enableExtrapolation();
            WLOG("DefaultCurve: recovery rate found but no CDS quotes for "
                 << curveID << " and "
                 << "ImplyDefaultFromMarket is true. Curve built that gives default immediately.");
            return;
        }
    } else {
        QL_REQUIRE(!quotes.empty(), "No market points found for CDS curve config " << curveID);
    }

    // Create the CDS instrument helpers, only keep alive helpers
    vector<QuantLib::ext::shared_ptr<QuantExt::DefaultProbabilityHelper>> helpers;
    std::map<QuantLib::Date, QuantLib::Period> helperQuoteTerms;
    Real runningSpread = Null<Real>();
    QuantExt::CreditDefaultSwap::ProtectionPaymentTime ppt = cdsConv->paysAtDefaultTime()
                                                                 ? QuantExt::CreditDefaultSwap::atDefault
                                                                 : QuantExt::CreditDefaultSwap::atPeriodEnd;

    if (config.type() == DefaultCurveConfig::Config::Type::SpreadCDS) {
        for (auto quote : quotes) {
            QuantLib::ext::shared_ptr<SpreadCdsHelper> tmp;
            try {
                tmp = QuantLib::ext::make_shared<SpreadCdsHelper>(
                    quote.value, quote.term, cdsConv->settlementDays(), cdsConv->calendar(), cdsConv->frequency(),
                    cdsConv->paymentConvention(), cdsConv->rule(), cdsConv->dayCounter(), recoveryRate_, discountCurve,
                    cdsConv->settlesAccrual(), ppt, config.startDate(), cdsConv->lastPeriodDayCounter());

            } catch (exception& e) {
                if (quote.term == Period(0, Months)) {
                    WLOG("DefaultCurve:: Cannot add quote of term 0M to CDS curve " << curveID << " for asof date "
                                                                                    << asof);
                } else {
                    QL_FAIL("DefaultCurve:: Failed to add quote of term " << quote.term << " to CDS curve " << curveID
                                                                          << " for asof date " << asof
                                                                          << ", with error: " << e.what());
                }
            }
            if (tmp) {
                if (tmp->latestDate() > asof) {
                    helpers.push_back(tmp);
                    runningSpread = config.runningSpread();
                }
                helperQuoteTerms[tmp->latestDate()] = quote.term;
            }
        }
    } else {
        for (auto quote : quotes) {
            // If there is no running spread encoded in the quote, the config must have one.
            runningSpread = quote.runningSpread;
            if (runningSpread == Null<Real>()) {
                QL_REQUIRE(config.runningSpread() != Null<Real>(),
                           "A running spread was not provided in the quote "
                               << "string so it must be provided in the config for CDS upfront curve " << curveID);
                runningSpread = config.runningSpread();
            }
            auto tmp = QuantLib::ext::make_shared<UpfrontCdsHelper>(
                quote.value, runningSpread, quote.term, cdsConv->settlementDays(), cdsConv->calendar(),
                cdsConv->frequency(), cdsConv->paymentConvention(), cdsConv->rule(), cdsConv->dayCounter(),
                recoveryRate_, discountCurve, cdsConv->upfrontSettlementDays(), cdsConv->settlesAccrual(), ppt,
                config.startDate(), cdsConv->lastPeriodDayCounter());
            if (tmp->latestDate() > asof) {
                helpers.push_back(tmp);
            }
            helperQuoteTerms[tmp->latestDate()] = quote.term;
        }
    }

    QL_REQUIRE(!helpers.empty(), "DefaultCurve: no alive quotes found.");

    refData.runningSpread = runningSpread;

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

    typedef PiecewiseDefaultCurve<QuantExt::SurvivalProbability, LogLinear, QuantExt::IterativeBootstrap> SpCurve;
    SpCurve::bootstrap_type btconfig(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                   minFactor, dontThrowSteps);
    QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> qlCurve;

    if (config.indexTerm() != 0 * Days) {

        // build flat index curve picking a quote with identical term as configured if possible
        // otherwise build interpolated curve using the existing quotes

        std::vector<Real> helperTermTimes;
        for (auto const& h : helpers) {
            helperTermTimes.push_back(QuantExt::periodToTime(helperQuoteTerms[h->latestDate()]));
        }

        Real t = QuantExt::periodToTime(config.indexTerm());
        Size helperIndex_m, helperIndex_p;
        Real alpha;
        std::tie(helperIndex_m, helperIndex_p, alpha) = QuantExt::interpolationIndices(helperTermTimes, t);

        auto tmp1 = QuantLib::ext::make_shared<SpCurve>(
            asof, std::vector<QuantLib::ext::shared_ptr<QuantExt::DefaultProbabilityHelper>>{helpers[helperIndex_m]},
            config.dayCounter(), LogLinear(), btconfig);
        Date d1 = helpers[helperIndex_m]->pillarDate();
        Real p1 = tmp1->survivalProbability(d1);
        auto tmp1i = QuantLib::ext::make_shared<QuantExt::InterpolatedSurvivalProbabilityCurve<LogLinear>>(
            std::vector<Date>{asof, d1}, std::vector<Real>{1.0, p1}, config.dayCounter(), Calendar(),
            std::vector<Handle<Quote>>(), std::vector<Date>(), LogLinear(), config.allowNegativeRates());

        if (close_enough(alpha, 1.0)) {
            qlCurve = tmp1i;
        } else {
            auto tmp2 = QuantLib::ext::make_shared<SpCurve>(
                asof, std::vector<QuantLib::ext::shared_ptr<QuantExt::DefaultProbabilityHelper>>{helpers[helperIndex_p]},
                config.dayCounter(), LogLinear(), btconfig);
            Date d2 = helpers[helperIndex_p]->pillarDate();
            Real p2 = tmp2->survivalProbability(d2);
            auto tmp2i = QuantLib::ext::make_shared<QuantExt::InterpolatedSurvivalProbabilityCurve<LogLinear>>(
                std::vector<Date>{asof, d2}, std::vector<Real>{1.0, p2}, config.dayCounter(), Calendar(),
                std::vector<Handle<Quote>>(), std::vector<Date>(), LogLinear(), config.allowNegativeRates());
            tmp1i->enableExtrapolation();
            tmp2i->enableExtrapolation();
            qlCurve = QuantLib::ext::make_shared<QuantExt::TermInterpolatedDefaultCurve>(
                Handle<DefaultProbabilityTermStructure>(tmp1i), Handle<DefaultProbabilityTermStructure>(tmp2i), alpha);
        }

    } else {

        // build single name curve

        QuantLib::ext::shared_ptr<DefaultProbabilityTermStructure> tmp = QuantLib::ext::make_shared<SpCurve>(
            asof, helpers, config.dayCounter(), LogLinear(),
            QuantExt::IterativeBootstrap<SpCurve>(accuracy, globalAccuracy, dontThrow, maxAttempts, maxFactor,
                                                  minFactor, dontThrowSteps));

        // As for yield curves we need to copy the piecewise curve because on eval date changes the relative date
        // helpers with trigger a bootstrap.
        vector<Date> dates;
        vector<Real> survivalProbs;
        dates.push_back(asof);
        survivalProbs.push_back(1.0);

        for (Size i = 0; i < helpers.size(); ++i) {
            if (helpers[i]->latestDate() > asof) {
                Date pillarDate = helpers[i]->pillarDate();
                Probability sp = tmp->survivalProbability(pillarDate);

                // In some cases the bootstrapped survival probability at one tenor will be `close` to that at a
                // previous tenor. Here we don't add that survival probability and date to avoid issues when creating
                // the InterpolatedSurvivalProbabilityCurve below.
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
        if (dates.size() == 1) {
            // We might have removed points above. To make the interpolation work, we need at least two points though.
            dates.push_back(dates.back() + 1);
            survivalProbs.push_back(survivalProbs.back());
        }
        qlCurve = QuantLib::ext::make_shared<QuantExt::InterpolatedSurvivalProbabilityCurve<LogLinear>>(
            dates, survivalProbs, config.dayCounter(), Calendar(), std::vector<Handle<Quote>>(), std::vector<Date>(),
            LogLinear(), config.allowNegativeRates());
    }

    if (config.extrapolation()) {
        qlCurve->enableExtrapolation();
        DLOG("DefaultCurve: Enabled Extrapolation");
    }

    curve_ = QuantLib::ext::make_shared<QuantExt::CreditCurve>(Handle<DefaultProbabilityTermStructure>(qlCurve), discountCurve,
                                                       Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(recoveryRate_)),
                                                       refData);

    LOG("Finished building default curve of type SpreadCDS for curve " << curveID);
}

void DefaultCurve::buildHazardRateCurve(const std::string& curveID, const DefaultCurveConfig::Config& config,
                                        const Date& asof, const DefaultCurveSpec& spec, const Loader& loader) {

    LOG("Start building default curve of type HazardRate for curve " << curveID);

    QL_REQUIRE(config.type() == DefaultCurveConfig::Config::Type::HazardRate,
               "DefaultCurve::buildHazardRateCurve expected a default curve configuration with type HazardRate");

    // Get the hazard rate curve conventions
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    QL_REQUIRE(conventions->has(config.conventionID()), "No conventions found with id " << config.conventionID());
    QuantLib::ext::shared_ptr<CdsConvention> cdsConv =
        QuantLib::ext::dynamic_pointer_cast<CdsConvention>(conventions->get(config.conventionID()));
    QL_REQUIRE(cdsConv, "HazardRate curves require CDS convention");

    // Get the hazard rate quotes
    set<QuoteData> quotes = getConfiguredQuotes(curveID, config, asof, loader);

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
    curve_ = QuantLib::ext::make_shared<QuantExt::CreditCurve>(
        Handle<DefaultProbabilityTermStructure>(QuantLib::ext::make_shared<QuantExt::InterpolatedHazardRateCurve<BackwardFlat>>(
            dates, quoteValues, config.dayCounter(), BackwardFlat(), config.allowNegativeRates())));

    if (config.extrapolation()) {
        curve_->curve()->enableExtrapolation();
        DLOG("DefaultCurve: Enabled Extrapolation");
    }

    if (recoveryRate_ == Null<Real>()) {
        LOG("DefaultCurve: setting recovery rate to 0.0 for hazard rate curve, because none is given.");
        recoveryRate_ = 0.0;
    }

    // Force bootstrap so that errors are thrown during the build, not later
    curve_->curve()->survivalProbability(QL_EPSILON);

    LOG("Finished building default curve of type HazardRate for curve " << curveID);
}

void DefaultCurve::buildBenchmarkCurve(const std::string& curveID, const DefaultCurveConfig::Config& config,
                                       const Date& asof, const DefaultCurveSpec& spec, const Loader& loader,
                                       map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves) {

    LOG("Start building default curve of type Benchmark for curve " << curveID);

    QL_REQUIRE(config.type() == DefaultCurveConfig::Config::Type::Benchmark,
               "DefaultCurve::buildBenchmarkCurve expected a default curve configuration with type Benchmark");

    if (recoveryRate_ == Null<Real>())
        recoveryRate_ = 0.0;

    // Populate benchmark yield curve
    auto it = yieldCurves.find(config.benchmarkCurveID());
    QL_REQUIRE(it != yieldCurves.end(), "The benchmark curve, " << config.benchmarkCurveID()
                                                                << ", required in the building of the curve, "
                                                                << spec.name() << ", was not found.");
    QuantLib::ext::shared_ptr<YieldCurve> benchmarkCurve = it->second;

    // Populate source yield curve
    it = yieldCurves.find(config.sourceCurveID());
    QL_REQUIRE(it != yieldCurves.end(), "The source curve, " << config.sourceCurveID()
                                                             << ", required in the building of the curve, "
                                                             << spec.name() << ", was not found.");
    QuantLib::ext::shared_ptr<YieldCurve> sourceCurve = it->second;

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
    curve_ = QuantLib::ext::make_shared<QuantExt::CreditCurve>(Handle<DefaultProbabilityTermStructure>(
        QuantLib::ext::make_shared<QuantExt::InterpolatedSurvivalProbabilityCurve<LogLinear>>(
            dates, impliedSurvProb, config.dayCounter(), Calendar(), std::vector<Handle<Quote>>(), std::vector<Date>(),
            LogLinear(), config.allowNegativeRates())));

    if (config.extrapolation()) {
        curve_->curve()->enableExtrapolation();
        DLOG("DefaultCurve: Enabled Extrapolation");
    }

    // Force bootstrap so that errors are thrown during the build, not later
    curve_->curve()->survivalProbability(QL_EPSILON);

    LOG("Finished building default curve of type Benchmark for curve " << curveID);
}

void DefaultCurve::buildMultiSectionCurve(const std::string& curveID, const DefaultCurveConfig::Config& config,
                                          const Date& asof, const DefaultCurveSpec& spec, const Loader& loader,
                                          map<string, QuantLib::ext::shared_ptr<DefaultCurve>>& defaultCurves) {
    LOG("Start building default curve of type MultiSection for curve " << curveID);

    std::vector<Handle<DefaultProbabilityTermStructure>> curves;
    std::vector<Handle<Quote>> recoveryRates;
    std::vector<Date> switchDates;

    for (auto const& s : config.multiSectionSourceCurveIds()) {
        auto it = defaultCurves.find(s);
        QL_REQUIRE(it != defaultCurves.end(),
                   "The multi section source curve " << s << " required for " << spec.name() << " was not found.");
        curves.push_back(Handle<DefaultProbabilityTermStructure>(it->second->creditCurve()->curve()));
        recoveryRates.push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(it->second->recoveryRate())));
    }

    for (auto const& d : config.multiSectionSwitchDates()) {
        switchDates.push_back(parseDate(d));
    }

    Handle<Quote> recoveryRate(QuantLib::ext::make_shared<SimpleQuote>(recoveryRate_));
    LOG("DefaultCurve: set up multi section curve with " << curves.size() << " sections");
    curve_ = QuantLib::ext::make_shared<QuantExt::CreditCurve>(
        Handle<DefaultProbabilityTermStructure>(QuantLib::ext::make_shared<QuantExt::MultiSectionDefaultCurve>(
            curves, recoveryRates, switchDates, recoveryRate, config.dayCounter(), config.extrapolation())));

    LOG("Finished building default curve of type MultiSection for curve " << curveID);
}

void DefaultCurve::buildTransitionMatrixCurve(const std::string& curveID, const DefaultCurveConfig::Config& config,
                                              const Date& asof, const DefaultCurveSpec& spec, const Loader& loader,
                                              map<string, QuantLib::ext::shared_ptr<DefaultCurve>>& defaultCurves) {
    LOG("Start building default curve of type TransitionMatrix for curve " << curveID);
    Size dim = config.states().size();
    QL_REQUIRE(dim >= 2,
               "DefaultCurve::buildTransitionMatrixCurve(): transition matrix dimension >= 2 required, found " << dim);
    Matrix transitionMatrix(dim, dim, Null<Real>());
    map<string, Size> stateIndex;
    for (Size i = 0; i < config.states().size(); ++i)
        stateIndex[config.states()[i]] = i;
    QL_REQUIRE(!config.cdsQuotes().empty(), "DefaultCurve::buildTransitionMatrixCurve(): not quotes given.");
    std::vector<std::string> tmp;
    std::transform(config.cdsQuotes().begin(), config.cdsQuotes().end(), std::back_inserter(tmp),
                   [](const std::pair<std::string, bool>& p) { return p.first; });
    auto wildcard = getUniqueWildcard(tmp);
    std::set<QuantLib::ext::shared_ptr<MarketDatum>> mdData;
    if (wildcard) {
        mdData = loader.get(*wildcard, asof);
    } else {
        for (auto const& q : config.cdsQuotes()) {
            if (auto m = loader.get(q, asof))
                mdData.insert(m);
        }
    }
    for (const auto& md : mdData) {
        QL_REQUIRE(md->instrumentType() == MarketDatum::InstrumentType::RATING,
                   "DefaultCurve::buildTransitionMatrixCurve(): quote instrument type must be RATING.");
        QuantLib::ext::shared_ptr<TransitionProbabilityQuote> q = QuantLib::ext::dynamic_pointer_cast<TransitionProbabilityQuote>(md);
        Size i = stateIndex[q->fromRating()];
        Size j = stateIndex[q->toRating()];
        transitionMatrix[i][j] = q->quote()->value();
    }
    for (Size i = 0; i < dim; ++i) {
        for (Size j = 0; j < dim; ++j) {
            QL_REQUIRE(transitionMatrix[i][j] != Null<Real>(),
                       "DefaultCurve::buildTransitionMatrixCurve():matrix element "
                           << config.states()[i] << " -> " << config.states()[j] << " missing in market data");
        }
    }
    Size initialStateIndex = stateIndex[config.initialState()];
    curve_ = QuantLib::ext::make_shared<QuantExt::CreditCurve>(
        Handle<DefaultProbabilityTermStructure>(QuantLib::ext::make_shared<QuantExt::GeneratorDefaultProbabilityTermStructure>(
            QuantExt::GeneratorDefaultProbabilityTermStructure::MatrixType::Transition, transitionMatrix,
            initialStateIndex, asof)));
    if (recoveryRate_ == Null<Real>())
        recoveryRate_ = 0.0;
    LOG("Finished building default curve of type TransitionMatrix for curve " << curveID);
}

void DefaultCurve::buildNullCurve(const std::string& curveID, const DefaultCurveConfig::Config& config,
                                  const Date& asof, const DefaultCurveSpec& spec) {
    LOG("Start building null default curve for " << curveID);
    curve_ = QuantLib::ext::make_shared<QuantExt::CreditCurve>(Handle<DefaultProbabilityTermStructure>(
        QuantLib::ext::make_shared<QuantLib::FlatHazardRate>(asof, 0.0, config.dayCounter())));
    recoveryRate_ = 0.0;
    LOG("Finished building default curve of type Null for curve " << curveID);
}

} // namespace data
} // namespace ore
