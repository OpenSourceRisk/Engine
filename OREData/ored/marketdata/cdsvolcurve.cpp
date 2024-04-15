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

using namespace QuantExt;
using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

CDSVolCurve::CDSVolCurve(Date asof, CDSVolatilityCurveSpec spec, const Loader& loader,
                         const CurveConfigurations& curveConfigs,
                         const std::map<std::string, QuantLib::ext::shared_ptr<CDSVolCurve>>& requiredCdsVolCurves,
                         const std::map<std::string, QuantLib::ext::shared_ptr<DefaultCurve>>& requiredCdsCurves) {

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
            buildVolatility(asof, config, *vssc, loader, requiredCdsCurves);
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

    std::map<std::tuple<QuantLib::Date, QuantLib::Period, QuantLib::Real>, QuantLib::Handle<QuantLib::Quote>> quotes;
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
    std::map<std::tuple<QuantLib::Date, QuantLib::Period, QuantLib::Real>, QuantLib::Handle<QuantLib::Quote>> quotes;

    // Different approaches depending on whether we are using a regex or searching for a list of explicit quotes.
    if (wildcard) {

        DLOG("Have single quote with pattern " << wildcard->pattern());

        // Loop over quotes and process CDS option quotes matching pattern on asof
        for (const auto& md : loader.get(*wildcard, asof)) {

            QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

            auto q = QuantLib::ext::dynamic_pointer_cast<IndexCDSOptionQuote>(md);
            QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to IndexCDSOptionQuote");

            TLOG("The quote " << q->name() << " matched the pattern");

            /* - We load quotes with empty term only if there is at most one term specified in the curve config.
               - We load quotes with a term, if they match a term specified in the curve config or if no term is
                 specified in the curve config.
               - Quotes with an empty term get the unique term of the curve config assigned, if the curve config
                 has no terms specified, 5 * Years. */
            QuantLib::Period quoteTerm;
            if (q->indexTerm().empty()) {
                if (vc.terms().size() > 1)
                    continue;
                quoteTerm = vc.terms().empty() ? 5 * Years : vc.terms().front();
            } else {
                quoteTerm = parsePeriod(q->indexTerm());
                if (std::find(vc.terms().begin(), vc.terms().end(), quoteTerm) == vc.terms().end() &&
                    !vc.terms().empty())
                    continue;
            }

            Date expiryDate = getExpiry(asof, q->expiry());
            if (expiryDate > asof) {
                quotes[std::make_tuple(expiryDate, quoteTerm,
                                       strikeType_ == CreditVolCurve::Type::Price ? 1.0 : 0.0)] = q->quote();
                TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," << fixed
                                    << setprecision(9) << q->quote()->value() << ")");
            }
        }

        // Check that we have quotes in the end
        QL_REQUIRE(quotes.size() > 0, "No quotes found matching regular expression " << vcc.quotes()[0]);

    } else {

        DLOG("Have " << vcc.quotes().size() << " explicit quotes");

        // Loop over quotes and process CDS option quotes that are explicitly specified in the config
        std::ostringstream ss;
        ss << MarketDatum::InstrumentType::INDEX_CDS_OPTION << "/*";
        Wildcard w(ss.str());
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

void CDSVolCurve::buildVolatility(const Date& asof, CDSVolatilityCurveConfig& vc,
                                  const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
                                  const std::map<std::string, QuantLib::ext::shared_ptr<DefaultCurve>>& requiredCdsCurves) {

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
    vector<QuantLib::ext::shared_ptr<Expiry>> configuredExpiries;
    if (!expWc) {
        // Parse the list of expiry strings.
        for (const string& strExpiry : vssc.expiries()) {
            configuredExpiries.push_back(parseExpiry(strExpiry));
        }
        DLOG("Parsed " << configuredExpiries.size() << " unique configured expiries");
    }

    // If there are no wildcard strikes or wildcard expiries, delegate to buildVolatilityExplicit.
    if (!expWc && !strkWc) {
        buildVolatilityExplicit(asof, vc, vssc, loader, configuredStrikes, requiredCdsCurves);
        return;
    }

    DLOG("Expiries and or strikes have been configured via wildcards so building a "
         << "wildcard based absolute strike surface");

    // Store quotes by expiry, term, strike in a map
    std::map<std::tuple<QuantLib::Date, QuantLib::Period, QuantLib::Real>, QuantLib::Handle<QuantLib::Quote>> quotes;
    Size quotesAdded = 0;

    // Loop over quotes and process any CDS option quote that matches a wildcard
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::INDEX_CDS_OPTION << "/RATE_LNVOL/*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {

        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

        // Go to next quote if not a CDS option quote.
        auto q = QuantLib::ext::dynamic_pointer_cast<IndexCDSOptionQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to IndexCDSOptionQuote");

        // Go to next quote if index name in the quote does not match the cds vol configuration name.
        if (vc.curveID() != q->indexName() && vc.quoteName() != q->indexName())
            continue;

        // This surface is for absolute strikes only.
        auto strike = QuantLib::ext::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
        if (!strike)
            continue;

        QuantLib::Period quoteTerm;
        if (q->indexTerm().empty()) {
            if (vc.terms().size() > 1)
                continue;
            quoteTerm = vc.terms().empty() ? 5 * Years : vc.terms().front();
        } else {
            quoteTerm = parsePeriod(q->indexTerm());
            if (std::find(vc.terms().begin(), vc.terms().end(), quoteTerm) == vc.terms().end() && !vc.terms().empty())
                continue;
        }

        // If we have been given a list of explicit expiries, check that the quote matches one of them.
        // Move to the next quote if it does not.
        if (!expWc) {
            auto expiryIt = find_if(configuredExpiries.begin(), configuredExpiries.end(),
                                    [&q](QuantLib::ext::shared_ptr<Expiry> e) { return *e == *q->expiry(); });
            if (expiryIt == configuredExpiries.end())
                continue;
        }

        // If we have been given a list of explicit strikes, check that the quote matches one of them.
        // Move to the next quote if it does not.
        if (!strkWc) {
            auto strikeIt = find_if(configuredStrikes.begin(), configuredStrikes.end(),
                                    [&strike](Real s) { return close(s, strike->strike()); });
            if (strikeIt == configuredStrikes.end())
                continue;
        }

        // If we make it here, add the data to the map
        quotes[std::make_tuple(getExpiry(asof, q->expiry()), quoteTerm, strike->strike() / vc.strikeFactor())] =
            q->quote();
        quotesAdded++;

        TLOG("Added quote " << q->name() << ": (" << q->expiry() << "," << fixed << setprecision(9) << strike->strike()
                            << "," << q->quote()->value() << ")");
    }

    LOG("CDSVolCurve: added " << quotesAdded << " quotes in building wildcard based absolute strike surface.");
    QL_REQUIRE(quotesAdded > 0, "No quotes loaded for " << vc.curveID());

    DLOG("Creating the CreditVolCurve object");

    std::vector<QuantLib::Period> effTerms;
    std::vector<QuantLib::Handle<CreditCurve>> termCurves;
    for (Size i = 0; i < vc.termCurves().size(); ++i) {
        if (vc.termCurves()[i].empty())
            continue;
        auto t = requiredCdsCurves.find(vc.termCurves()[i]);
        QL_REQUIRE(t != requiredCdsCurves.end(), "CDSVolCurve: required cds curve '"
                                                     << vc.termCurves()[i]
                                                     << "' was not found during vol curve building.");
        termCurves.push_back(Handle<QuantExt::CreditCurve>(t->second->creditCurve()));
        effTerms.push_back(vc.terms()[i]);
    }

    vol_ = QuantLib::ext::make_shared<QuantExt::InterpolatingCreditVolCurve>(asof, calendar_, Following, dayCounter_, effTerms,
                                                                     termCurves, quotes, strikeType_);
    vol_->enableExtrapolation();

    LOG("CDSVolCurve: finished building 2-D volatility absolute strike surface");
}

