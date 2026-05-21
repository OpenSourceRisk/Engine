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

#include <algorithm>
#include <ored/marketdata/cdsvolcurve.hpp>
#include <ored/marketdata/structuredcurveerror.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/wildcard.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/termstructures/creditvolcurve.hpp>
#include <qle/termstructures/indexcdsvolstripper.hpp>
#include <qle/termstructures/svimodeltraits.hpp>
#include <qle/utilities/time.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

using IndexFactors = CDSVolatilityCurveConfig::PriceInfo::IndexFactors;

namespace {

    // Logic to check that quote matches configured expiries and strikes if provided.
    bool quoteMatchesStrikeExpiry(const ext::shared_ptr<IndexCDSOptionQuote>& q, Real quoteStrike,
        const vector<ext::shared_ptr<Expiry>>& configuredExpiries,
        const vector<Real>& configuredStrikes)
    {
        // Check that the quote matches one of the expiries if given.
        bool result = true;
        if (!configuredExpiries.empty()) {
            auto expiryIt = find_if(configuredExpiries.begin(), configuredExpiries.end(),
                [&q](QuantLib::ext::shared_ptr<Expiry> e) { return *e == *q->expiry(); });
            if (expiryIt == configuredExpiries.end())
                result = false;
        }

        // If we have been given a list of explicit strikes, check that the quote matches one of them.
        // Move to the next quote if it does not.
        if (!configuredStrikes.empty()) {
            auto strikeIt = find_if(configuredStrikes.begin(), configuredStrikes.end(),
                [quoteStrike](Real s) { return close(s, quoteStrike); });
            if (strikeIt == configuredStrikes.end())
                result = false;
        }

        return result;
    }

    // Logic to populate the index CDS term given a quote and a volatility curve configuration.
    // If the return is not populated, the quote will be ignored in the loading.
    /* - Load quotes with empty term only if there is zero or one term specified in the volatility curve config.
         If zero terms are specified in the curve config, the term of the quote is set to 5Y.
       - Load quotes with a term, if:
           1. no term is specified in the volatility curve config; or
           2. they match a term specified in the volatility curve config.
    */
    ext::optional<Period> getQuoteTerm(const ext::shared_ptr<IndexCDSOptionQuote>& q,
        const CDSVolatilityCurveConfig& vc)
    {
        ext::optional<Period> term;
        if (q->indexTerm().empty()) {
            if (vc.terms().empty())
                term = 5 * Years;
            else if (vc.terms().size() == 1)
                term = vc.terms().front();
            // term left uninitialised if vc.terms() has more than one element.
        } else {
            Period tmp = parsePeriod(q->indexTerm());
            if (vc.terms().empty() || std::find(vc.terms().begin(), vc.terms().end(), tmp) != vc.terms().end())
                term = tmp;
        }

        return term;
    }

    using OptEng = IndexCdsVolStripper::OptionEngine;
    OptEng getOptionEngine(const string& engineOverride)
    {
        if (engineOverride.empty()) {
            return OptEng::None;
        } else if (engineOverride == "BlackIndexCdsOptionEngine") {
            return OptEng::Black;
        } else if (engineOverride == "NumericalIntegrationEngine") {
            return OptEng::Numerical;
        } else {
            WLOG("Invalid option engine override (" << engineOverride << ") specified in CDS volatility curve"
                " configuration. The default will be used in option stripping.");
            return OptEng::None;
        }
    }

    Solver1DOptions getSolverOptions(const ext::optional<OneDimSolverConfig>& solverConfig,
        QuantExt::CdsOption::StrikeType strikeType)
    {
        // If given an explicit configuration, use the solver options created from it.
        if (solverConfig)
            return *solverConfig;

        // Use a conservative best guess (based on data as of 1 Apr 2026)
        using ST = QuantExt::CdsOption::StrikeType;
        Size maxEvals = 200;
        Real accuracy = 0.00001;
        if (strikeType == ST::Price)
            return Solver1DOptions{ maxEvals, accuracy, 0.15, std::make_pair(0.0001, 1.5), 0.015, 0.0001, 1.5 };
        else
            return Solver1DOptions{ maxEvals, accuracy, 1.10, std::make_pair( 0.001, 5.0), 0.050,  0.001, 5.0 };
    }

