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
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/wildcard.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

CDSVolCurve::CDSVolCurve(Date asof, CDSVolatilityCurveSpec spec, const Loader& loader,
                         const CurveConfigurations& curveConfigs) {

    try {

        LOG("CDSVolCurve: start building CDS volatility structure with ID " << spec.curveConfigID());

        QL_REQUIRE(curveConfigs.hasCdsVolCurveConfig(spec.curveConfigID()), "No curve configuration found for CDS "
                                                                                << "volatility curve spec with ID "
                                                                                << spec.curveConfigID() << ".");
        auto config = *curveConfigs.cdsVolCurveConfig(spec.curveConfigID());

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
            QL_FAIL("CDSVolCurve does not support a VolatilityDeltaSurfaceConfig yet.");
        } else if (auto vmsc = boost::dynamic_pointer_cast<VolatilityMoneynessSurfaceConfig>(vc)) {
            QL_FAIL("CDSVolCurve does not support a VolatilityMoneynessSurfaceConfig yet.");
        } else if (auto vapo = boost::dynamic_pointer_cast<VolatilityApoFutureSurfaceConfig>(vc)) {
            QL_FAIL("VolatilityApoFutureSurfaceConfig does not make sense for CDSVolCurve.");
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

    // Loop over all market datums and find the single quote
    // Return error if there are duplicates (this is why we do not use loader.get() method)
    Real quoteValue = Null<Real>();
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::INDEX_CDS_OPTION) {

            boost::shared_ptr<IndexCDSOptionQuote> q = boost::dynamic_pointer_cast<IndexCDSOptionQuote>(md);

            if (q->name() == cvc.quote()) {
                TLOG("Found the constant volatility quote " << q->name());
                QL_REQUIRE(quoteValue == Null<Real>(), "Duplicate quote found for quote with id " << cvc.quote());
                quoteValue = q->quote()->value();
            }
        }
    }
    QL_REQUIRE(quoteValue != Null<Real>(), "Quote not found for id " << cvc.quote());

    DLOG("Creating BlackConstantVol structure");
    vol_ = boost::make_shared<BlackConstantVol>(asof, calendar_, quoteValue, dayCounter_);

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
    map<Date, Real> curveData;

    // Different approaches depending on whether we are using a regex or searching for a list of explicit quotes.
    if (wildcard) {

        DLOG("Have single quote with pattern " << (*wildcard).regex());

        // Loop over quotes and process CDS option quotes matching pattern on asof
        for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

            // Go to next quote if the market data point's date does not equal our asof
            if (md->asofDate() != asof)
                continue;

            auto q = boost::dynamic_pointer_cast<IndexCDSOptionQuote>(md);
            if (q && (*wildcard).matches(q->name())) {

                TLOG("The quote " << q->name() << " matched the pattern");

                Date expiryDate = getExpiry(asof, q->expiry());
                if (expiryDate > asof) {
                    // Add the quote to the curve data
                    QL_REQUIRE(curveData.count(expiryDate) == 0, "Duplicate quote for the expiry date "
                                                                     << io::iso_date(expiryDate)
                                                                     << " provided by CDS volatility config "
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

        // Loop over quotes and process CDS option quotes that are explicitly specified in the config
        for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

            // Go to next quote if the market data point's date does not equal our asof
            if (md->asofDate() != asof)
                continue;

            if (auto q = boost::dynamic_pointer_cast<IndexCDSOptionQuote>(md)) {

                // Find quote name in configured quotes.
                auto it = find(vcc.quotes().begin(), vcc.quotes().end(), q->name());

                if (it != vcc.quotes().end()) {
                    TLOG("Found the configured quote " << q->name());

                    Date expiryDate = getExpiry(asof, q->expiry());
                    QL_REQUIRE(expiryDate > asof, "CDS volatility quote '" << q->name() << "' has expiry in the past ("
                                                                           << io::iso_date(expiryDate) << ")");
                    QL_REQUIRE(curveData.count(expiryDate) == 0, "Duplicate quote for the date "
                                                                     << io::iso_date(expiryDate)
                                                                     << " provided by CDS volatility config "
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
    auto tmp =
        boost::make_shared<BlackVarianceCurve>(asof, dates, volatilities, dayCounter_, vcc.enforceMontoneVariance());

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
    vol_ = tmp;

    // Set the extrapolation
    if (parseExtrapolation(vcc.extrapolation()) == Extrapolation::Flat) {
        DLOG("Enabling BlackVarianceCurve flat volatility extrapolation.");
        vol_->enableExtrapolation();
    } else if (parseExtrapolation(vcc.extrapolation()) == Extrapolation::None) {
        DLOG("Disabling BlackVarianceCurve extrapolation.");
        vol_->disableExtrapolation();
    } else if (parseExtrapolation(vcc.extrapolation()) == Extrapolation::UseInterpolator) {
        DLOG("BlackVarianceCurve does not support using interpolator for extrapolation "
             << "so default to flat volatility extrapolation.");
        vol_->enableExtrapolation();
    } else {
        DLOG("Unexpected extrapolation so default to flat volatility extrapolation.");
        vol_->enableExtrapolation();
    }

    LOG("CDSVolCurve: finished building 1-D volatility curve");
}

void CDSVolCurve::buildVolatility(const Date& asof, CDSVolatilityCurveConfig& vc,
                                  const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader) {

    LOG("CDSVolCurve: start building 2-D volatility absolute strike surface");

    // We are building a cds volatility surface here of the form expiry vs strike where the strikes are absolute
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

    // Loop over quotes and process any CDS option quote that matches a wildcard
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

        // Go to next quote if the market data point's date does not equal our asof.
        if (md->asofDate() != asof)
            continue;

        // Go to next quote if not a CDS option quote.
        auto q = boost::dynamic_pointer_cast<IndexCDSOptionQuote>(md);
        if (!q)
            continue;

        // Go to next quote if index name in the quote does not match the cds vol configuration name.
        if (vc.curveID() != q->indexName() && vc.quoteName() != q->indexName())
            continue;

        // This surface is for absolute strikes only.
        auto strike = boost::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
        if (!strike)
            continue;

        // Make sure that the CDS index option quote term matches the configured term
        if (q->indexTerm() != vc.nameTerm().second)
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
        expiries.push_back(getExpiry(asof, q->expiry()));
        strikes.push_back(strike->strike() / vc.strikeFactor());
        vols.push_back(q->quote()->value());
        quotesAdded++;

        TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiries.back()) << "," << fixed << setprecision(9)
                            << strikes.back() << "," << vols.back() << ")");
    }

    LOG("CDSVolCurve: added " << quotesAdded << " quotes in building wildcard based absolute strike surface.");
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
    vol_ = boost::make_shared<BlackVarianceSurfaceSparse>(asof, calendar_, expiries, strikes, vols, dayCounter_,
                                                          flatStrikeExtrap, flatStrikeExtrap, flatTimeExtrap);

    DLOG("Setting BlackVarianceSurfaceSparse extrapolation to " << to_string(vssc.extrapolation()));
    vol_->enableExtrapolation(vssc.extrapolation());

    LOG("CDSVolCurve: finished building 2-D volatility absolute strike surface");
}

void CDSVolCurve::buildVolatilityExplicit(const Date& asof, CDSVolatilityCurveConfig& vc,
                                          const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
                                          const vector<Real>& configuredStrikes) {

    LOG("CDSVolCurve: start building 2-D volatility absolute strike surface with explicit strikes and expiries");

    // Map to hold the rows of the CDS volatility matrix. The keys are the expiry dates and the values are the
    // vectors of volatilities, one for each configured strike.
    map<Date, vector<Real>> surfaceData;

    // Count the number of quotes added. We check at the end that we have added all configured quotes.
    Size quotesAdded = 0;

    // Loop over quotes and process CDS option quotes that have been requested
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {

        // Go to next quote if the market data point's date does not equal our asof.
        if (md->asofDate() != asof)
            continue;

        // Go to next quote if not a CDS option quote.
        auto q = boost::dynamic_pointer_cast<IndexCDSOptionQuote>(md);
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
        Date eDate = getExpiry(asof, q->expiry());

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

    LOG("CDSVolCurve: added " << quotesAdded << " quotes in building explicit absolute strike surface.");

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

    // Divide the configured strikes by the strike factor before using in surface
    vector<Real> strikes = configuredStrikes;
    for (Real& strike : strikes) {
        strike /= vc.strikeFactor();
    }

    DLOG("Creating BlackVarianceSurface object");
    auto tmp = boost::make_shared<BlackVarianceSurface>(asof, calendar_, expiryDates, strikes, volatilities,
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
    vol_ = tmp;

    DLOG("Setting BlackVarianceSurface extrapolation to " << to_string(vssc.extrapolation()));
    vol_->enableExtrapolation(vssc.extrapolation());

    LOG("CDSVolCurve: finished building 2-D volatility absolute strike "
        << "surface with explicit strikes and expiries");
}

Date CDSVolCurve::getExpiry(const Date& asof, const boost::shared_ptr<Expiry>& expiry) const {

    Date result;

    if (auto expiryDate = boost::dynamic_pointer_cast<ExpiryDate>(expiry)) {
        result = expiryDate->expiryDate();
    } else if (auto expiryPeriod = boost::dynamic_pointer_cast<ExpiryPeriod>(expiry)) {
        // We may need more conventions here eventually.
        result = calendar_.adjust(asof + expiryPeriod->expiryPeriod());
    } else if (auto fcExpiry = boost::dynamic_pointer_cast<FutureContinuationExpiry>(expiry)) {
        QL_FAIL("CDSVolCurve::getExpiry: future continuation expiry not supported for CDS volatility quotes.");
    } else {
        QL_FAIL("CDSVolCurve::getExpiry: cannot determine expiry type.");
    }

    return result;
}

} // namespace data
} // namespace ore