void CDSVolCurve::buildVolatility(const Date& asof, const CDSVolatilityCurveSpec& spec,
                                  const CDSVolatilityCurveConfig& vc, const CDSProxyVolatilityConfig& pvc,
                                  const std::map<std::string, QuantLib::ext::shared_ptr<CDSVolCurve>>& requiredCdsVolCurves,
                                  const std::map<std::string, QuantLib::ext::shared_ptr<DefaultCurve>>& requiredCdsCurves) {
    LOG("CDSVolCurve: start building proxy volatility surface");
    auto proxyVolCurve = requiredCdsVolCurves.find(CDSVolatilityCurveSpec(pvc.cdsVolatilityCurve()).name());
    QL_REQUIRE(proxyVolCurve != requiredCdsVolCurves.end(), "CDSVolCurve: Failed to find cds vol curve '"
                                                                << pvc.cdsVolatilityCurve() << "' when building '"
                                                                << spec.name() << "'");
    std::vector<QuantLib::Period> effTerms;
    std::vector<QuantLib::Handle<CreditCurve>> termCurves;
    for (Size i = 0; i < vc.termCurves().size(); ++i) {
        if (vc.termCurves()[i].empty())
            continue;
        auto t = requiredCdsCurves.find(vc.termCurves()[i]);
        QL_REQUIRE(t != requiredCdsCurves.end(), "CDSVolCurve: required cds curve '"
                                                     << vc.termCurves()[i]
                                                     << "' was not found during vol curve building.");
        termCurves.push_back(Handle<QuantExt::CreditCurve>(t->second->creditCurve()));
        effTerms.push_back(vc.terms()[i]);
    }
    LOG("Will use " << termCurves.size()
                    << " term curves in target surface to determine atm levels and moneyness-adjustments");
    vol_ = QuantLib::ext::make_shared<QuantExt::ProxyCreditVolCurve>(
        Handle<CreditVolCurve>(proxyVolCurve->second->volTermStructure()), effTerms, termCurves);
    LOG("CDSVolCurve: finished building proxy volatility surface");
}