    IndexFactors getIndexFactors(const string& indexId, const ext::shared_ptr<ReferenceDataManager>& referenceData)
    {
        IndexFactors indexFactors;
        auto [hasData, refDatum] = referenceData->tryGetData(CreditIndexReferenceDatum::TYPE, indexId);

        // A number checks before we can proceed.
        if (!hasData) {
            WLOG("CDSVolCurve: reference data manager does not have reference data for index " << indexId <<
                ". Will proceed as if we have version 1 of the index i.e. default index factor of 1 and FEP of 0.");
            return indexFactors;
        }

        auto ciRefDatum = ext::dynamic_pointer_cast<CreditIndexReferenceDatum>(refDatum);
        if (!ciRefDatum->indexVersion() || *ciRefDatum->indexVersion() == 0) {
            WLOG("CDSVolCurve: credit index reference datum for index " << indexId << " does not have a version. " <<
                "Will proceed as if we have version 1 of the index i.e. default index factor of 1 and FEP of 0.");
            return indexFactors;
        }
        Size indexVersion = *ciRefDatum->indexVersion();

        // Derive the current index factor (`indexFactor` below), the index factor relevant for the given index version 
        // (`indexFactorStrike` below) and the FEP relevant for the given index version from the constituents.
        map<Date, pair<Real, Real>> defaultInfo;
        Real indexFactor = 1.0;
        for (const CreditIndexConstituent& c : ciRefDatum->constituents()) {
            if (c.priorWeight() == Null<Real>())
                continue;
            indexFactor -= c.priorWeight();
            defaultInfo.try_emplace(c.auctionDate(), std::make_pair(c.priorWeight(), c.recovery()));
        }

        // If version is 1 and there are no defaults, can just proceed with default `IndexFactors`. Usual case.
        if (defaultInfo.empty() && indexVersion == 1)
            return indexFactors;

        if (defaultInfo.empty() && indexVersion > 1) {
            WLOG("CDSVolCurve: credit index reference datum for index " << indexId << " has no default data " <<
                "but the version is " << indexVersion << " (> 1). Will proceed as if we have version 1 of the index " <<
                "i.e. default index factor of 1 and FEP of 0 but the index data should be checked.");
            return indexFactors;
        }

        if (indexFactor < 0) {
            WLOG("CDSVolCurve: credit index reference datum for index " << indexId << " implies a negative index " <<
            "factor (" << indexFactor << "). Will proceed as if we have version 1 of the index i.e. default " <<
            "index factor of 1 and FEP of 0 but the index data should be checked.");
            return indexFactors;
        }

        // If version is 2, should be at least one default, if version is 3, should be at least 2 defaults etc.
        if (defaultInfo.size() < indexVersion - 1) {
            WLOG("CDSVolCurve: credit index reference datum for index " << indexId << " has information on only " <<
                defaultInfo.size() << " defaults but we are looking for information on version " << indexVersion <<
                " of the index. Will proceed as if we have version 1 of the index i.e. default index factor of " <<
                "1 and FEP of 0 but the index data should be checked.");
            return indexFactors;
        }

        // If version is 1: `indexFactorStrike` is 1 and `realisedFep` is \Sum_{i=1}^{N} (1 - R_i) w_i.
        // If version is 2: `indexFactorStrike` is 1 - w_1 and `realisedFep` is \Sum_{i=2}^{N} (1 - R_i) w_i.
        Size count = 0;
        indexFactors.indexFactor = indexFactor;
        for (auto it = defaultInfo.begin(); it != defaultInfo.end(); ++it, ++count) {
            if (count < indexVersion - 1)
                indexFactors.indexFactorStrike -= it->second.first;
            else
                indexFactors.realisedFep += it->second.first * (1 - it->second.second);
        }

        // Log a warning if the calculated `indexFactorStrike` does not match the index factor given for this version 
        // of the index in the reference datum.
        if (ciRefDatum->indexFactor() && !close_enough(indexFactors.indexFactorStrike, *ciRefDatum->indexFactor())) {
            WLOG("CDSVolCurve: the calculated index factor for index " << indexId << " is " <<
                indexFactors.indexFactorStrike << " but the credit index reference datum gives the index factor as " <<
                *ciRefDatum->indexFactor() << ". We will use the calculated index factor but the index data " <<
                "should be checked.");
        }

        return indexFactors;
    }
}

CDSVolCurve::CDSVolCurve(Date asof, CDSVolatilityCurveSpec spec, const Loader& loader,
                         const CurveConfigurations& curveConfigs,
                         const CDSVolCurveCache& requiredCdsVolCurves,
                         const DefaultCurveCache& requiredCdsCurves,
                         const ext::shared_ptr<ReferenceDataManager>& referenceData) {

    try {

        LOG("CDSVolCurve: start building CDS volatility structure with ID " << spec.curveConfigID());

        QL_REQUIRE(curveConfigs.hasCdsVolCurveConfig(spec.curveConfigID()), "No curve configuration found for CDS "
                                                                                << "volatility curve spec with ID "
                                                                                << spec.curveConfigID() << ".");
        auto config = *curveConfigs.cdsVolCurveConfig(spec.curveConfigID());

        calendar_ = parseCalendar(config.calendar());
        dayCounter_ = parseDayCounter(config.dayCounter());

        strikeType_ = config.strikeType() == "Price" ? CreditVolCurve::Type::Price : CreditVolCurve::Type::Spread;

        // Do different things depending on the type of volatility configured
        QuantLib::ext::shared_ptr<VolatilityConfig> vc = config.volatilityConfig();
        if (auto cvc = QuantLib::ext::dynamic_pointer_cast<ConstantVolatilityConfig>(vc)) {
            buildVolatility(asof, config, *cvc, loader);
        } else if (auto vcc = QuantLib::ext::dynamic_pointer_cast<VolatilityCurveConfig>(vc)) {
            buildVolatility(asof, config, *vcc, loader);
        } else if (auto vssc = QuantLib::ext::dynamic_pointer_cast<VolatilityStrikeSurfaceConfig>(vc)) {
            buildVolatility(asof, config, *vssc, loader, requiredCdsCurves, referenceData);
        } else if (auto vdsc = QuantLib::ext::dynamic_pointer_cast<VolatilityDeltaSurfaceConfig>(vc)) {
            QL_FAIL("CDSVolCurve does not support a VolatilityDeltaSurfaceConfig yet.");
        } else if (auto vmsc = QuantLib::ext::dynamic_pointer_cast<VolatilityMoneynessSurfaceConfig>(vc)) {
            QL_FAIL("CDSVolCurve does not support a VolatilityMoneynessSurfaceConfig yet.");
        } else if (auto vapo = QuantLib::ext::dynamic_pointer_cast<VolatilityApoFutureSurfaceConfig>(vc)) {
            QL_FAIL("VolatilityApoFutureSurfaceConfig does not make sense for CDSVolCurve.");
        } else if (auto vpc = QuantLib::ext::dynamic_pointer_cast<CDSProxyVolatilityConfig>(vc)) {
            buildVolatility(asof, spec, config, *vpc, requiredCdsVolCurves, requiredCdsCurves);
        } else {
            QL_FAIL("Unexpected VolatilityConfig in CDSVolatilityConfig");
        }

        LOG("CDSVolCurve: finished building CDS volatility structure with ID " << spec.curveConfigID());

    } catch (exception& e) {
        QL_FAIL("CDS volatility curve building for ID " << spec.curveConfigID() << " failed : " << e.what());
    } catch (...) {
        QL_FAIL("CDS volatility curve building for ID " << spec.curveConfigID() << " failed: unknown error");
    }
}

