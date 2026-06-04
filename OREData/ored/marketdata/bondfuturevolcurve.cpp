/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

#include <ored/marketdata/bondfuturevolcurve.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>

using namespace QuantLib;
using namespace QuantExt;
using std::pair;
using std::vector;

namespace {

using ore::data::BondFutureOptionQuote;
using ore::data::Expiry;
using ore::data::Extrapolation;
using ore::data::OneDimSolverConfig;
using ore::data::parseDate;
using ore::data::parseExtrapolation;
using ore::data::VolatilityStrikeSurfaceConfig;

// Logic to check that quote matches configured expiries and strikes if provided.
bool quoteMatchesStrikeExpiry(const ext::shared_ptr<BondFutureOptionQuote>& q, Real quoteStrike,
    const vector<Date>& configuredExpiries, const vector<Real>& configuredStrikes)
{
    // Check that the quote matches one of the expiries if given.
    bool result = true;
    if (!configuredExpiries.empty()) {
        Date expiry;
        // If quote expiry cannot be parsed as a date, return false immediately.
        if (!ore::data::tryParse<Date>(q->expiry(), expiry, parseDate)) {
            return false;
        }
        auto expiryIt = find_if(configuredExpiries.begin(), configuredExpiries.end(),
            [&expiry](const QuantLib::Date& e) { return e == expiry; });
        if (expiryIt == configuredExpiries.end())
            result = false;
    }

    // If we have been given a list of explicit strikes, check that the quote matches one of them.
    if (!configuredStrikes.empty()) {
        auto strikeIt = find_if(configuredStrikes.begin(), configuredStrikes.end(),
            [quoteStrike](Real s) { return close(s, quoteStrike); });
        if (strikeIt == configuredStrikes.end())
            result = false;
    }

    return result;
}

// Should not really rely on this default i.e. solverConfig should be populated in the configuration.
Solver1DOptions getSolverOptions(const ext::optional<OneDimSolverConfig>& solverConfig)
{
    // If given an explicit configuration, use the solver options created from it.
    if (solverConfig)
        return *solverConfig;

    // Use a conservative best guess.
    Solver1DOptions opts;
    opts.maxEvaluations = 200;
    opts.accuracy = 1E-9;
    opts.initialGuess = 0.15;
    opts.minMax = { 0.001, 3.0 };
    opts.step = 0.01;
    opts.lowerBound = 0.001;
    opts.upperBound = 3.0;
    return opts;
}

// Return a pair of bool and BlackVolTimeExtrapolation::Type indicating whether to use flat extrapolation in strike and
// the type of time extrapolation respectively.
pair<bool, BlackVolTimeExtrapolation::Type> getStrikeTimeExtrap(const VolatilityStrikeSurfaceConfig& vssc) {

    pair<bool, BlackVolTimeExtrapolation::Type> res{true, BlackVolTimeExtrapolation::Type::FlatVolatility};

    if (vssc.extrapolation()) {
        auto strikeExtrapType = parseExtrapolation(vssc.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            TLOG("BondFutureVolCurve: strike extrapolation switched to using interpolator.");
            res.first = false;
        } else if (strikeExtrapType == Extrapolation::None) {
            TLOG("BondFutureVolCurve: strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            TLOG("BondFutureVolCurve: strike extrapolation has been set to flat.");
        } else {
            TLOG("BondFutureVolCurve: unexpected strike extrapolation " << strikeExtrapType << " so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vssc.timeExtrapolation());
        if (timeExtrapType == Extrapolation::UseInterpolator) {
            TLOG("BondFutureVolCurve: time extrapolation switched to using interpolator.");
            res.second = BlackVolTimeExtrapolation::Type::UseInterpolator;
        } else if (timeExtrapType == Extrapolation::None) {
            TLOG("BondFutureVolCurve: time extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (timeExtrapType == Extrapolation::Flat) {
            TLOG("BondFutureVolCurve: time extrapolation has been set to flat.");
        } else {
            TLOG("BondFutureVolCurve: unexpected time extrapolation " << timeExtrapType << " so default to flat.");
        }
    } else {
        TLOG("BondFutureVolCurve: extrapolation is turned off for the whole surface so the time and"
                << " strike extrapolation settings are ignored.");
    }

    return res;
}

} // namespace

namespace ore {
namespace data {

BondFutureVolCurve::BondFutureVolCurve(Date asof,
    BondFutureVolatilityCurveSpec spec,
    const Loader& loader,
    const CurveConfigurations& curveConfigs,
    const YieldCurveCache& yieldCurves) {

    try {
        LOG("BondFutureVolCurve: start building bond future volatility structure with ID " << spec.curveConfigID());

        auto config = *curveConfigs.bondFutureVolatilityConfig(spec.curveConfigID());
        const string& calendar = config.calendar();
        calendar_ = calendar.empty() ? NullCalendar() : parseCalendar(calendar);
        dayCounter_ = parseDayCounter(config.dayCounter());

        const auto& volConfigs = config.volatilityConfig();
        DLOG("BondFutureVolCurve: Attempting to build bond future vol curve from volatilityConfig, " <<
            volConfigs.size() << " volatility configs provided.");

        for (const auto& vc : volConfigs) {
            try {
                if (!vc->calendar().empty())
                    calendar_ = vc->calendar();

                // We currently only support one type of volatility config - premia or vol for expiry x strike.
                auto vssc = ext::dynamic_pointer_cast<VolatilityStrikeSurfaceConfig>(vc);
                QL_REQUIRE(vssc, "BondFutureVolCurve: only volatility configurations of type "
                    "VolatilityStrikeSurfaceConfig are currently supported");
                using MDQT = MarketDatum::QuoteType;
                QL_REQUIRE(vssc->quoteType() == MDQT::PRICE || vssc->quoteType() == MDQT::RATE_LNVOL,
                    "BondFutureVolCurve: only option premiums or lognormal volatilities are currently supported for "
                    "bond future volatility surfaces");
                QL_REQUIRE(vssc->expiries().size() > 0, "BondFutureVolCurve: no expiries configured");
                QL_REQUIRE(vssc->strikes().size() > 0, "BondFutureVolCurve: no strikes configured");
                QL_REQUIRE(vssc->exerciseType() == Exercise::European || vssc->exerciseType() == Exercise::American,
                    "BondFutureVolCurve: only European or American exercise type is supported");

                if (vssc->quoteType() == MDQT::PRICE) {
                    // Build volatility surface from premia.
                    buildVolatilityFromPremia(asof, config, *vssc, loader, yieldCurves);
                } else {
                    // Build volatility surface from lognormal volatilities.
                    buildVolatilityFromVolatilities(asof, config, *vssc, loader);
                }

                // We've successfully built a surface. Set extrapolation, save the config and exit the loop
                DLOG("BondFutureVolCurve: setting extrapolation to " << to_string(vssc->extrapolation()));
                vol_->enableExtrapolation(vssc->extrapolation());
                volatilityConfig_ = vc;
                break;

            } catch (std::exception& e) {
                DLOG("BondFutureVolCurve: bond future vol curve building failed: " << e.what());
            } catch (...) {
                DLOG("BondFutureVolCurve: bond future vol curve building failed: unknown error");
            }
        }

        QL_REQUIRE(vol_, "BondFutureVolCurve: failed to volatility structure from " <<
            volConfigs.size() << " volatility configs provided.");

        LOG("BondFutureVolCurve: finished building bond future volatility structure with ID " << spec.curveConfigID());

    } catch (std::exception& e) {
        QL_FAIL("BondFutureVolCurve: bond future volatility curve building failed with error: " << e.what() << ".");
    } catch (...) {
        QL_FAIL("BondFutureVolCurve: bond future volatility curve building failed with unknown error.");
    }
}

BondFutureVolCurve::ConfiguredStrikesExpiries
BondFutureVolCurve::generateStrikesExpiries(const VolatilityStrikeSurfaceConfig& vssc,
    const BondFutureVolatilityConfig& vc) const {

    ConfiguredStrikesExpiries result;

    const auto& expiries = vssc.expiries();
    result.expiryWildcard = false;
    if (find(expiries.begin(), expiries.end(), "*") != expiries.end()) {
        result.expiryWildcard = true;
        QL_REQUIRE(expiries.size() == 1, "BondFutureVolCurve: wild card expiry given, no more expiries allowed.");
        DLOG("Have expiry wildcard pattern " << expiries[0]);
    }

    const auto& strikes = vssc.strikes();
    result.strikeWildcard = false;
    if (find(strikes.begin(), strikes.end(), "*") != strikes.end()) {
        result.strikeWildcard = true;
        QL_REQUIRE(strikes.size() == 1, "BondFutureVolCurve: wild card strike given, no more strikes allowed.");
        DLOG("Have strike wildcard pattern " << strikes[0]);
    }

    // If we do not have a strike wild card, we expect a list of absolute strike values
    auto& cfgStrikes = result.strikes;
    if (!result.strikeWildcard) {
        // Parse the list of absolute strikes
        cfgStrikes = parseVectorOfValues<Real>(strikes, &parseReal);
        std::sort(cfgStrikes.begin(), cfgStrikes.end());
        QL_REQUIRE(std::adjacent_find(cfgStrikes.begin(), cfgStrikes.end(),
            [](Real x, Real y) { return close(x, y); }) == cfgStrikes.end(),
            "BondFutureVolCurve: the configured strikes contain duplicates.");
        DLOG("Parsed " << cfgStrikes.size() << " unique configured absolute strikes");
    }

    // If we do not have an expiry wild card, parse the configured expiries.
    auto& cfgExpiries = result.expiries;
    if (!result.expiryWildcard) {
        // Parse the list of expiry strings.
        for (const string& strExpiry : expiries) {
            Date expiry;
            if (tryParse<Date>(strExpiry, expiry, parseDate)) {
                cfgExpiries.push_back(expiry);
            } else {
                WLOG("BondFutureVolCurve: only expiry dates are currently supported but got " <<
                    strExpiry << " for " << vc.curveID() << ". Ignoring it.");
            }
        }
        DLOG("Parsed " << cfgExpiries.size() << " unique configured expiries");
    }

    return result;
}

void BondFutureVolCurve::populateVolatilityQuotes(const Date& asof, const BondFutureVolatilityConfig& vc,
    const Loader& loader,const ConfiguredStrikesExpiries& strikesExpiries, QuoteSurface& quotes,
    const string& quoteType) {

    DLOG("BondFutureVolCurve: start populating volatility quotes.");

    // Store quotes by term, expiry in OptionPrice structs.
    using OptionPrice = BondFutureVolStripper::OptionPrice;

    // Process the relevant bond future option premium quotes.
    string wildcardStr = "BOND_FUTURE_OPTION/" + quoteType + "/" + vc.contractName() + "/*";

    // Configuration may specify that we only want call or put quotes. Use wildcard to filter them.
    const auto& onlyPutCall = vc.useOnlyPutCall();
    if (!onlyPutCall.empty()) {
        QL_REQUIRE(onlyPutCall == "C" || onlyPutCall == "P", "BondFutureVolCurve: if specified, useOnlyPutCall " <<
            "must be 'C' for calls or 'P' for puts. Got '"<< onlyPutCall << "' for curve " << vc.curveID() << ".");
        wildcardStr += "/" + onlyPutCall;
    }

    for (const auto& md : loader.get(Wildcard(wildcardStr), asof))
    {
        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

        // Go to next quote if not a bond future option quote.
        auto q = ext::dynamic_pointer_cast<BondFutureOptionQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to BondFutureOptionQuote");

        // This surface is for absolute strikes only.
        auto strike = ext::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
        if (!strike)
            continue;

        // If expiries or strikes are given, check that the quote matches one of them.
        // Note: we really only support expiry dates currently but keep it generic.
        if (!quoteMatchesStrikeExpiry(q, strike->strike(), strikesExpiries.expiries, strikesExpiries.strikes))
            continue;

        // Get the expiry date.
        Date expiryDate;
        if (!tryParse<Date>(q->expiry(), expiryDate, parseDate)) {
            WLOG("BondFutureVolCurve: bond future vol quote " << q->name() << " has an expiry string, " <<
                q->expiry() << ", that is not a date. Ignoring it for curve " << vc.curveID() << ".");
            continue;
        }

        if (expiryDate == asof) {
            TLOG("BondFutureVolCurve: bond future vol quote " << q->name() << " has an expiry date " <<
                "equal to asof date. Ignoring it for curve " << vc.curveID() << ".");
            continue;
        }

        // Iterator to an existing OptionPrice holder or end if don't have one.
        Real strikeValue = strike->strike() / vc.strikeFactor();
        vector<OptionPrice>& prices = quotes[expiryDate];
        auto priceIt = find_if(prices.begin(), prices.end(), [&strikeValue](const OptionPrice& p) {
            return close(p.strike, strikeValue);
        });

        // Ignore duplicate quotes for the same expiry, strike and option type (call/put).
        if (priceIt != prices.end()) {
            if ((q->isCall() && !priceIt->callPrice.empty()) || (!q->isCall() && !priceIt->putPrice.empty())) {
                WLOG("BondFutureVolCurve: duplicate quote found for expiry " << expiryDate << ", strike "
                    << strike->strike() << " and option type " << (q->isCall() ? "call" : "put") <<
                    ". Ignoring this quote: " << q->name() << " for curve " << vc.curveID() << ".");
                continue;
            }
        }

        // Add the quote to the OptionPrice holder.
        OptionPrice& op = priceIt == prices.end() ? prices.emplace_back() : *priceIt;
        op.strike = strikeValue;
        if (q->isCall())
            op.callPrice = q->quote();
        else
            op.putPrice = q->quote();

        TLOG("Added quote " << q->name() << ": (" << q->expiry() << "," << std::fixed << std::setprecision(9) <<
            strike->strike() << "," << q->quote()->value() << ")");
    }

    QL_REQUIRE(!quotes.empty(), "BondFutureVolCurve: found no premium quotes for curve " << vc.curveID() << ".");

    DLOG("BondFutureVolCurve: finished populating volatility premia quotes.");
}

void BondFutureVolCurve::buildVolatilityFromPremia(const Date& asof, BondFutureVolatilityConfig& vc,
    const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader, const YieldCurveCache& yieldCurves) {

    DLOG("BondFutureVolCurve: start building expiry x strike volatility surface from premia.");

    // We need a bond future price quote to create the stripper.
    auto futurePriceQuote = getFuturePriceQuote(asof, vc, loader);

    // We also need the a yield curve.
    const string& ytsId = vc.yieldCurveId();
    QL_REQUIRE(!ytsId.empty(), "BondFutureVolCurve: curve " << vc.curveID() << " needs a non-empty yield curve ID.");
    auto itYts = yieldCurves.find(ytsId);
    QL_REQUIRE(itYts != yieldCurves.end(), "BondFutureVolCurve: curve " << vc.curveID() << " needs yield curve " <<
        ytsId << " but it is not available.");
    auto yts = itYts->second->handle();

    // Get the configured strikes and expiries and whether or not we have wildcards.
    auto cfgStrikesExpiries = generateStrikesExpiries(vssc, vc);

    // Populate the quotes.
    QuoteSurface quotes;
    populateVolatilityQuotes(asof, vc, loader, cfgStrikesExpiries, quotes, "PRICE");

    // Other attributes needed to create the stripper.
    auto [flatStrikeExtrap, timeExtrapType] = getStrikeTimeExtrap(vssc);
    bool preferOutOfTheMoney = vc.preferOutOfTheMoney() ? *vc.preferOutOfTheMoney() : true;

    // Determine the exercise type.
    auto exerciseType = vssc.exerciseType();
    if (exerciseType == Exercise::American && vc.treatAsEuropean().value_or(false)) {
        exerciseType = Exercise::European;
    }

    // Create the bond future volatility stripper.
    BondFutureVolStripper volStripper(asof, calendar_, Following, dayCounter_, futurePriceQuote, yts, quotes,
        getSolverOptions(vc.solverConfig()), exerciseType, flatStrikeExtrap, flatStrikeExtrap, timeExtrapType,
        preferOutOfTheMoney);

    // Set the volatility curve using the stripper.
    vol_ = volStripper.volSurface();

    // Log any errors encountered during stripping.
    if (!volStripper.errorMessages().empty()) {
        WLOG("BondFutureVolCurve: errors encountered during stripping of volatilities for " << vc.curveID() << ":");
        for (const auto& msg : volStripper.errorMessages())
            WLOG("  - " << msg);
    }

    DLOG("BondFutureVolCurve: finished building expiry x strike volatility surface from premia.");
}

void BondFutureVolCurve::buildVolatilityFromVolatilities(const Date& asof, BondFutureVolatilityConfig& vc,
    const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader) {

    DLOG("BondFutureVolCurve: start building expiry x strike volatility surface from volatilities.");

    // Get the configured strikes and expiries and whether or not we have wildcards.
    auto cfgStrikesExpiries = generateStrikesExpiries(vssc, vc);

    // Populate the quotes. Note: using OptionPrice struct to hold volatility quotes rather than premia quotes here.
    QuoteSurface quotes;
    populateVolatilityQuotes(asof, vc, loader, cfgStrikesExpiries, quotes, "RATE_LNVOL");

    // We may need a bond future price to pick the volatilities below based on prefer out of the money setting.
    Real futurePrice = getFuturePriceQuote(asof, vc, loader)->value();

    // Small helper indicating whether to use call or put option.
    bool preferOutOfTheMoney = vc.preferOutOfTheMoney() ? *vc.preferOutOfTheMoney() : true;
    using OptionPrice = BondFutureVolStripper::OptionPrice;
    auto useCall = [preferOutOfTheMoney, futurePrice](const OptionPrice& op) {
        if (!op.callPrice.empty() && !op.putPrice.empty()) {
            if (preferOutOfTheMoney)
                return op.strike > futurePrice;
            else
                return op.strike < futurePrice;
        } else {
            return !op.callPrice.empty();
        }
    };

    // Need to populate the expiries, strikes, and vols to create the BlackVarianceSurfaceSparse below.
    vector<Date> expiries;
    vector<Real> strikes;
    vector<Volatility> vols;
    for (const auto& [expiryDate, volQuotes] : quotes) {
        for (const auto& volQuote : volQuotes) {
            expiries.push_back(expiryDate);
            strikes.push_back(volQuote.strike);
            vols.push_back(useCall(volQuote) ? volQuote.callPrice->value() : volQuote.putPrice->value());
        }
    }

    // Extrapolation attributes.
    auto [flatStrikeExtrap, timeExtrapType] = getStrikeTimeExtrap(vssc);

    // Populate the variance surface.
    vol_ = ext::make_shared<BlackVarianceSurfaceSparse<>>(asof, calendar_, expiries, strikes, vols,
        dayCounter_, flatStrikeExtrap, flatStrikeExtrap, timeExtrapType);

    DLOG("BondFutureVolCurve: finished building expiry x strike volatility surface from volatilities.");
}

Handle<Quote> BondFutureVolCurve::getFuturePriceQuote(const Date& asof, const BondFutureVolatilityConfig& vc,
    const Loader& loader) const {

    string futurePriceMdName = "BOND_FUTURE/PRICE/" + vc.contractName();
    QL_REQUIRE(loader.has(futurePriceMdName, asof), "BondFutureVolCurve: curve " << vc.curveID() <<
        " needs bond future price market datum " << futurePriceMdName << ".");
    auto futurePriceQuote = loader.get(futurePriceMdName, asof)->quote();
    return futurePriceQuote;
}

} // namespace data
} // namespace ore