void CDSVolCurve::buildVolatilityExplicit(
    const Date& asof, CDSVolatilityCurveConfig& vc, const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
    const vector<Real>& configuredStrikes,
    const std::map<std::string, QuantLib::ext::shared_ptr<DefaultCurve>>& requiredCdsCurves) {

    LOG("CDSVolCurve: start building 2-D volatility absolute strike surface with explicit strikes and expiries");

    // Store quotes by expiry, term, strike in a map
    std::map<std::tuple<QuantLib::Date, QuantLib::Period, QuantLib::Real>, QuantLib::Handle<QuantLib::Quote>> quotes;

    // Count the number of quotes added. We check at the end that we have added all configured quotes.
    Size quotesAdded = 0;

    // Loop over quotes and process CDS option quotes that have been requested
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::INDEX_CDS_OPTION << "/*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {

        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

        // Go to next quote if not a CDS option quote.
        auto q = QuantLib::ext::dynamic_pointer_cast<IndexCDSOptionQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to IndexCDSOptionQuote");

        // This surface is for absolute strikes only.
        auto strike = QuantLib::ext::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
        if (!strike)
            continue;

        QuantLib::Period quoteTerm;
        if (q->indexTerm().empty()) {
            if (vc.terms().size() > 1)
                continue;
            quoteTerm = vc.terms().empty() ? 5 * Years : vc.terms().front();
        } else {
            quoteTerm = parsePeriod(q->indexTerm());
            if (std::find(vc.terms().begin(), vc.terms().end(), quoteTerm) == vc.terms().end() && !vc.terms().empty())
                continue;
        }

        // Add quote to surface
        quotes[std::make_tuple(getExpiry(asof, q->expiry()), quoteTerm, strike->strike() / vc.strikeFactor())] =
            q->quote();
        quotesAdded++;

        TLOG("Added quote " << q->name() << ": (" << q->expiry() << "," << fixed << setprecision(9) << strike->strike()
                            << "," << q->quote()->value() << ")")
    }

    LOG("CDSVolCurve: added " << quotesAdded << " quotes in building explicit absolute strike surface.");

    QL_REQUIRE(vc.quotes().size() == quotesAdded,
               "Found " << quotesAdded << " quotes, but " << vc.quotes().size() << " quotes required by config.");

    DLOG("Creating the CreditVolCurve object");

    std::vector<QuantLib::Period> effTerms;
    std::vector<QuantLib::Handle<CreditCurve>> termCurves;
    for (Size i = 0; i < vc.termCurves().size(); ++i) {
        if (vc.termCurves()[i].empty())
            continue;
        auto t = requiredCdsCurves.find(vc.termCurves()[i]);
        QL_REQUIRE(t != requiredCdsCurves.end(), "CDSVolCurve: required cds curve '"
                                                     << vc.termCurves()[i]
                                                     << "' was not found during vol curve building.");
        termCurves.push_back(Handle<QuantExt::CreditCurve>(t->second->creditCurve()));
        effTerms.push_back(vc.terms()[i]);
    }

    vol_ = QuantLib::ext::make_shared<QuantExt::InterpolatingCreditVolCurve>(asof, calendar_, Following, dayCounter_, effTerms,
                                                                     termCurves, quotes, strikeType_);
    vol_->enableExtrapolation();

    LOG("CDSVolCurve: finished building 2-D volatility absolute strike "
        << "surface with explicit strikes and expiries");
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