void CDSVolCurve::buildVolatility(const Date& asof, const CDSVolatilityCurveConfig& vc,
                                  const ConstantVolatilityConfig& cvc, const Loader& loader) {

    LOG("CDSVolCurve: start building constant volatility structure");

    const QuantLib::ext::shared_ptr<MarketDatum>& md = loader.get(cvc.quote(), asof);
    QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");
    QL_REQUIRE(md->instrumentType() == MarketDatum::InstrumentType::INDEX_CDS_OPTION,
        "MarketDatum instrument type '" << md->instrumentType() << "' <> 'MarketDatum::InstrumentType::INDEX_CDS_OPTION'");
    QuantLib::ext::shared_ptr<IndexCDSOptionQuote> q = QuantLib::ext::dynamic_pointer_cast<IndexCDSOptionQuote>(md);
    QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to IndexCDSOptionQuote");
    QL_REQUIRE(q->name() == cvc.quote(),
        "IndexCDSOptionQuote name '" << q->name() << "' <> ConstantVolatilityConfig quote '" << cvc.quote() << "'");
    TLOG("Found the constant volatility quote " << q->name());
    Handle<Quote> quote = q->quote();

    DLOG("Creating CreditVolCurve structure");

    CreditVolQuoteMap quotes;
    quotes[std::make_tuple(asof + 1 * Years, 5 * Years, strikeType_ == CreditVolCurve::Type::Price ? 1.0 : 0.0)] =
        quote;
    vol_ = QuantLib::ext::make_shared<QuantExt::InterpolatingCreditVolCurve>(
        asof, calendar_, Following, dayCounter_, std::vector<QuantLib::Period>{},
        std::vector<Handle<QuantExt::CreditCurve>>{}, quotes, strikeType_);

    LOG("CDSVolCurve: finished building constant volatility structure");
}

void CDSVolCurve::buildVolatility(const QuantLib::Date& asof, const CDSVolatilityCurveConfig& vc,
                                  const VolatilityCurveConfig& vcc, const Loader& loader) {

    LOG("CDSVolCurve: start building 1-D volatility curve");

    // Must have at least one quote
    QL_REQUIRE(vcc.quotes().size() > 0, "No quotes specified in config " << vc.curveID());

    // Check if we are using a regular expression to select the quotes for the curve. If we are, the quotes should
    // contain exactly one element.
    auto wildcard = getUniqueWildcard(vcc.quotes());

    // curveData will be populated with the expiry dates and volatility values.
    CreditVolQuoteMap quotes;

    // Different approaches depending on whether we are using a regex or searching for a list of explicit quotes.
    if (wildcard) {

        DLOG("Have single quote with pattern " << wildcard->pattern());

        // Loop over quotes and process CDS option quotes matching pattern on asof
        for (const auto& md : loader.get(*wildcard, asof)) {

            QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

            auto q = QuantLib::ext::dynamic_pointer_cast<IndexCDSOptionQuote>(md);
            QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to IndexCDSOptionQuote");

            TLOG("The quote " << q->name() << " matched the pattern");

            auto quoteTerm = getQuoteTerm(q, vc);
            if (!quoteTerm)
                continue;

            Date expiryDate = getExpiry(asof, q->expiry());
            if (expiryDate > asof) {
                Real strike = strikeType_ == CreditVolCurve::Type::Price ? 1.0 : 0.0;
                quotes[{expiryDate, *quoteTerm, strike}] = q->quote();
                TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," << fixed
                                    << setprecision(9) << q->quote()->value() << ")");
            }
        }

        // Check that we have quotes in the end
        QL_REQUIRE(quotes.size() > 0, "No quotes found matching regular expression " << vcc.quotes()[0]);

    } else {

        DLOG("Have " << vcc.quotes().size() << " explicit quotes");

        // Loop over quotes and process CDS option quotes that are explicitly specified in the config.
        Wildcard w("INDEX_CDS_OPTION/*");
        for (const auto& md : loader.get(w, asof)) {

            QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

            auto q = QuantLib::ext::dynamic_pointer_cast<IndexCDSOptionQuote>(md);
            QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to IndexCDSOptionQuote");

            // Find quote name in configured quotes.
            auto it = find(vcc.quotes().begin(), vcc.quotes().end(), q->name());
            if (it != vcc.quotes().end()) {
                TLOG("Found the configured quote " << q->name());

                Date expiryDate = getExpiry(asof, q->expiry());
                QL_REQUIRE(expiryDate > asof, "CDS volatility quote '" << q->name() << "' has expiry in the past ("
                                                                       << io::iso_date(expiryDate) << ")");
                // we load all quotes, just populate the term of empty quotes
                QuantLib::Period quoteTerm;
                if (q->indexTerm().empty()) {
                    quoteTerm = vc.terms().size() == 1 ? vc.terms().front() : 5 * Years;
                } else {
                    quoteTerm = parsePeriod(q->indexTerm());
                }
                quotes[std::make_tuple(expiryDate, quoteTerm,
                                       strikeType_ == CreditVolCurve::Type::Price ? 1.0 : 0.0)] = q->quote();
                TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," << fixed
                                    << setprecision(9) << q->quote()->value() << ")");
            }
        }

        // Check that we have found all of the explicitly configured quotes
        QL_REQUIRE(quotes.size() == vcc.quotes().size(), "Found " << quotes.size() << " quotes, but "
                                                                  << vcc.quotes().size()
                                                                  << " quotes were given in config.");
    }

    DLOG("Creating InterpolatingCreditVolCurve object.");
    vol_ = QuantLib::ext::make_shared<QuantExt::InterpolatingCreditVolCurve>(
        asof, calendar_, Following, dayCounter_, std::vector<QuantLib::Period>{},
        std::vector<Handle<QuantExt::CreditCurve>>{}, quotes, strikeType_);

    LOG("CDSVolCurve: finished building 1-D volatility curve");
}

