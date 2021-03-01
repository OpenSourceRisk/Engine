/*
Copyright (C) 2018 Quaternion Risk Management Ltd
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
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/range/adaptor/indexed.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <ored/marketdata/commodityvolcurve.hpp>
#include <ored/utilities/conventionsbasedfutureexpiry.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/math/interpolations/bicubicsplineinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/aposurface.hpp>
#include <qle/termstructures/blackvariancesurfacemoneyness.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>
#include <qle/termstructures/blackvolsurfacedelta.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>
#include <regex>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

CommodityVolCurve::CommodityVolCurve(const Date& asof, const CommodityVolatilityCurveSpec& spec, const Loader& loader,
                                     const CurveConfigurations& curveConfigs, const Conventions& conventions,
                                     const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                                     const map<string, boost::shared_ptr<CommodityCurve>>& commodityCurves,
                                     const map<string, boost::shared_ptr<CommodityVolCurve>>& commodityVolCurves) {

    try {
        LOG("CommodityVolCurve: start building commodity volatility structure with ID " << spec.curveConfigID());

        auto config = *curveConfigs.commodityVolatilityConfig(spec.curveConfigID());

        if (!config.futureConventionsId().empty()) {
            const auto& cId = config.futureConventionsId();
            QL_REQUIRE(conventions.has(cId), "Conventions, " << cId <<
                " for config " << config.curveID() << " not found.");
            convention_ = boost::dynamic_pointer_cast<CommodityFutureConvention>(conventions.get(cId));
            QL_REQUIRE(convention_, "Convention with ID '" << cId << "' should be of type CommodityFutureConvention");
            expCalc_ = boost::make_shared<ConventionsBasedFutureExpiry>(*convention_);
        }

        calendar_ = parseCalendar(config.calendar());
        dayCounter_ = parseDayCounter(config.dayCounter());

        // Do different things depending on the type of volatility configured
        boost::shared_ptr<VolatilityConfig> vc = config.volatilityConfig();
        if (auto cvc = boost::dynamic_pointer_cast<ConstantVolatilityConfig>(vc)) {
            buildVolatility(asof, config, *cvc, loader);
        } else if (auto vcc = boost::dynamic_pointer_cast<VolatilityCurveConfig>(vc)) {
            buildVolatility(asof, config, *vcc, loader);
        } else if (auto vssc = boost::dynamic_pointer_cast<VolatilityStrikeSurfaceConfig>(vc)) {
            buildVolatility(asof, config, *vssc, loader);
        } else if (auto vdsc = boost::dynamic_pointer_cast<VolatilityDeltaSurfaceConfig>(vc)) {
            // Need a yield curve and price curve to create a delta surface.
            populateCurves(config, yieldCurves, commodityCurves, true);
            buildVolatility(asof, config, *vdsc, loader);
        } else if (auto vmsc = boost::dynamic_pointer_cast<VolatilityMoneynessSurfaceConfig>(vc)) {
            // Need a yield curve (if forward moneyness) and price curve to create a moneyness surface.
            MoneynessStrike::Type moneynessType = parseMoneynessType(vmsc->moneynessType());
            bool fwdMoneyness = moneynessType == MoneynessStrike::Type::Forward;
            populateCurves(config, yieldCurves, commodityCurves, fwdMoneyness);
            buildVolatility(asof, config, *vmsc, loader);
        } else if (auto vapo = boost::dynamic_pointer_cast<VolatilityApoFutureSurfaceConfig>(vc)) {

            // Get the base conventions and create the associated expiry calculator.
            QL_REQUIRE(!vapo->baseConventionsId().empty(),
                       "The APO FutureConventions must be populated to build a future APO surface");
            QL_REQUIRE(conventions.has(vapo->baseConventionsId()), "Conventions, " << vapo->baseConventionsId()
                                                                                   << " for config " << config.curveID()
                                                                                   << " not found.");
            auto convention =
                boost::dynamic_pointer_cast<CommodityFutureConvention>(conventions.get(vapo->baseConventionsId()));
            QL_REQUIRE(convention, "Convention with ID '" << config.futureConventionsId()
                                                          << "' should be of type CommodityFutureConvention");
            auto baseExpCalc = boost::make_shared<ConventionsBasedFutureExpiry>(*convention);

            // Need to get the base commodity volatility structure.
            QL_REQUIRE(!vapo->baseVolatilityId().empty(),
                       "The APO VolatilityId must be populated to build a future APO surface.");
            auto itVs = commodityVolCurves.find(vapo->baseVolatilityId());
            QL_REQUIRE(itVs != commodityVolCurves.end(),
                       "Can't find commodity volatility with id " << vapo->baseVolatilityId());
            auto baseVs = Handle<BlackVolTermStructure>(itVs->second->volatility());

            // Need to get the base price curve
            QL_REQUIRE(!vapo->basePriceCurveId().empty(),
                       "The APO PriceCurveId must be populated to build a future APO surface.");
            auto itPts = commodityCurves.find(vapo->basePriceCurveId());
            QL_REQUIRE(itPts != commodityCurves.end(), "Can't find price curve with id " << vapo->basePriceCurveId());
            auto basePts = Handle<PriceTermStructure>(itPts->second->commodityPriceCurve());

            // Need a yield curve and price curve to create an APO surface.
            populateCurves(config, yieldCurves, commodityCurves, true);

            buildVolatility(asof, config, *vapo, baseVs, basePts, conventions);

        } else {
            QL_FAIL("Unexpected VolatilityConfig in CommodityVolatilityConfig");
        }

        LOG("CommodityVolCurve: finished building commodity volatility structure with ID " << spec.curveConfigID());

    } catch (exception& e) {
        QL_FAIL("Commodity volatility curve building failed : " << e.what());
    } catch (...) {
        QL_FAIL("Commodity volatility curve building failed: unknown error");
    }
}

void CommodityVolCurve::buildVolatility(const Date& asof, const CommodityVolatilityConfig& vc,
                                        const ConstantVolatilityConfig& cvc, const Loader& loader) {

    LOG("CommodityVolCurve: start building constant volatility structure");

    // Loop over all market datums and find the single quote
    // Return error if there are duplicates (this is why we do not use loader.get() method)
    Real quoteValue = Null<Real>();
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION) {

            boost::shared_ptr<CommodityOptionQuote> q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);

            if (q->name() == cvc.quote()) {
                TLOG("Found the constant volatility quote " << q->name());
                QL_REQUIRE(quoteValue == Null<Real>(), "Duplicate quote found for quote with id " << cvc.quote());
                quoteValue = q->quote()->value();
            }
        }
    }
    QL_REQUIRE(quoteValue != Null<Real>(), "Quote not found for id " << cvc.quote());

    DLOG("Creating BlackConstantVol structure");
    volatility_ = boost::make_shared<BlackConstantVol>(asof, calendar_, quoteValue, dayCounter_);

    LOG("CommodityVolCurve: finished building constant volatility structure");
}

void CommodityVolCurve::buildVolatility(const QuantLib::Date& asof, const CommodityVolatilityConfig& vc,
                                        const VolatilityCurveConfig& vcc, const Loader& loader) {

    LOG("CommodityVolCurve: start building 1-D volatility curve");

    // Must have at least one quote
    QL_REQUIRE(vcc.quotes().size() > 0, "No quotes specified in config " << vc.curveID());

    // Check if we are using a regular expression to select the quotes for the curve. If we are, the quotes should
    // contain exactly one element.
    bool isRegex = false;
    for (Size i = 0; i < vcc.quotes().size(); i++) {
        if ((isRegex = vcc.quotes()[i].find("*") != string::npos)) {
            QL_REQUIRE(i == 0 && vcc.quotes().size() == 1,
                       "Wild card config, " << vc.curveID() << ", should have exactly one quote.");
            break;
        }
    }

    // curveData will be populated with the expiry dates and volatility values.
    map<Date, Real> curveData;

    // Different approaches depending on whether we are using a regex or searching for a list of explicit quotes.
    if (isRegex) {

        DLOG("Have single quote with pattern " << vcc.quotes()[0]);

        // Create the regular expression
        regex regexp(boost::replace_all_copy(vcc.quotes()[0], "*", ".*"));

        // Loop over quotes and process commodity option quotes matching pattern on asof
        for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

            // Go to next quote if the market data point's date does not equal our asof
            if (md->asofDate() != asof)
                continue;

            auto q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);
            if (q && regex_match(q->name(), regexp)) {

                TLOG("The quote " << q->name() << " matched the pattern");

                Date expiryDate = getExpiry(asof, q->expiry(), vc.futureConventionsId(), vc.optionExpiryRollDays());
                if (expiryDate > asof) {
                    // Add the quote to the curve data
                    QL_REQUIRE(curveData.count(expiryDate) == 0, "Duplicate quote for the expiry date "
                                                                     << io::iso_date(expiryDate)
                                                                     << " provided by commodity volatility config "
                                                                     << vc.curveID());
                    curveData[expiryDate] = q->quote()->value();

                    TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," << fixed
                                        << setprecision(9) << q->quote()->value() << ")");
                }
            }
        }

        // Check that we have quotes in the end
        QL_REQUIRE(curveData.size() > 0, "No quotes found matching regular expression " << vcc.quotes()[0]);

    } else {

        DLOG("Have " << vcc.quotes().size() << " explicit quotes");

        // Loop over quotes and process commodity option quotes that are explicitly specified in the config
        for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

            // Go to next quote if the market data point's date does not equal our asof
            if (md->asofDate() != asof)
                continue;

            if (auto q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md)) {

                // Find quote name in configured quotes.
                auto it = find(vcc.quotes().begin(), vcc.quotes().end(), q->name());

                if (it != vcc.quotes().end()) {
                    TLOG("Found the configured quote " << q->name());

                    Date expiryDate = getExpiry(asof, q->expiry(), vc.futureConventionsId(), vc.optionExpiryRollDays());
                    QL_REQUIRE(expiryDate > asof, "Commodity volatility quote '" << q->name()
                                                                                 << "' has expiry in the past ("
                                                                                 << io::iso_date(expiryDate) << ")");
                    QL_REQUIRE(curveData.count(expiryDate) == 0, "Duplicate quote for the date "
                                                                     << io::iso_date(expiryDate)
                                                                     << " provided by commodity volatility config "
                                                                     << vc.curveID());
                    curveData[expiryDate] = q->quote()->value();

                    TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," << fixed
                                        << setprecision(9) << q->quote()->value() << ")");
                }
            }
        }

        // Check that we have found all of the explicitly configured quotes
        QL_REQUIRE(curveData.size() == vcc.quotes().size(), "Found " << curveData.size() << " quotes, but "
                                                                     << vcc.quotes().size()
                                                                     << " quotes were given in config.");
    }

    // Create the dates and volatility vector
    vector<Date> dates;
    vector<Volatility> volatilities;
    for (const auto& datum : curveData) {
        dates.push_back(datum.first);
        volatilities.push_back(datum.second);
        TLOG("Added data point (" << io::iso_date(dates.back()) << "," << fixed << setprecision(9)
                                  << volatilities.back() << ")");
    }

    DLOG("Creating BlackVarianceCurve object.");
    auto tmp = boost::make_shared<BlackVarianceCurve>(asof, dates, volatilities, dayCounter_);

    // Set the interpolation.
    if (vcc.interpolation() == "Linear") {
        DLOG("Interpolation set to Linear.");
    } else if (vcc.interpolation() == "Cubic") {
        DLOG("Setting interpolation to Cubic.");
        tmp->setInterpolation<Cubic>();
    } else if (vcc.interpolation() == "LogLinear") {
        DLOG("Setting interpolation to LogLinear.");
        tmp->setInterpolation<LogLinear>();
    } else {
        DLOG("Interpolation " << vcc.interpolation() << " not recognised so leaving it Linear.");
    }

    // Set the volatility_ member after we have possibly updated the interpolation.
    volatility_ = tmp;

    // Set the extrapolation
    if (parseExtrapolation(vcc.extrapolation()) == Extrapolation::Flat) {
        DLOG("Enabling BlackVarianceCurve flat volatility extrapolation.");
        volatility_->enableExtrapolation();
    } else if (parseExtrapolation(vcc.extrapolation()) == Extrapolation::None) {
        DLOG("Disabling BlackVarianceCurve extrapolation.");
        volatility_->disableExtrapolation();
    } else if (parseExtrapolation(vcc.extrapolation()) == Extrapolation::UseInterpolator) {
        DLOG("BlackVarianceCurve does not support using interpolator for extrapolation "
             << "so default to flat volatility extrapolation.");
        volatility_->enableExtrapolation();
    } else {
        DLOG("Unexpected extrapolation so default to flat volatility extrapolation.");
        volatility_->enableExtrapolation();
    }

    LOG("CommodityVolCurve: finished building 1-D volatility curve");
}

void CommodityVolCurve::buildVolatility(const Date& asof, CommodityVolatilityConfig& vc,
                                        const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader) {

    LOG("CommodityVolCurve: start building 2-D volatility absolute strike surface");

    // We are building a commodity volatility surface here of the form expiry vs strike where the strikes are absolute
    // numbers. The list of expiries may be explicit or contain a single wildcard character '*'. Similarly, the list
    // of strikes may be explicit or contain a single wildcard character '*'. So, we have four options here which
    // ultimately lead to two different types of surface being built:
    // 1. explicit strikes and explicit expiries => BlackVarianceSurface
    // 2. wildcard strikes and/or wildcard expiries (3 combinations) => BlackVarianceSurfaceSparse

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
    vector<boost::shared_ptr<Expiry>> configuredExpiries;
    if (!expWc) {
        // Parse the list of expiry strings.
        for (const string& strExpiry : vssc.expiries()) {
            configuredExpiries.push_back(parseExpiry(strExpiry));
        }
        DLOG("Parsed " << configuredExpiries.size() << " unique configured expiries");
    }

    // If there are no wildcard strikes or wildcard expiries, delegate to buildVolatilityExplicit.
    if (!expWc && !strkWc) {
        buildVolatilityExplicit(asof, vc, vssc, loader, configuredStrikes);
        return;
    }

    DLOG("Expiries and or strikes have been configured via wildcards so building a "
         << "wildcard based absolute strike surface");

    // Store aligned strikes, expiries and vols found via wildcard lookup
    vector<Real> strikes;
    vector<Date> expiries;
    vector<Volatility> vols;
    Size quotesAdded = 0;

    // Loop over quotes and process any commodity option quote that matches a wildcard
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

        // Go to next quote if the market data point's date does not equal our asof.
        if (md->asofDate() != asof)
            continue;

        // Go to next quote if not a commodity option quote.
        auto q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);
        if (!q)
            continue;

        // Go to next quote if not a commodity name or currency do not match config.
        if (vc.curveID() != q->commodityName() || vc.currency() != q->quoteCurrency())
            continue;

        // This surface is for absolute strikes only.
        auto strike = boost::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
        if (!strike)
            continue;

        // If we have been given a list of explicit expiries, check that the quote matches one of them.
        // Move to the next quote if it does not.
        if (!expWc) {
            auto expiryIt = find_if(configuredExpiries.begin(), configuredExpiries.end(),
                                    [&q](boost::shared_ptr<Expiry> e) { return *e == *q->expiry(); });
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

        // If we make it here, add the data to the aligned vectors.
        expiries.push_back(getExpiry(asof, q->expiry(), vc.futureConventionsId(), vc.optionExpiryRollDays()));
        strikes.push_back(strike->strike());
        vols.push_back(q->quote()->value());
        quotesAdded++;

        TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiries.back()) << "," << fixed << setprecision(9)
                            << strikes.back() << "," << vols.back() << ")");
    }

    LOG("CommodityVolCurve: added " << quotesAdded << " quotes in building wildcard based absolute strike surface.");
    QL_REQUIRE(quotesAdded > 0, "No quotes loaded for " << vc.curveID());

    // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
    bool flatStrikeExtrap = true;
    bool flatTimeExtrap = true;
    if (vssc.extrapolation()) {

        auto strikeExtrapType = parseExtrapolation(vssc.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Strike extrapolation switched to using interpolator.");
            flatStrikeExtrap = false;
        } else if (strikeExtrapType == Extrapolation::None) {
            DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            DLOG("Strike extrapolation has been set to flat.");
        } else {
            DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vssc.timeExtrapolation());
        if (timeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Time extrapolation switched to using interpolator.");
            flatTimeExtrap = false;
        } else if (timeExtrapType == Extrapolation::None) {
            DLOG("Time extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (timeExtrapType == Extrapolation::Flat) {
            DLOG("Time extrapolation has been set to flat.");
        } else {
            DLOG("Time extrapolation " << timeExtrapType << " not expected so default to flat.");
        }

    } else {
        DLOG("Extrapolation is turned off for the whole surface so the time and"
             << " strike extrapolation settings are ignored");
    }

    DLOG("Creating the BlackVarianceSurfaceSparse object");
    volatility_ = boost::make_shared<BlackVarianceSurfaceSparse>(asof, calendar_, expiries, strikes, vols, dayCounter_,
                                                                 flatStrikeExtrap, flatStrikeExtrap, flatTimeExtrap);

    DLOG("Setting BlackVarianceSurfaceSparse extrapolation to " << to_string(vssc.extrapolation()));
    volatility_->enableExtrapolation(vssc.extrapolation());

    LOG("CommodityVolCurve: finished building 2-D volatility absolute strike surface");
}

void CommodityVolCurve::buildVolatilityExplicit(const Date& asof, CommodityVolatilityConfig& vc,
                                                const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
                                                const vector<Real>& configuredStrikes) {

    LOG("CommodityVolCurve: start building 2-D volatility absolute strike surface with explicit strikes and expiries");

    // Map to hold the rows of the commodity volatility matrix. The keys are the expiry dates and the values are the
    // vectors of volatilities, one for each configured strike.
    map<Date, vector<Real>> surfaceData;

    // Count the number of quotes added. We check at the end that we have added all configured quotes.
    Size quotesAdded = 0;

    // Loop over quotes and process commodity option quotes that have been requested
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

        // Go to next quote if the market data point's date does not equal our asof.
        if (md->asofDate() != asof)
            continue;

        // Go to next quote if not a commodity option quote.
        auto q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);
        if (!q)
            continue;

        // This surface is for absolute strikes only.
        auto strike = boost::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
        if (!strike)
            continue;

        // If the quote is not in the configured quotes continue
        auto it = find(vc.quotes().begin(), vc.quotes().end(), q->name());
        if (it == vc.quotes().end())
            continue;

        // Process the quote
        Date eDate = getExpiry(asof, q->expiry(), vc.futureConventionsId(), vc.optionExpiryRollDays());

        // Position of quote in vector of strikes
        auto strikeIt = find_if(configuredStrikes.begin(), configuredStrikes.end(),
                                [&strike](Real s) { return close(s, strike->strike()); });
        Size pos = distance(configuredStrikes.begin(), strikeIt);
        QL_REQUIRE(
            pos < configuredStrikes.size(),
            "The quote '" << q->name()
                          << "' is in the list of configured quotes but does not match any of the configured strikes");

        // Add quote to surface
        if (surfaceData.count(eDate) == 0)
            surfaceData[eDate] = vector<Real>(configuredStrikes.size(), Null<Real>());

        QL_REQUIRE(surfaceData[eDate][pos] == Null<Real>(),
                   "Quote " << q->name() << " provides a duplicate quote for the date " << io::iso_date(eDate)
                            << " and the strike " << configuredStrikes[pos]);
        surfaceData[eDate][pos] = q->quote()->value();
        quotesAdded++;

        TLOG("Added quote " << q->name() << ": (" << io::iso_date(eDate) << "," << fixed << setprecision(9)
                            << configuredStrikes[pos] << "," << q->quote()->value() << ")");
    }

    LOG("CommodityVolCurve: added " << quotesAdded << " quotes in building explicit absolute strike surface.");

    QL_REQUIRE(vc.quotes().size() == quotesAdded,
               "Found " << quotesAdded << " quotes, but " << vc.quotes().size() << " quotes required by config.");

    // Set up the BlackVarianceSurface. Note that the rows correspond to strikes and that the columns correspond to
    // the expiry dates in the matrix that is fed to the BlackVarianceSurface ctor.
    vector<Date> expiryDates;
    Matrix volatilities = Matrix(configuredStrikes.size(), surfaceData.size());
    Size expiryIdx = 0;
    for (const auto& expiryRow : surfaceData) {
        expiryDates.push_back(expiryRow.first);
        for (Size i = 0; i < configuredStrikes.size(); i++) {
            volatilities[i][expiryIdx] = expiryRow.second[i];
        }
        expiryIdx++;
    }

    // Trace log the surface
    TLOG("Explicit strike surface grid points:");
    TLOG("expiry,strike,volatility");
    for (Size i = 0; i < volatilities.rows(); i++) {
        for (Size j = 0; j < volatilities.columns(); j++) {
            TLOG(io::iso_date(expiryDates[j])
                 << "," << fixed << setprecision(9) << configuredStrikes[i] << "," << volatilities[i][j]);
        }
    }

    // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
    // BlackVarianceSurface time extrapolation is hard-coded to constant in volatility.
    BlackVarianceSurface::Extrapolation strikeExtrap = BlackVarianceSurface::ConstantExtrapolation;
    if (vssc.extrapolation()) {

        auto strikeExtrapType = parseExtrapolation(vssc.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Strike extrapolation switched to using interpolator.");
            strikeExtrap = BlackVarianceSurface::InterpolatorDefaultExtrapolation;
        } else if (strikeExtrapType == Extrapolation::None) {
            DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            DLOG("Strike extrapolation has been set to flat.");
        } else {
            DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vssc.timeExtrapolation());
        if (timeExtrapType != Extrapolation::Flat) {
            DLOG("BlackVarianceSurface only supports flat volatility extrapolation in the time direction");
        }
    } else {
        DLOG("Extrapolation is turned off for the whole surface so the time and"
             << " strike extrapolation settings are ignored");
    }

    DLOG("Creating BlackVarianceSurface object");
    auto tmp = boost::make_shared<BlackVarianceSurface>(asof, calendar_, expiryDates, configuredStrikes, volatilities,
                                                        dayCounter_, strikeExtrap, strikeExtrap);

    // Set the interpolation if configured properly. The default is Bilinear.
    if (!(vssc.timeInterpolation() == "Linear" && vssc.strikeInterpolation() == "Linear")) {
        if (vssc.timeInterpolation() != vssc.strikeInterpolation()) {
            DLOG("Time and strike interpolation must be the same for BlackVarianceSurface but we got strike "
                 << "interpolation " << vssc.strikeInterpolation() << " and time interpolation "
                 << vssc.timeInterpolation());
        } else if (vssc.timeInterpolation() == "Cubic") {
            DLOG("Setting interpolation to BiCubic");
            tmp->setInterpolation<Bicubic>();
        } else {
            DLOG("Interpolation type " << vssc.timeInterpolation() << " not supported in 2 dimensions");
        }
    }

    // Set the volatility_ member after we have possibly updated the interpolation.
    volatility_ = tmp;

    DLOG("Setting BlackVarianceSurface extrapolation to " << to_string(vssc.extrapolation()));
    volatility_->enableExtrapolation(vssc.extrapolation());

    LOG("CommodityVolCurve: finished building 2-D volatility absolute strike "
        << "surface with explicit strikes and expiries");
}

void CommodityVolCurve::buildVolatility(const Date& asof, CommodityVolatilityConfig& vc,
                                        const VolatilityDeltaSurfaceConfig& vdsc, const Loader& loader) {

    using boost::adaptors::transformed;
    using boost::algorithm::join;

    LOG("CommodityVolCurve: start building 2-D volatility delta strike surface");

    // Parse, sort and check the vector of configured put deltas
    vector<Real> putDeltas = parseVectorOfValues<Real>(vdsc.putDeltas(), &parseReal);
    sort(putDeltas.begin(), putDeltas.end(), [](Real x, Real y) { return !close(x, y) && x < y; });
    QL_REQUIRE(adjacent_find(putDeltas.begin(), putDeltas.end(), [](Real x, Real y) { return close(x, y); }) ==
                   putDeltas.end(),
               "The configured put deltas contain duplicates");
    DLOG("Parsed " << putDeltas.size() << " unique configured put deltas");
    DLOG("Put deltas are: " << join(putDeltas | transformed([](Real d) { return ore::data::to_string(d); }), ","));

    // Parse, sort descending and check the vector of configured call deltas
    vector<Real> callDeltas = parseVectorOfValues<Real>(vdsc.callDeltas(), &parseReal);
    sort(callDeltas.begin(), callDeltas.end(), [](Real x, Real y) { return !close(x, y) && x > y; });
    QL_REQUIRE(adjacent_find(callDeltas.begin(), callDeltas.end(), [](Real x, Real y) { return close(x, y); }) ==
                   callDeltas.end(),
               "The configured call deltas contain duplicates");
    DLOG("Parsed " << callDeltas.size() << " unique configured call deltas");
    DLOG("Call deltas are: " << join(callDeltas | transformed([](Real d) { return ore::data::to_string(d); }), ","));

    // Expiries may be configured with a wildcard or given explicitly
    bool expWc = false;
    if (find(vdsc.expiries().begin(), vdsc.expiries().end(), "*") != vdsc.expiries().end()) {
        expWc = true;
        QL_REQUIRE(vdsc.expiries().size() == 1, "Wild card expiry specified but more expiries also specified.");
        DLOG("Have expiry wildcard pattern " << vdsc.expiries()[0]);
    }

    // Map to hold the rows of the commodity volatility matrix. The keys are the expiry dates and the values are the
    // vectors of volatilities, one for each configured delta.
    map<Date, vector<Real>> surfaceData;

    // Number of strikes = number of put deltas + ATM + number of call deltas
    Size numStrikes = putDeltas.size() + 1 + callDeltas.size();

    // Count the number of quotes added. We check at the end that we have added all configured quotes.
    Size quotesAdded = 0;

    // Configured delta and Atm types.
    DeltaVolQuote::DeltaType deltaType = parseDeltaType(vdsc.deltaType());
    DeltaVolQuote::AtmType atmType = parseAtmType(vdsc.atmType());
    boost::optional<DeltaVolQuote::DeltaType> atmDeltaType;
    if (!vdsc.atmDeltaType().empty()) {
        atmDeltaType = parseDeltaType(vdsc.atmDeltaType());
    }

    // Populate the configured strikes.
    vector<boost::shared_ptr<BaseStrike>> strikes;
    for (const auto& pd : putDeltas) {
        strikes.push_back(boost::make_shared<DeltaStrike>(deltaType, Option::Put, pd));
    }
    strikes.push_back(boost::make_shared<AtmStrike>(atmType, atmDeltaType));
    for (const auto& cd : callDeltas) {
        strikes.push_back(boost::make_shared<DeltaStrike>(deltaType, Option::Call, cd));
    }

    // Read the quotes to fill the expiry dates and vols matrix.
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

        // Go to next quote if the market data point's date does not equal our asof.
        if (md->asofDate() != asof)
            continue;

        // Go to next quote if not a commodity option quote.
        auto q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);
        if (!q)
            continue;

        // Go to next quote if not a commodity name or currency do not match config.
        if (vc.curveID() != q->commodityName() || vc.currency() != q->quoteCurrency())
            continue;

        // Iterator to one of the configured strikes.
        vector<boost::shared_ptr<BaseStrike>>::iterator strikeIt;

        if (!expWc) {

            // If we have explicitly configured expiries and the quote is not in the configured quotes continue.
            auto it = find(vc.quotes().begin(), vc.quotes().end(), q->name());
            if (it == vc.quotes().end())
                continue;

            // Check if quote's strike is in the configured strikes.
            // It should be as we have selected from the explicitly configured quotes in the last step.
            strikeIt = find_if(strikes.begin(), strikes.end(),
                               [&q](boost::shared_ptr<BaseStrike> s) { return *s == *q->strike(); });
            QL_REQUIRE(strikeIt != strikes.end(),
                       "The quote '"
                           << q->name()
                           << "' is in the list of configured quotes but does not match any of the configured strikes");

        } else {

            // Check if quote's strike is in the configured strikes and continue if it is not.
            strikeIt = find_if(strikes.begin(), strikes.end(),
                               [&q](boost::shared_ptr<BaseStrike> s) { return *s == *q->strike(); });
            if (strikeIt == strikes.end())
                continue;
        }

        // Position of quote in vector of strikes
        Size pos = std::distance(strikes.begin(), strikeIt);

        // Process the quote
        Date eDate = getExpiry(asof, q->expiry(), vc.futureConventionsId(), vc.optionExpiryRollDays());

        // Add quote to surface
        if (surfaceData.count(eDate) == 0)
            surfaceData[eDate] = vector<Real>(numStrikes, Null<Real>());

        QL_REQUIRE(surfaceData[eDate][pos] == Null<Real>(),
                   "Quote " << q->name() << " provides a duplicate quote for the date " << io::iso_date(eDate)
                            << " and strike " << *q->strike());
        surfaceData[eDate][pos] = q->quote()->value();
        quotesAdded++;

        TLOG("Added quote " << q->name() << ": (" << io::iso_date(eDate) << "," << *q->strike() << "," << fixed
                            << setprecision(9) << "," << q->quote()->value() << ")");
    }

    LOG("CommodityVolCurve: added " << quotesAdded << " quotes in building delta strike surface.");

    // Check the data gathered.
    if (!expWc) {
        // If expiries were configured explicitly, the number of configured quotes should equal the
        // number of quotes added.
        QL_REQUIRE(vc.quotes().size() == quotesAdded,
                   "Found " << quotesAdded << " quotes, but " << vc.quotes().size() << " quotes required by config.");
    } else {
        // If the expiries were configured via a wildcard, check that no surfaceData element has a Null<Real>().
        for (const auto& kv : surfaceData) {
            for (Size j = 0; j < numStrikes; j++) {
                QL_REQUIRE(kv.second[j] != Null<Real>(), "Volatility for expiry date "
                                                             << io::iso_date(kv.first) << " and strike " << *strikes[j]
                                                             << " not found. Cannot proceed with a sparse matrix.");
            }
        }
    }

    // Populate the matrix of volatilities and the expiry dates.
    vector<Date> expiryDates;
    Matrix vols(surfaceData.size(), numStrikes);
    for (const auto& row : surfaceData | boost::adaptors::indexed(0)) {
        expiryDates.push_back(row.value().first);
        copy(row.value().second.begin(), row.value().second.end(), vols.row_begin(row.index()));
    }

    // Need to multiply each put delta value by -1 before passing it to the BlackVolatilitySurfaceDelta ctor
    // i.e. a put delta of 0.25 that is passed in to the config must be -0.25 when passed to the ctor.
    transform(putDeltas.begin(), putDeltas.end(), putDeltas.begin(), [](Real pd) { return -1.0 * pd; });
    DLOG("Multiply put deltas by -1.0 before creating BlackVolatilitySurfaceDelta object.");
    DLOG("Put deltas are: " << join(putDeltas | transformed([](Real d) { return ore::data::to_string(d); }), ","));

    // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
    // BlackVolatilitySurfaceDelta time extrapolation is hard-coded to constant in volatility.
    bool flatExtrapolation = true;
    if (vdsc.extrapolation()) {

        auto strikeExtrapType = parseExtrapolation(vdsc.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Strike extrapolation switched to using interpolator.");
            flatExtrapolation = false;
        } else if (strikeExtrapType == Extrapolation::None) {
            DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            DLOG("Strike extrapolation has been set to flat.");
        } else {
            DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vdsc.timeExtrapolation());
        if (timeExtrapType != Extrapolation::Flat) {
            DLOG("BlackVolatilitySurfaceDelta only supports flat volatility extrapolation in the time direction");
        }
    } else {
        DLOG("Extrapolation is turned off for the whole surface so the time and"
             << " strike extrapolation settings are ignored");
    }

    // Time interpolation
    if (vdsc.timeInterpolation() != "Linear") {
        DLOG("BlackVolatilitySurfaceDelta only supports linear time interpolation.");
    }

    // Strike interpolation
    InterpolatedSmileSection::InterpolationMethod im;
    if (vdsc.strikeInterpolation() == "Linear") {
        im = InterpolatedSmileSection::InterpolationMethod::Linear;
    } else if (vdsc.strikeInterpolation() == "NaturalCubic") {
        im = InterpolatedSmileSection::InterpolationMethod::NaturalCubic;
    } else if (vdsc.strikeInterpolation() == "FinancialCubic") {
        im = InterpolatedSmileSection::InterpolationMethod::FinancialCubic;
    } else {
        im = InterpolatedSmileSection::InterpolationMethod::Linear;
        DLOG("BlackVolatilitySurfaceDelta does not support strike interpolation '" << vdsc.strikeInterpolation()
                                                                                   << "' so setting it to linear.");
    }

    // Apply correction to future price term structure if requested and we have a valid expiry calculator
    QL_REQUIRE(!pts_.empty(), "Expected the price term structure to be populated for a delta surface.");
    Handle<PriceTermStructure> cpts = pts_;
    if (vdsc.futurePriceCorrection() && expCalc_)
        cpts = correctFuturePriceCurve(asof, vc.futureConventionsId(), *pts_, expiryDates);

    // Need to construct a dummy spot and foreign yts such that fwd = spot * DF_for / DF
    QL_REQUIRE(!yts_.empty(), "Expected the yield term structure to be populated for a delta surface.");
    Handle<Quote> spot(boost::make_shared<DerivedPriceQuote>(cpts));
    Handle<YieldTermStructure> pyts =
        Handle<YieldTermStructure>(boost::make_shared<PriceTermStructureAdapter>(*cpts, *yts_));
    pyts->enableExtrapolation();

    DLOG("Creating BlackVolatilitySurfaceDelta object");
    bool hasAtm = true;
    volatility_ = boost::make_shared<BlackVolatilitySurfaceDelta>(
        asof, expiryDates, putDeltas, callDeltas, hasAtm, vols, dayCounter_, calendar_, spot, yts_, pyts, deltaType,
        atmType, atmDeltaType, 0 * Days, deltaType, atmType, atmDeltaType, im, flatExtrapolation);

    DLOG("Setting BlackVolatilitySurfaceDelta extrapolation to " << to_string(vdsc.extrapolation()));
    volatility_->enableExtrapolation(vdsc.extrapolation());

    LOG("CommodityVolCurve: finished building 2-D volatility delta strike surface");
}

void CommodityVolCurve::buildVolatility(const Date& asof, CommodityVolatilityConfig& vc,
                                        const VolatilityMoneynessSurfaceConfig& vmsc, const Loader& loader) {

    using boost::adaptors::transformed;
    using boost::algorithm::join;

    LOG("CommodityVolCurve: start building 2-D volatility moneyness strike surface");

    // Parse, sort and check the vector of configured moneyness levels
    vector<Real> moneynessLevels = checkMoneyness(vmsc.moneynessLevels());

    // Expiries may be configured with a wildcard or given explicitly
    bool expWc = false;
    if (find(vmsc.expiries().begin(), vmsc.expiries().end(), "*") != vmsc.expiries().end()) {
        expWc = true;
        QL_REQUIRE(vmsc.expiries().size() == 1, "Wild card expiry specified but more expiries also specified.");
        DLOG("Have expiry wildcard pattern " << vmsc.expiries()[0]);
    }

    // Map to hold the rows of the commodity volatility matrix. The keys are the expiry dates and the values are the
    // vectors of volatilities, one for each configured moneyness.
    map<Date, vector<Real>> surfaceData;

    // Count the number of quotes added. We check at the end that we have added all configured quotes.
    Size quotesAdded = 0;

    // Configured moneyness type.
    MoneynessStrike::Type moneynessType = parseMoneynessType(vmsc.moneynessType());

    // Populate the configured strikes.
    vector<boost::shared_ptr<BaseStrike>> strikes;
    for (const auto& moneynessLevel : moneynessLevels) {
        strikes.push_back(boost::make_shared<MoneynessStrike>(moneynessType, moneynessLevel));
    }

    // Read the quotes to fill the expiry dates and vols matrix.
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

        // Go to next quote if the market data point's date does not equal our asof.
        if (md->asofDate() != asof)
            continue;

        // Go to next quote if not a commodity option quote.
        auto q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);
        if (!q)
            continue;

        // Go to next quote if not a commodity name or currency do not match config.
        if (vc.curveID() != q->commodityName() || vc.currency() != q->quoteCurrency())
            continue;

        // Iterator to one of the configured strikes.
        vector<boost::shared_ptr<BaseStrike>>::iterator strikeIt;

        if (!expWc) {

            // If we have explicitly configured expiries and the quote is not in the configured quotes continue.
            auto it = find(vc.quotes().begin(), vc.quotes().end(), q->name());
            if (it == vc.quotes().end())
                continue;

            // Check if quote's strike is in the configured strikes.
            // It should be as we have selected from the explicitly configured quotes in the last step.
            strikeIt = find_if(strikes.begin(), strikes.end(),
                               [&q](boost::shared_ptr<BaseStrike> s) { return *s == *q->strike(); });
            QL_REQUIRE(strikeIt != strikes.end(),
                       "The quote '"
                           << q->name()
                           << "' is in the list of configured quotes but does not match any of the configured strikes");

        } else {

            // Check if quote's strike is in the configured strikes and continue if it is not.
            strikeIt = find_if(strikes.begin(), strikes.end(),
                               [&q](boost::shared_ptr<BaseStrike> s) { return *s == *q->strike(); });
            if (strikeIt == strikes.end())
                continue;
        }

        // Position of quote in vector of strikes
        Size pos = std::distance(strikes.begin(), strikeIt);

        // Process the quote
        Date eDate = getExpiry(asof, q->expiry(), vc.futureConventionsId(), vc.optionExpiryRollDays());

        // Add quote to surface
        if (surfaceData.count(eDate) == 0)
            surfaceData[eDate] = vector<Real>(moneynessLevels.size(), Null<Real>());

        QL_REQUIRE(surfaceData[eDate][pos] == Null<Real>(),
                   "Quote " << q->name() << " provides a duplicate quote for the date " << io::iso_date(eDate)
                            << " and strike " << *q->strike());
        surfaceData[eDate][pos] = q->quote()->value();
        quotesAdded++;

        TLOG("Added quote " << q->name() << ": (" << io::iso_date(eDate) << "," << *q->strike() << "," << fixed
                            << setprecision(9) << "," << q->quote()->value() << ")");
    }

    LOG("CommodityVolCurve: added " << quotesAdded << " quotes in building moneyness strike surface.");

    // Check the data gathered.
    if (!expWc) {
        // If expiries were configured explicitly, the number of configured quotes should equal the
        // number of quotes added.
        QL_REQUIRE(vc.quotes().size() == quotesAdded,
                   "Found " << quotesAdded << " quotes, but " << vc.quotes().size() << " quotes required by config.");
    } else {
        // If the expiries were configured via a wildcard, check that no surfaceData element has a Null<Real>().
        for (const auto& kv : surfaceData) {
            for (Size j = 0; j < moneynessLevels.size(); j++) {
                QL_REQUIRE(kv.second[j] != Null<Real>(), "Volatility for expiry date "
                                                             << io::iso_date(kv.first) << " and strike " << *strikes[j]
                                                             << " not found. Cannot proceed with a sparse matrix.");
            }
        }
    }

    // Populate the volatility quotes and the expiry times.
    // Rows are moneyness levels and columns are expiry times - this is what the ctor needs below.
    vector<Date> expiryDates(surfaceData.size());
    vector<Time> expiryTimes(surfaceData.size());
    vector<vector<Handle<Quote>>> vols(moneynessLevels.size());
    for (const auto& row : surfaceData | boost::adaptors::indexed(0)) {
        expiryDates[row.index()] = row.value().first;
        expiryTimes[row.index()] = dayCounter_.yearFraction(asof, row.value().first);
        for (Size i = 0; i < row.value().second.size(); i++) {
            vols[i].push_back(Handle<Quote>(boost::make_shared<SimpleQuote>(row.value().second[i])));
        }
    }

    // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
    // BlackVarianceSurfaceMoneyness time extrapolation is hard-coded to constant in volatility.
    bool flatExtrapolation = true;
    if (vmsc.extrapolation()) {

        auto strikeExtrapType = parseExtrapolation(vmsc.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Strike extrapolation switched to using interpolator.");
            flatExtrapolation = false;
        } else if (strikeExtrapType == Extrapolation::None) {
            DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            DLOG("Strike extrapolation has been set to flat.");
        } else {
            DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vmsc.timeExtrapolation());
        if (timeExtrapType != Extrapolation::Flat) {
            DLOG("BlackVarianceSurfaceMoneyness only supports flat volatility extrapolation in the time direction");
        }
    } else {
        DLOG("Extrapolation is turned off for the whole surface so the time and"
             << " strike extrapolation settings are ignored");
    }

    // Time interpolation
    if (vmsc.timeInterpolation() != "Linear") {
        DLOG("BlackVarianceSurfaceMoneyness only supports linear time interpolation in variance.");
    }

    // Strike interpolation
    if (vmsc.strikeInterpolation() != "Linear") {
        DLOG("BlackVarianceSurfaceMoneyness only supports linear strike interpolation in variance.");
    }

    // Apply correction to future price term structure if requested and we have a valid expiry calculator
    QL_REQUIRE(!pts_.empty(), "Expected the price term structure to be populated for a moneyness surface.");
    Handle<PriceTermStructure> cpts = pts_;
    if (vmsc.futurePriceCorrection() && expCalc_)
        cpts = correctFuturePriceCurve(asof, vc.futureConventionsId(), *pts_, expiryDates);

    // Both moneyness surfaces need a spot quote.
    Handle<Quote> spot(boost::make_shared<DerivedPriceQuote>(cpts));

    // The choice of false here is important for forward moneyness. It means that we use the cpts and yts in the
    // BlackVarianceSurfaceMoneynessForward to get the forward value at all times and in particular at times that
    // are after the last expiry time. If we set it to true, BlackVarianceSurfaceMoneynessForward uses a linear
    // interpolated forward curve on the expiry times internally which is poor.
    bool stickyStrike = false;

    if (moneynessType == MoneynessStrike::Type::Forward) {

        QL_REQUIRE(!yts_.empty(), "Expected yield term structure to be populated for a forward moneyness surface.");
        Handle<YieldTermStructure> pyts =
            Handle<YieldTermStructure>(boost::make_shared<PriceTermStructureAdapter>(*cpts, *yts_));
        pyts->enableExtrapolation();

        DLOG("Creating BlackVarianceSurfaceMoneynessForward object");
        volatility_ = boost::make_shared<BlackVarianceSurfaceMoneynessForward>(calendar_, spot, expiryTimes,
                                                                               moneynessLevels, vols, dayCounter_, pyts,
                                                                               yts_, stickyStrike, flatExtrapolation);

    } else {

        DLOG("Creating BlackVarianceSurfaceMoneynessSpot object");
        volatility_ = boost::make_shared<BlackVarianceSurfaceMoneynessSpot>(
            calendar_, spot, expiryTimes, moneynessLevels, vols, dayCounter_, stickyStrike, flatExtrapolation);
    }

    DLOG("Setting BlackVarianceSurfaceMoneyness extrapolation to " << to_string(vmsc.extrapolation()));
    volatility_->enableExtrapolation(vmsc.extrapolation());

    LOG("CommodityVolCurve: finished building 2-D volatility moneyness strike surface");
}

void CommodityVolCurve::buildVolatility(const Date& asof, CommodityVolatilityConfig& vc,
                                        const VolatilityApoFutureSurfaceConfig& vapo,
                                        const Handle<BlackVolTermStructure>& baseVts,
                                        const Handle<PriceTermStructure>& basePts, const Conventions& conventions) {

    LOG("CommodityVolCurve: start building the APO surface");

    // Get the base conventions and create the associated expiry calculator.
    QL_REQUIRE(!vapo.baseConventionsId().empty(),
               "The APO FutureConventions must be populated to build a future APO surface");
    QL_REQUIRE(conventions.has(vapo.baseConventionsId()),
               "Conventions, " << vapo.baseConventionsId() << " for config " << vc.curveID() << " not found.");
    auto baseConvention =
        boost::dynamic_pointer_cast<CommodityFutureConvention>(conventions.get(vapo.baseConventionsId()));
    QL_REQUIRE(baseConvention,
               "Convention with ID '" << vapo.baseConventionsId() << "' should be of type CommodityFutureConvention");

    auto baseExpCalc = boost::make_shared<ConventionsBasedFutureExpiry>(*baseConvention);

    // Get the max tenor from the config if provided.
    boost::optional<QuantLib::Period> maxTenor;
    if (!vapo.maxTenor().empty())
        maxTenor = parsePeriod(vapo.maxTenor());

    // Get the moneyness levels
    vector<Real> moneynessLevels = checkMoneyness(vapo.moneynessLevels());

    // Get the beta parameter to use for valuing the APOs in the surface
    Real beta = vapo.beta();

    // Construct the commodity index.
    auto index = parseCommodityIndex(baseConvention->id(), conventions, false, basePts);

    // Set the strike extrapolation which only matters if extrapolation is turned on for the whole surface.
    // BlackVarianceSurfaceMoneyness, which underlies the ApoFutureSurface, has time extrapolation hard-coded to
    // constant in volatility.
    bool flatExtrapolation = true;
    if (vapo.extrapolation()) {

        auto strikeExtrapType = parseExtrapolation(vapo.strikeExtrapolation());
        if (strikeExtrapType == Extrapolation::UseInterpolator) {
            DLOG("Strike extrapolation switched to using interpolator.");
            flatExtrapolation = false;
        } else if (strikeExtrapType == Extrapolation::None) {
            DLOG("Strike extrapolation cannot be turned off on its own so defaulting to flat.");
        } else if (strikeExtrapType == Extrapolation::Flat) {
            DLOG("Strike extrapolation has been set to flat.");
        } else {
            DLOG("Strike extrapolation " << strikeExtrapType << " not expected so default to flat.");
        }

        auto timeExtrapType = parseExtrapolation(vapo.timeExtrapolation());
        if (timeExtrapType != Extrapolation::Flat) {
            DLOG("ApoFutureSurface only supports flat volatility extrapolation in the time direction");
        }
    } else {
        DLOG("Extrapolation is turned off for the whole surface so the time and"
             << " strike extrapolation settings are ignored");
    }

    // Time interpolation
    if (vapo.timeInterpolation() != "Linear") {
        DLOG("ApoFutureSurface only supports linear time interpolation in variance.");
    }

    // Strike interpolation
    if (vapo.strikeInterpolation() != "Linear") {
        DLOG("ApoFutureSurface only supports linear strike interpolation in variance.");
    }

    DLOG("Creating ApoFutureSurface object");
    volatility_ = boost::make_shared<ApoFutureSurface>(asof, moneynessLevels, index, pts_, yts_, expCalc_, baseVts,
                                                       baseExpCalc, beta, flatExtrapolation, maxTenor);

    DLOG("Setting ApoFutureSurface extrapolation to " << to_string(vapo.extrapolation()));
    volatility_->enableExtrapolation(vapo.extrapolation());

    LOG("CommodityVolCurve: finished building the APO surface");
}

Handle<PriceTermStructure> CommodityVolCurve::correctFuturePriceCurve(const Date& asof, const string& contractName,
                                                                      const boost::shared_ptr<PriceTermStructure>& pts,
                                                                      const vector<Date>& optionExpiries) const {

    LOG("CommodityVolCurve: start adding future price correction at option expiry.");

    // Gather curve dates and prices
    map<Date, Real> curveData;

    // Get existing curve dates and prices. Messy but can't think of another way.
    vector<Date> ptsDates;
    if (auto ipc = boost::dynamic_pointer_cast<InterpolatedPriceCurve<Linear>>(pts)) {
        ptsDates = ipc->pillarDates();
    } else if (auto ipc = boost::dynamic_pointer_cast<InterpolatedPriceCurve<LogLinear>>(pts)) {
        ptsDates = ipc->pillarDates();
    } else if (auto ipc = boost::dynamic_pointer_cast<InterpolatedPriceCurve<Cubic>>(pts)) {
        ptsDates = ipc->pillarDates();
    } else if (auto ipc = boost::dynamic_pointer_cast<InterpolatedPriceCurve<LinearFlat>>(pts)) {
        ptsDates = ipc->pillarDates();
    } else if (auto ipc = boost::dynamic_pointer_cast<InterpolatedPriceCurve<LogLinearFlat>>(pts)) {
        ptsDates = ipc->pillarDates();
    } else if (auto ipc = boost::dynamic_pointer_cast<InterpolatedPriceCurve<CubicFlat>>(pts)) {
        ptsDates = ipc->pillarDates();
    } else {
        DLOG("Could not cast the price term structure so do not have its pillar dates.");
    }

    // Add existing dates and prices to data.
    DLOG("Got " << ptsDates.size() << " pillar dates from the price curve.");
    for (const Date& d : ptsDates) {
        curveData[d] = pts->price(d);
        TLOG("Added (" << io::iso_date(d) << "," << fixed << setprecision(9) << curveData.at(d) << ").");
    }

    // Add future price at option expiry to the curve i.e. override any interpolation between future option
    // expiry (oed) and future expiry (fed).
    for (const Date& oed : optionExpiries) {
        Date fed = expCalc_->nextExpiry(true, oed, 0, false);
        // In general, expect that the future expiry date will already be in curveData but maybe not.
        auto it = curveData.find(fed);
        if (it != curveData.end()) {
            curveData[oed] = it->second;
            TLOG("Found future expiry in existing data. (Option Expiry,Future Expiry,Future Price) is ("
                 << io::iso_date(oed) << "," << io::iso_date(fed) << "," << fixed << setprecision(9)
                 << curveData.at(oed) << ").");
        } else {
            curveData[oed] = pts->price(fed);
            curveData[fed] = pts->price(fed);
            TLOG("Future expiry not found in existing data. (Option Expiry,Future Expiry,Future Price) is ("
                 << io::iso_date(oed) << "," << io::iso_date(fed) << "," << fixed << setprecision(9)
                 << curveData.at(oed) << ").");
        }
    }

    // Gather data for building the "corrected" curve.
    vector<Date> curveDates;
    vector<Real> curvePrices;
    for (const auto& kv : curveData) {
        curveDates.push_back(kv.first);
        curvePrices.push_back(kv.second);
    }
    DayCounter dc = pts->dayCounter();

    // Create the "corrected" curve. Again messy but can't think of another way.
    boost::shared_ptr<PriceTermStructure> cpts;
    if (auto ipc = boost::dynamic_pointer_cast<InterpolatedPriceCurve<Linear>>(pts)) {
        cpts = boost::make_shared<InterpolatedPriceCurve<Linear>>(asof, curveDates, curvePrices, dc, pts->currency());
    } else if (auto ipc = boost::dynamic_pointer_cast<InterpolatedPriceCurve<LogLinear>>(pts)) {
        cpts =
            boost::make_shared<InterpolatedPriceCurve<LogLinear>>(asof, curveDates, curvePrices, dc, pts->currency());
    } else if (auto ipc = boost::dynamic_pointer_cast<InterpolatedPriceCurve<Cubic>>(pts)) {
        cpts = boost::make_shared<InterpolatedPriceCurve<Cubic>>(asof, curveDates, curvePrices, dc, pts->currency());
    } else if (auto ipc = boost::dynamic_pointer_cast<InterpolatedPriceCurve<LinearFlat>>(pts)) {
        cpts =
            boost::make_shared<InterpolatedPriceCurve<LinearFlat>>(asof, curveDates, curvePrices, dc, pts->currency());
    } else if (auto ipc = boost::dynamic_pointer_cast<InterpolatedPriceCurve<LogLinearFlat>>(pts)) {
        cpts = boost::make_shared<InterpolatedPriceCurve<LogLinearFlat>>(asof, curveDates, curvePrices, dc,
                                                                         pts->currency());
    } else if (auto ipc = boost::dynamic_pointer_cast<InterpolatedPriceCurve<CubicFlat>>(pts)) {
        cpts =
            boost::make_shared<InterpolatedPriceCurve<CubicFlat>>(asof, curveDates, curvePrices, dc, pts->currency());
    } else {
        DLOG("Could not cast the price term structure so corrected curve is a linear InterpolatedPriceCurve.");
        cpts = boost::make_shared<InterpolatedPriceCurve<Linear>>(asof, curveDates, curvePrices, dc, pts->currency());
    }
    cpts->enableExtrapolation(pts->allowsExtrapolation());

    LOG("CommodityVolCurve: finished adding future price correction at option expiry.");

    return Handle<PriceTermStructure>(cpts);
}

Date CommodityVolCurve::getExpiry(const Date& asof, const boost::shared_ptr<Expiry>& expiry, const string& name,
                                  Natural rollDays) const {

    Date result;

    if (auto expiryDate = boost::dynamic_pointer_cast<ExpiryDate>(expiry)) {
        result = expiryDate->expiryDate();
    } else if (auto expiryPeriod = boost::dynamic_pointer_cast<ExpiryPeriod>(expiry)) {
        // We may need more conventions here eventually.
        result = calendar_.adjust(asof + expiryPeriod->expiryPeriod());
    } else if (auto fcExpiry = boost::dynamic_pointer_cast<FutureContinuationExpiry>(expiry)) {

        QL_REQUIRE(expCalc_, "CommodityVolCurve::getExpiry: need a future expiry calculator for continuation quotes.");
        QL_REQUIRE(convention_, "CommodityVolCurve::getExpiry: need a future convention for continuation quotes.");
        DLOG("Future option continuation expiry is " << *fcExpiry);

        // Firstly, get the next option expiry on or after the asof date
        result = expCalc_->nextExpiry(true, asof, 0, true);
        TLOG("CommodityVolCurve::getExpiry: next option expiry relative to " << io::iso_date(asof) << " is "
                                                                             << io::iso_date(result) << ".");

        // Market quotes may be delivered with a given number of roll days.
        if (rollDays > 0) {
            Date roll;
            roll = calendar_.advance(result, -static_cast<Integer>(rollDays), Days);
            TLOG("CommodityVolCurve::getExpiry: roll days is " << rollDays << " giving a roll date "
                                                               << io::iso_date(roll) << ".");
            // Take the next option expiry if the roll days means the roll date is before asof.
            if (roll < asof) {
                result = expCalc_->nextExpiry(true, asof, 1, true);
                roll = calendar_.advance(result, -static_cast<Integer>(rollDays), Days);
                QL_REQUIRE(roll > asof, "CommodityVolCurve::getExpiry: expected roll to be greater than asof.");
                TLOG("CommodityVolCurve::getExpiry: roll date " << io::iso_date(roll) << " is less than asof "
                                                                << io::iso_date(asof) << " so take next option expiry "
                                                                << io::iso_date(result));
            }
        }

        // At this stage, 'result' should hold the next option expiry on or after the asof date accounting for roll 
        // days.
        TLOG("CommodityVolCurve::getExpiry: first option expiry is " << io::iso_date(result) << ".");

        // If the continuation index is greater than 1 get the corresponding expiry.
        Natural fcIndex = fcExpiry->expiryIndex();

        // The option continuation expiry may be mapped to another one.
        const auto& ocm = convention_->optionContinuationMappings();
        auto it = ocm.find(fcIndex);
        if (it != ocm.end())
            fcIndex = it->second;

        if (fcIndex > 1) {
            result += 1 * Days;
            result = expCalc_->nextExpiry(true, result, fcIndex - 2, true);
        }

        DLOG("Expiry date corresponding to continuation expiry, " << *fcExpiry <<
            ", is " << io::iso_date(result) << ".");

    } else {
        QL_FAIL("CommodityVolCurve::getExpiry: cannot determine expiry type.");
    }

    return result;
}

void CommodityVolCurve::populateCurves(const CommodityVolatilityConfig& config,
                                       const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                                       const map<string, boost::shared_ptr<CommodityCurve>>& commodityCurves,
                                       bool deltaOrFwdMoneyness) {

    if (deltaOrFwdMoneyness) {
        QL_REQUIRE(!config.yieldCurveId().empty(), "YieldCurveId must be populated to build delta or "
                                                       << "forward moneyness surface.");
        auto itYts = yieldCurves.find(config.yieldCurveId());
        QL_REQUIRE(itYts != yieldCurves.end(), "Can't find yield curve with id " << config.yieldCurveId());
        yts_ = itYts->second->handle();
    }

    QL_REQUIRE(!config.priceCurveId().empty(), "PriceCurveId must be populated to build delta or moneyness surface.");
    auto itPts = commodityCurves.find(config.priceCurveId());
    QL_REQUIRE(itPts != commodityCurves.end(), "Can't find price curve with id " << config.priceCurveId());
    pts_ = Handle<PriceTermStructure>(itPts->second->commodityPriceCurve());
}

vector<Real> CommodityVolCurve::checkMoneyness(const vector<string>& strMoneynessLevels) const {

    using boost::adaptors::transformed;
    using boost::algorithm::join;

    vector<Real> moneynessLevels = parseVectorOfValues<Real>(strMoneynessLevels, &parseReal);
    sort(moneynessLevels.begin(), moneynessLevels.end(), [](Real x, Real y) { return !close(x, y) && x < y; });
    QL_REQUIRE(adjacent_find(moneynessLevels.begin(), moneynessLevels.end(),
                             [](Real x, Real y) { return close(x, y); }) == moneynessLevels.end(),
               "The configured moneyness levels contain duplicates");
    DLOG("Parsed " << moneynessLevels.size() << " unique configured moneyness levels.");
    DLOG("The moneyness levels are: " << join(
             moneynessLevels | transformed([](Real d) { return ore::data::to_string(d); }), ","));

    return moneynessLevels;
}

} // namespace data
} // namespace ore