void CDSVolCurve::buildVolatility(const Date& asof, const CDSVolatilityCurveSpec& spec,
                                  const CDSVolatilityCurveConfig& vc, const CDSProxyVolatilityConfig& pvc,
                                  const CDSVolCurveCache& requiredCdsVolCurves,
                                  const DefaultCurveCache& requiredCdsCurves)
{
    LOG("CDSVolCurve: start building proxy volatility surface");
    auto proxyVolCurve = requiredCdsVolCurves.find(CDSVolatilityCurveSpec(pvc.cdsVolatilityCurve()).name());
    QL_REQUIRE(proxyVolCurve != requiredCdsVolCurves.end(), "CDSVolCurve: Failed to find cds vol curve '"
        << pvc.cdsVolatilityCurve() << "' when building '" << spec.name() << "'");

    vector<Period> terms;
    vector<Handle<CreditCurve>> termCurves;
    populateTermCurves(vc, requiredCdsCurves, terms, termCurves);
    LOG("CDSVolCurve: will use " << termCurves.size() << " term curves in target surface, " << spec.name() <<
        ", to determine atm levels and moneyness-adjustments");

    auto hProxy = Handle<CreditVolCurve>(proxyVolCurve->second->volTermStructure());
    vol_ = ext::make_shared<ProxyCreditVolCurve>(hProxy, terms, termCurves);

    LOG("CDSVolCurve: finished building proxy volatility surface.");
}

void CDSVolCurve::buildVolatility(const Date& asof, CDSVolatilityCurveConfig& vc,
                                  const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
                                  const DefaultCurveCache& requiredCdsCurves,
                                  const ext::shared_ptr<ReferenceDataManager>& referenceData) {

    LOG("CDSVolCurve: start building 2-D volatility absolute strike surface");

    // We are building a cds volatility surface here of the form expiry vs strike where the strikes are absolute
    // numbers. The list of expiries may be explicit or contain a single wildcard character '*'. Similarly, the list
    // of strikes may be explicit or contain a single wildcard character '*'. So, we have four options here.
    // 1. explicit strikes and explicit expiries
    // 2. wildcard strikes and/or wildcard expiries (3 combinations)
    // All variants are handled by CreditVolCurve.

    bool expWc = false;
    if (find(vssc.expiries().begin(), vssc.expiries().end(), "*") != vssc.expiries().end()) {
        expWc = true;
        QL_REQUIRE(vssc.expiries().size() == 1, "Wild card expiry specified but more expiries also specified.");
        DLOG("Have expiry wildcard pattern " << vssc.expiries()[0]);
    }

    bool strkWc = false;
    if (find(vssc.strikes().begin(), vssc.strikes().end(), "*") != vssc.strikes().end()) {
        strkWc = true;
        QL_REQUIRE(vssc.strikes().size() == 1, "Wild card strike specified but more strikes also specified.");
        DLOG("Have strike wildcard pattern " << vssc.strikes()[0]);
    }

    // If we do not have a strike wild card, we expect a list of absolute strike values
    vector<Real> configuredStrikes;
    if (!strkWc) {
        // Parse the list of absolute strikes
        configuredStrikes = parseVectorOfValues<Real>(vssc.strikes(), &parseReal);
        sort(configuredStrikes.begin(), configuredStrikes.end(), [](Real x, Real y) { return !close(x, y) && x < y; });
        QL_REQUIRE(adjacent_find(configuredStrikes.begin(), configuredStrikes.end(),
                                 [](Real x, Real y) { return close(x, y); }) == configuredStrikes.end(),
                   "The configured strikes contain duplicates");
        DLOG("Parsed " << configuredStrikes.size() << " unique configured absolute strikes");
    }

    // If we do not have an expiry wild card, parse the configured expiries.
    vector<ext::shared_ptr<Expiry>> configuredExpiries;
    if (!expWc) {
        // Parse the list of expiry strings.
        for (const string& strExpiry : vssc.expiries()) {
            configuredExpiries.push_back(parseExpiry(strExpiry));
        }
        DLOG("Parsed " << configuredExpiries.size() << " unique configured expiries");
    }

    // Delegate to the appropriate method.
    BuildVolatilityArgs args{asof, vc, vssc, loader, configuredExpiries, configuredStrikes, requiredCdsCurves};
    const bool wildcard = expWc || strkWc;
    const bool viaPremia = vssc.quoteType() == MarketDatum::QuoteType::PRICE;

    if (viaPremia) {
        // Get the quotes.
        PremiumQuoteCube quotes;
        populateVolatilityPremiaQuotes(args, quotes);
        buildVolatilityViaPremia(args, quotes, referenceData);
    } else {
        CreditVolQuoteMap quotes;
        populateVolatilityQuotes(args, quotes);
        setVolatilityCurve(asof, vc, vssc, quotes, args.requiredCdsCurves, !wildcard);
    }
}

void CDSVolCurve::populateVolatilityQuotes(const BuildVolatilityArgs& args, CreditVolQuoteMap& quotes)
{
    DLOG("CDSVolCurve: start populating volatility quotes.");

    // Process relevant CDS option volatility quotes.
    const Date& asof = args.asof;
    CDSVolatilityCurveConfig& vc = args.vc;
    Wildcard w("INDEX_CDS_OPTION/RATE_LNVOL/*");
    for (const auto& md : args.loader.get(w, asof))
    {
        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

        // Go to next quote if not a CDS option quote.
        auto q = ext::dynamic_pointer_cast<IndexCDSOptionQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to IndexCDSOptionQuote");

        // Go to next quote if index name in the quote does not match the cds vol configuration name.
        if (vc.curveID() != q->indexName() && vc.quoteName() != q->indexName())
            continue;

        // This surface is for absolute strikes only.
        auto strike = ext::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
        if (!strike)
            continue;

        // If expiries or strikes are given, check that the quote matches one of them.
        if (!quoteMatchesStrikeExpiry(q, strike->strike(), args.configuredExpiries, args.configuredStrikes))
            continue;

        auto quoteTerm = getQuoteTerm(q, vc);
        if (!quoteTerm)
            continue;

        // If we make it here, add the data to the map
        Date expiryDate = getExpiry(asof, q->expiry());
        quotes[{expiryDate, *quoteTerm, strike->strike() / vc.strikeFactor()}] = q->quote();

        TLOG("Added quote " << q->name() << ": (" << q->expiry() << "," << fixed << setprecision(9) << strike->strike()
            << "," << q->quote()->value() << ")");
    }

    QL_REQUIRE(!quotes.empty(), "CDSVolCurve: found no volatility quotes for curve " << vc.curveID() << ".");

    DLOG("CDSVolCurve: finished populating volatility quotes.");
}

void CDSVolCurve::populateVolatilityPremiaQuotes(const BuildVolatilityArgs& args, PremiumQuoteCube& quotes)
{
    DLOG("CDSVolCurve: start populating volatility premia quotes.");

    // Store quotes by term, expiry in OptionPrice structs.
    using OptionPrice = IndexCdsVolStripper::OptionPrice;

    // Process relevant CDS option premium quotes.
    const Date& asof = args.asof;
    CDSVolatilityCurveConfig& vc = args.vc;
    Wildcard w("INDEX_CDS_OPTION/PRICE/*");
    for (const auto& md : args.loader.get(w, asof))
    {
        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

        // Go to next quote if not a CDS option quote.
        auto q = ext::dynamic_pointer_cast<IndexCDSOptionQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to IndexCDSOptionQuote");

        // Go to next quote if index name in the quote does not match the cds vol configuration name.
        if (vc.curveID() != q->indexName() && vc.quoteName() != q->indexName())
            continue;

        // This surface is for absolute strikes only.
        auto strike = ext::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
        if (!strike)
            continue;

        // If expiries or strikes are given, check that the quote matches one of them.
        if (!quoteMatchesStrikeExpiry(q, strike->strike(), args.configuredExpiries, args.configuredStrikes))
            continue;

        // Quote term must be populated in the configuration and in the quote for price quotes.
        Period quoteTerm = parsePeriod(q->indexTerm());
        if (std::find(vc.terms().begin(), vc.terms().end(), quoteTerm) == vc.terms().end())
            continue;

        // Get the expiry date.
        Date expiryDate = getExpiry(asof, q->expiry());

        // Add or update existing OptionPrice in the cube for this expiry and term.
        Real strikeValue = strike->strike() / vc.strikeFactor();
        vector<OptionPrice>& prices = quotes[quoteTerm][expiryDate];
        auto priceIt = find_if(prices.begin(), prices.end(), [&strikeValue](const OptionPrice& p) {
            return close(p.strike, strikeValue);
        });

        if (priceIt != prices.end()) {
            if ((q->side() == Protection::Buyer && !priceIt->payerPrice.empty()) ||
                (q->side() == Protection::Seller && !priceIt->receiverPrice.empty())) {
                WLOG("Duplicate quote found for expiry " << expiryDate << ", term " << quoteTerm << ", strike "
                    << strike->strike() << " and side " << q->side()  << ". Ignoring this quote: " << q->name() << ".");
                continue;
            }
        }

        OptionPrice& op = priceIt == prices.end() ? prices.emplace_back() : *priceIt;

        // Add the quote.
        op.strike = strikeValue;
        if (q->side() == Protection::Buyer)
            op.payerPrice = q->quote();
        else
            op.receiverPrice = q->quote();

        TLOG("Added quote " << q->name() << ": (" << q->expiry() << "," << fixed << setprecision(9) << strike->strike()
            << "," << q->quote()->value() << ")");
    }

    QL_REQUIRE(!quotes.empty(), "CDSVolCurve: found no premium quotes for curve " << vc.curveID() << ".");

    DLOG("CDSVolCurve: finished populating volatility premia quotes.");
}

void CDSVolCurve::buildVolatilityViaPremia(const BuildVolatilityArgs& args, const PremiumQuoteCube& quotes,
    const ext::shared_ptr<ReferenceDataManager>& referenceData)
{
    DLOG("CDSVolCurve: start building 2-D volatility absolute strike surface via premium stripping.");

    const auto& vc = args.vc;

    // Make sure that we have a valid price information node in the configuration or we can't proceed.
    QL_REQUIRE(vc.priceInfo(), "CDSVolCurve: to build a volatility structure from price quotes, we need a valid "
        "price information node in the configuration.");
    const auto& priceInfo = *vc.priceInfo();

    // Validation on term curves, terms and term maturities.
    // These are checked in the configuration creation but better to double check here.
    const auto& terms = vc.terms();
    const auto& termCrvNames = vc.termCurves();
    const auto& maturities = vc.termMaturities();
    QL_REQUIRE(terms.size() == termCrvNames.size(), "CDSVolCurve: terms (" << terms.size() <<
        ") and term curves (" << termCrvNames.size() << ") size mismatch.");
    QL_REQUIRE(terms.size() == maturities.size(), "CDSVolCurve: terms (" << terms.size() <<
        ") and term maturities (" << maturities.size() << ") size mismatch.");

    // Get the term curves and term maturities required by IndexCdsVolStripper.
    map<Period, Handle<CreditCurve>> termCurves;
    map<Period, Date> termMaturities;
    for (Size i = 0; i < termCrvNames.size(); ++i)
    {
        const string& termCrvName = termCrvNames[i];
        QL_REQUIRE(!termCrvName.empty(), "CDSVolCurve: term curves should not be empty for CDS "
            "volatility premium configurations.");
        auto t = args.requiredCdsCurves.find(termCrvName);
        QL_REQUIRE(t != args.requiredCdsCurves.end(), "CDSVolCurve: required cds curve '"
            << termCrvName << "' was not found during vol curve building.");
        termCurves.try_emplace(terms[i], Handle<CreditCurve>(t->second->creditCurve()));
        termMaturities.try_emplace(terms[i], maturities[i]);
    }

    // Get the CDS curve conventions to build CreditCurve::RefData for IndexCdsVolStripper::TradeData.
    ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    const string& convId = priceInfo.cdsConventionsId();
    auto p = conventions->get(convId, Convention::Type::CDS);
    QL_REQUIRE(p.first, "CDSVolCurve: no CDS conventions found with id " << convId << ".");
    ext::shared_ptr<CdsConvention> cdsConv = ext::dynamic_pointer_cast<CdsConvention>(p.second);
    QL_REQUIRE(cdsConv, "CDSVolCurve: convention '" << convId << "' could not be case to CdsConvention.");
    // Index term and start date will not be used in stripping volatilities so just use defaults here.
    CreditCurve::RefData refData = createRefData(0 * Days, Date(), cdsConv, priceInfo.runningCoupon());

    // Create the IndexCdsVolStripper::TradeData.
    auto strikeType = parseCdsOptionStrikeType(vc.strikeType());
    auto optionEngine = getOptionEngine(priceInfo.engineOverride());
    IndexCdsVolStripper::TradeData tradeData{ refData, strikeType, 1.0, 1.0, 0.0, optionEngine };

    // If index factor information in config, use it else try to get it via the reference data.
    IndexFactors indexFactors;
    if (priceInfo.indexFactors()) {
        indexFactors = *priceInfo.indexFactors();
    } else if (referenceData) {
        DLOG("CDSVolCurve: index factor information was not available directly in CDS volatility curve " <<
            "configuration for " << vc.curveID() << ". Will try to get this information from reference data.");
        indexFactors = getIndexFactors(vc.curveID(), referenceData);
    } else {
        WLOG("CDSVolCurve: index factor information was not available directly in CDS volatility curve " <<
            "configuration or reference data for " << vc.curveID() << ". Will proceed as if we have version 1 of " <<
            "the index i.e. default index factor of 1 and FEP of 0.");
    }

    // Update the trade data with the index factor information.
    tradeData.indexFactor = indexFactors.indexFactor;
    tradeData.indexFactorStrike = indexFactors.indexFactorStrike;
    tradeData.realisedFepFactor = indexFactors.realisedFep;

    // Create the IndexCdsVolStripper.
    IndexCdsVolStripper volStripper(args.asof, calendar_, Following, dayCounter_, termCurves, termMaturities,
        quotes, tradeData, getSolverOptions(priceInfo.solverConfig(), strikeType));

    // Set the volatility curve using the stripper.
    vol_ = volStripper.creditVolCurve();
    vol_->enableExtrapolation();

    if (!volStripper.errorMessages().empty()) {
        WLOG("CDSVolCurve: errors were encountered during stripping of volatilities for " << vc.curveID() << ":");
        for (const auto& msg : volStripper.errorMessages())
            WLOG("  - " << msg);
    }

    DLOG("CDSVolCurve: finished building 2-D volatility absolute strike surface via premium stripping.");
}

void CDSVolCurve::populateTermCurves(const CDSVolatilityCurveConfig& vc, const DefaultCurveCache& requiredCdsCurves,
    vector<Period>& terms, vector<Handle<CreditCurve>>& termCurves)
{
    for (Size i = 0; i < vc.termCurves().size(); ++i)
    {
        if (vc.termCurves()[i].empty())
            continue;
        auto t = requiredCdsCurves.find(vc.termCurves()[i]);
        QL_REQUIRE(t != requiredCdsCurves.end(), "CDSVolCurve: required cds curve '"
            << vc.termCurves()[i] << "' was not found during vol curve building.");
        termCurves.push_back(Handle<CreditCurve>(t->second->creditCurve()));
        terms.push_back(vc.terms()[i]);
    }
}

void CDSVolCurve::setVolatilityCurve(const Date& asof, CDSVolatilityCurveConfig& vc,
    const VolatilityStrikeSurfaceConfig& vssc, const CreditVolQuoteMap& quotes,
    const DefaultCurveCache& requiredCdsCurves, bool checkNumQuotes)
{
    LOG("CDSVolCurve: added " << quotes.size() << " quotes in building absolute strike surface.");

    QL_REQUIRE(!quotes.empty(), "No quotes loaded for " << vc.curveID());
    if (checkNumQuotes) {
        QL_REQUIRE(vc.quotes().size() == quotes.size(),
            "Found " << quotes.size() << " quotes, but " << vc.quotes().size() << " quotes required by config.");
    }

    vector<Period> terms;
    vector<Handle<CreditCurve>> termCurves;
    populateTermCurves(vc, requiredCdsCurves, terms, termCurves);

    const string& strikeInterpolation = vssc.strikeInterpolation();
    SviParametricVolatility::ModelVariant sviModelVariant;
    if (tryParse<SviParametricVolatility::ModelVariant>(strikeInterpolation, sviModelVariant,
        parseSviParametricVolatilityModelVariant))
    {
        QL_REQUIRE(vssc.timeInterpolation() == strikeInterpolation,
            "CDSVolCurve: SVI requires TimeInterpolation and StrikeInterpolation to be set to the same "
            "variant, got '" << vssc.timeInterpolation() << "' vs '" << strikeInterpolation << "'");
        buildSviVolatility(asof, vc, vssc, quotes, terms, termCurves, sviModelVariant);
    } else {
        vol_ = ext::make_shared<InterpolatingCreditVolCurve>(
            asof, calendar_, Following, dayCounter_, terms, termCurves, quotes, strikeType_);
        vol_->enableExtrapolation();
    }

    LOG("CDSVolCurve: finished building 2-D volatility absolute strike surface");
}

void CDSVolCurve::buildSviVolatility(
    const Date& asof, const CDSVolatilityCurveConfig& vc, const VolatilityStrikeSurfaceConfig& vssc,
    const map<tuple<Date, Period, Real>, Handle<Quote>>& quotes, const vector<Period>& effTerms,
    const vector<Handle<CreditCurve>>& termCurves,
    SviParametricVolatility::ModelVariant modelVariant) {

    LOG("CDSVolCurve: building SVI surface with model variant " << vssc.strikeInterpolation());

    // Organise quotes into per-slice vectors sorted by {exerciseDate, underlyingTerm}.
    // The quotes map is already sorted by key, so we just iterate in order.
    map<pair<Date, Period>, pair<vector<Real>, vector<Real>>> sliceData; // {date,term} -> {strikes, vols}
    for (auto const& kv : quotes) {
        auto key = make_pair(get<0>(kv.first), get<1>(kv.first));
        sliceData[key].first.push_back(get<2>(kv.first));
        sliceData[key].second.push_back(kv.second->value());
    }

    vector<Date> exerciseDates;
    vector<Period> underlyingTerms;
    vector<vector<Real>> sliceStrikes, sliceVols;

    for (auto const& kv : sliceData) {
        exerciseDates.push_back(kv.first.first);
        underlyingTerms.push_back(kv.first.second);
        sliceStrikes.push_back(kv.second.first);
        sliceVols.push_back(kv.second.second);
    }

    // Build modelParameters overrides from ParametricSmileConfiguration if provided.
    map<pair<Real, Real>, vector<pair<Real, QuantExt::ParametricVolatility::ParameterCalibration>>> modelParameters;
    Size maxCalibrationAttempts = 10;
    Real exitEarlyErrorThreshold = 0.005;
    Real maxAcceptableError = 0.05;

    if (vssc.parametricSmileConfiguration()) {
        auto const& psc = *vssc.parametricSmileConfiguration();
        auto const& params = psc.parameters();
        Size expectedSize = QuantExt::SviModelTraits::expectedParametersSize(modelVariant);
        QL_REQUIRE(params.size() == expectedSize,
            "CDSVolCurve: ParametricSmileConfiguration has " << params.size()
            << " parameters, but model variant " << vssc.strikeInterpolation() << " expects "
            << expectedSize);
        for (auto const& p : params) {
            QL_REQUIRE(p.initialValue.size() == 1 || p.initialValue.size() == exerciseDates.size(),
                "CDSVolCurve: ParametricSmileConfiguration parameter '"
                << p.name << "' has " << p.initialValue.size() << " initial values, expected 1 or "
                << exerciseDates.size());
        }
        for (Size j = 0; j < exerciseDates.size(); ++j) {
            Real t = dayCounter_.yearFraction(asof, exerciseDates[j]);
            Real uLen = periodToTime(underlyingTerms[j]);
            vector<pair<Real, QuantExt::ParametricVolatility::ParameterCalibration>> paramVec;
            for (auto const& p : params) {
                Real val = p.initialValue.size() == 1 ? p.initialValue.front() : p.initialValue[j];
                paramVec.push_back(make_pair(val, p.calibration));
            }
            modelParameters[make_pair(t, uLen)] = paramVec;
        }
        maxCalibrationAttempts = psc.calibration().maxCalibrationAttempts;
        exitEarlyErrorThreshold = psc.calibration().exitEarlyErrorThreshold;
        maxAcceptableError = psc.calibration().maxAcceptableError;
    }

    // Determine the input quote type from the configured VolatilityType, so that lognormal
    // market data (RATE_LNVOL) is not misinterpreted as normal vol when building an SVI surface.
    const auto inputQuoteType =
        (vssc.volType() == VolatilityConfig::VolatilityType::Normal)
        ? ParametricVolatility::MarketQuoteType::NormalVolatility
        : ParametricVolatility::MarketQuoteType::ShiftedLognormalVolatility;

    auto sviVol = QuantLib::ext::make_shared<QuantExt::CreditVolCurveSvi>(
        asof, calendar_, Following, dayCounter_, effTerms, termCurves, strikeType_, exerciseDates, underlyingTerms,
        sliceStrikes, sliceVols, modelVariant, inputQuoteType, Handle<YieldTermStructure>(), modelParameters,
        map<Real, Real>(), maxCalibrationAttempts, exitEarlyErrorThreshold, maxAcceptableError);

    // Force calibration and log RMSE diagnostics.
    // Note: volatility() triggers lazy calibration but may throw if the calibration produced no valid
    // parameters (e.g. global Mingone on a difficult surface). We catch that inner exception so we can
    // still log RMSE metrics without the failure preventing vol_ assignment.
    if (!exerciseDates.empty() && !sliceStrikes.empty() && !sliceStrikes[0].empty()) {
        Real uLen0 = periodToTime(underlyingTerms[0]);
        try {
            sviVol->volatility(exerciseDates[0], uLen0, sliceStrikes[0].front(), strikeType_);
        }
        catch (...) {}
        auto sviPV = QuantLib::ext::dynamic_pointer_cast<QuantExt::SviParametricVolatility>(
            sviVol->parametricVolatility());
        if (sviPV) {
            try {
                auto const& rmseVol = sviPV->volRmseShiftedLognormal();
                for (Size j = 0; j < rmseVol.columns(); ++j) {
                    for (Size i = 0; i < rmseVol.rows(); ++i) {
                        Real r = rmseVol(i, j);
                        if (r != Null<Real>())
                            DLOG("CDSVolCurve SVI (" << vssc.strikeInterpolation() << ") slice [" << j
                                << "] vol RMSE = " << r);
                    }
                }
                Real gv = sviPV->globalVolRmseShiftedLognormal();
                if (gv != Null<Real>())
                    DLOG("CDSVolCurve SVI (" << vssc.strikeInterpolation() << ") global vol RMSE = " << gv);
            }
            catch (...) {}
        }
    }

    vol_ = sviVol;
    vol_->enableExtrapolation();
    LOG("CDSVolCurve: finished building SVI surface with model variant " << vssc.strikeInterpolation());
}

Date CDSVolCurve::getExpiry(const Date& asof, const QuantLib::ext::shared_ptr<Expiry>& expiry) const {

    Date result;

    if (auto expiryDate = QuantLib::ext::dynamic_pointer_cast<ExpiryDate>(expiry)) {
        result = expiryDate->expiryDate();
    } else if (auto expiryPeriod = QuantLib::ext::dynamic_pointer_cast<ExpiryPeriod>(expiry)) {
        // We may need more conventions here eventually.
        result = calendar_.adjust(asof + expiryPeriod->expiryPeriod());
    } else if (auto fcExpiry = QuantLib::ext::dynamic_pointer_cast<FutureContinuationExpiry>(expiry)) {
        QL_FAIL("CDSVolCurve::getExpiry: future continuation expiry not supported for CDS volatility quotes.");
    } else {
        QL_FAIL("CDSVolCurve::getExpiry: cannot determine expiry type.");
    }

    return result;
}

} // namespace data
} // namespace ore
