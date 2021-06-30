/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/indexed.hpp>
#include <ored/marketdata/equityvolcurve.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>
#include <qle/termstructures/blackvariancesurfacemoneyness.hpp>
#include <qle/termstructures/blackvolatilitysurfacemoneyness.hpp>
#include <qle/termstructures/equityblackvolsurfaceproxy.hpp>
#include <qle/termstructures/equityoptionsurfacestripper.hpp>
#include <qle/termstructures/optionpricesurface.hpp>
#include <regex>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

EquityVolCurve::EquityVolCurve(Date asof, EquityVolatilityCurveSpec spec, const Loader& loader,
                               const CurveConfigurations& curveConfigs, const Handle<EquityIndex>& eqIndex,
                               const map<string, boost::shared_ptr<YieldCurve>>& requiredYieldCurves,
                               const map<string, boost::shared_ptr<EquityCurve>>& requiredEquityCurves,
                               const map<string, boost::shared_ptr<EquityVolCurve>>& requiredEquityVolCurves) {

    try {
        LOG("EquityVolCurve: start building equity volatility structure with ID " << spec.curveConfigID());

        auto config = *curveConfigs.equityVolCurveConfig(spec.curveConfigID());

        calendar_ = parseCalendar(config.calendar());
        // if calendar is null use WeekdaysOnly
        if (calendar_ == NullCalendar())
            calendar_ = WeekendsOnly();
        dayCounter_ = parseDayCounter(config.dayCounter());

        if (config.isProxySurface()) {
            buildVolatility(asof, spec, curveConfigs, requiredEquityCurves, requiredEquityVolCurves);
        } else {
            QL_REQUIRE(config.quoteType() == MarketDatum::QuoteType::PRICE ||
                           config.quoteType() == MarketDatum::QuoteType::RATE_LNVOL,
                       "EquityVolCurve: Only lognormal volatilities and option premiums supported for equity "
                       "volatility surfaces.");

            // Do different things depending on the type of volatility configured
            boost::shared_ptr<VolatilityConfig> vc = config.volatilityConfig();
            if (auto cvc = boost::dynamic_pointer_cast<ConstantVolatilityConfig>(vc)) {
                buildVolatility(asof, config, *cvc, loader);
            } else if (auto vcc = boost::dynamic_pointer_cast<VolatilityCurveConfig>(vc)) {
                buildVolatility(asof, config, *vcc, loader);
            } else if (auto vssc = boost::dynamic_pointer_cast<VolatilityStrikeSurfaceConfig>(vc)) {
                buildVolatility(asof, config, *vssc, loader, eqIndex);
            } else if (auto vmsc = boost::dynamic_pointer_cast<VolatilityMoneynessSurfaceConfig>(vc)) {
                // Need a yield curve (if forward moneyness) and equity index (with dividend yield term structure) to
                // create a moneyness surface.
                MoneynessStrike::Type moneynessType = parseMoneynessType(vmsc->moneynessType());
                bool fwdMoneyness = moneynessType == MoneynessStrike::Type::Forward;
                populateCurves(config, requiredYieldCurves, requiredEquityCurves, fwdMoneyness);
                buildVolatility(asof, config, *vmsc, loader);
            } else {
                QL_FAIL("Unexpected VolatilityConfig in EquityVolatilityConfig");
            }
        }
        DLOG("EquityVolCurve: finished building equity volatility structure with ID " << spec.curveConfigID());

    } catch (exception& e) {
        QL_FAIL("Equity volatility curve building failed : " << e.what());
    } catch (...) {
        QL_FAIL("Equity volatility curve building failed: unknown error");
    }
}

void EquityVolCurve::buildVolatility(const Date& asof, const EquityVolatilityCurveConfig& vc,
                                     const ConstantVolatilityConfig& cvc, const Loader& loader) {
    LOG("EquityVolCurve: start building constant volatility structure");

    QL_REQUIRE(cvc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL ||
                   cvc.quoteType() == MarketDatum::QuoteType::RATE_SLNVOL ||
                   cvc.quoteType() == MarketDatum::QuoteType::RATE_NVOL,
               "Quote for Equity Constant Volatility Config must be a Volatility");

    // Loop over all market datums and find the single quote
    // Return error if there are duplicates (this is why we do not use loader.get() method)
    Real quoteValue = Null<Real>();
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::EQUITY_OPTION) {

            boost::shared_ptr<EquityOptionQuote> q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);

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

    LOG("EquityVolCurve: finished building constant volatility structure");
}

void EquityVolCurve::buildVolatility(const Date& asof, const EquityVolatilityCurveConfig& vc,
                                     const VolatilityCurveConfig& vcc, const Loader& loader) {

    LOG("EquityVolCurve: start building 1-D volatility curve");

    QL_REQUIRE(vcc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL ||
                   vcc.quoteType() == MarketDatum::QuoteType::RATE_SLNVOL ||
                   vcc.quoteType() == MarketDatum::QuoteType::RATE_NVOL,
               "Quote for Equity Constant Volatility Config must be a Volatility");

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

            auto q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);
            if (q && regex_match(q->name(), regexp) && q->quoteType() == vc.quoteType()) {

                TLOG("The quote " << q->name() << " matched the pattern");

                Date expiryDate = getDateFromDateOrPeriod(q->expiry(), asof, calendar_);
                if (expiryDate > asof) {
                    // Add the quote to the curve data
                    QL_REQUIRE(curveData.count(expiryDate) == 0, "Duplicate quote for the expiry date "
                                                                     << io::iso_date(expiryDate)
                                                                     << " provided by equity volatility config "
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

            if (auto q = boost::dynamic_pointer_cast<EquityOptionQuote>(md)) {

                // Find quote name in configured quotes.
                auto it = find(vcc.quotes().begin(), vcc.quotes().end(), q->name());

                if (it != vcc.quotes().end()) {
                    TLOG("Found the configured quote " << q->name());

                    Date expiryDate = getDateFromDateOrPeriod(q->expiry(), asof, calendar_);
                    QL_REQUIRE(expiryDate > asof, "Equity volatility quote '" << q->name()
                                                                              << "' has expiry in the past ("
                                                                              << io::iso_date(expiryDate) << ")");
                    QL_REQUIRE(curveData.count(expiryDate) == 0, "Duplicate quote for the date "
                                                                     << io::iso_date(expiryDate)
                                                                     << " provided by equity volatility config "
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

    LOG("EquityVolCurve: finished building 1-D volatility curve");
}

void EquityVolCurve::buildVolatility(const Date& asof, EquityVolatilityCurveConfig& vc,
                                     const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
                                     const QuantLib::Handle<EquityIndex>& eqIndex) {
    try {

        QL_REQUIRE(vssc.expiries().size() > 0, "No expiries defined");
        QL_REQUIRE(vssc.strikes().size() > 0, "No strikes defined");

        // check for wild cards
        bool expiriesWc = find(vssc.expiries().begin(), vssc.expiries().end(), "*") != vssc.expiries().end();
        bool strikesWc = find(vssc.strikes().begin(), vssc.strikes().end(), "*") != vssc.strikes().end();
        if (expiriesWc) {
            QL_REQUIRE(vssc.expiries().size() == 1, "Wild card expiriy specified but more expiries also specified.");
        }
        if (strikesWc) {
            QL_REQUIRE(vssc.strikes().size() == 1, "Wild card strike specified but more strikes also specified.");
        }
        bool wildCard = strikesWc || expiriesWc;

        vector<Real> callStrikes, putStrikes;
        vector<Real> callData, putData;
        vector<Date> callExpiries, putExpiries;

        // In case of wild card we need the following granularity within the mkt data loop
        bool strikeRelevant = strikesWc;
        bool expiryRelevant = expiriesWc;
        bool quoteRelevant = true;

        // If we do not have a strike wild card, we expect a list of absolute strike values
        vector<Real> configuredStrikes;
        if (!strikesWc) {
            // Parse the list of absolute strikes
            configuredStrikes = parseVectorOfValues<Real>(vssc.strikes(), &parseReal);
            sort(configuredStrikes.begin(), configuredStrikes.end(),
                 [](Real x, Real y) { return !close(x, y) && x < y; });
            QL_REQUIRE(adjacent_find(configuredStrikes.begin(), configuredStrikes.end(),
                                     [](Real x, Real y) { return close(x, y); }) == configuredStrikes.end(),
                       "The configured strikes contain duplicates");
            DLOG("Parsed " << configuredStrikes.size() << " unique configured absolute strikes");
        }

        // We loop over all market data, looking for quotes that match the configuration
        Size callQuotesAdded = 0;
        Size putQuotesAdded = 0;
        for (auto& md : loader.loadQuotes(asof)) {
            // skip irrelevant data
            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::EQUITY_OPTION &&
                md->quoteType() == vc.quoteType()) {
                boost::shared_ptr<EquityOptionQuote> q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);

                if (q->eqName() == vc.curveID() && q->ccy() == vc.ccy()) {
                    // This surface is for absolute strikes only.
                    auto strike = boost::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
                    if (!strike)
                        continue;

                    if (!expiriesWc) {
                        auto j = std::find(vssc.expiries().begin(), vssc.expiries().end(), q->expiry());
                        expiryRelevant = j != vssc.expiries().end();
                    }
                    if (!strikesWc) {
                        // Parse the list of absolute strikes
                        auto i = std::find_if(configuredStrikes.begin(), configuredStrikes.end(),
                                              [&strike](Real s) { return close(s, strike->strike()); });
                        strikeRelevant = i != configuredStrikes.end();
                    }
                    quoteRelevant = strikeRelevant && expiryRelevant;

                    // add quote to vectors, if relevant
                    // If a quote doesn't include a call/put flag (an Implied Vol for example), it
                    // defaults to a call. For an explicit surface we expect either a call and put
                    // for every point, or just a vol at every point
                    if (quoteRelevant) {
                        Date tmpDate = getDateFromDateOrPeriod(q->expiry(), asof, calendar_);
                        QL_REQUIRE(tmpDate > asof,
                                   "Option quote for a past date (" << ore::data::to_string(tmpDate) << ")");

                        if (q->isCall()) {
                            callStrikes.push_back(strike->strike());
                            callData.push_back(q->quote()->value());
                            callExpiries.push_back(tmpDate);
                            callQuotesAdded++;
                        } else {
                            putStrikes.push_back(strike->strike());
                            putData.push_back(q->quote()->value());
                            putExpiries.push_back(tmpDate);
                            putQuotesAdded++;
                        }
                    }
                }
            }
        }

        QL_REQUIRE(callQuotesAdded > 0, "No valid equity volatility quotes provided");
        bool callSurfaceOnly = false;
        if (callQuotesAdded > 0 && putQuotesAdded == 0) {
            QL_REQUIRE(vc.quoteType() != MarketDatum::QuoteType::PRICE,
                       "For Premium quotes, call and put quotes must be supplied.");
            DLOG("EquityVolatilityCurve " << vc.curveID() << ": Only one set of quotes, can build surface directly");
            callSurfaceOnly = true;
        }
        // Check loaded quotes
        if (!wildCard) {
            Size explicitGridSize = vssc.expiries().size() * vssc.strikes().size();
            QL_REQUIRE(callQuotesAdded == explicitGridSize,
                       "EquityVolatilityCurve " << vc.curveID() << ": " << callQuotesAdded << " quotes provided but "
                                                << explicitGridSize << " expected.");
            if (!callSurfaceOnly) {
                QL_REQUIRE(callQuotesAdded == putQuotesAdded,
                           "Call and Put quotes must match for explicitly defined surface, "
                               << callQuotesAdded << " call quotes, and " << putQuotesAdded << " put quotes");
                DLOG("EquityVolatilityCurve " << vc.curveID() << ": Complete set of " << callQuotesAdded
                                              << ", call and put quotes found.");
            }
        }

        QL_REQUIRE(callStrikes.size() == callData.size() && callData.size() == callExpiries.size(),
                   "Quotes loaded don't produce strike,vol,expiry vectors of equal length.");
        QL_REQUIRE(putStrikes.size() == putData.size() && putData.size() == putExpiries.size(),
                   "Quotes loaded don't produce strike,vol,expiry vectors of equal length.");
        DLOG("EquityVolatilityCurve " << vc.curveID() << ": Found " << callQuotesAdded << ", call quotes and "
                                      << putQuotesAdded << " put quotes using wildcard.");

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

        if (vc.quoteType() == MarketDatum::QuoteType::PRICE) {
            // build a call and put price surface

            DLOG("Building a option price surface for calls and puts");
            boost::shared_ptr<OptionPriceSurface> callSurface =
                boost::make_shared<OptionPriceSurface>(asof, callExpiries, callStrikes, callData, dayCounter_);
            boost::shared_ptr<OptionPriceSurface> putSurface =
                boost::make_shared<OptionPriceSurface>(asof, putExpiries, putStrikes, putData, dayCounter_);

            DLOG("CallSurface contains " << callSurface->expiries().size() << " expiries.");

            DLOG("Stripping equity volatility surface from the option premium surfaces");
            boost::shared_ptr<EquityOptionSurfaceStripper> eoss = boost::make_shared<EquityOptionSurfaceStripper>(
                callSurface, putSurface, eqIndex, calendar_, dayCounter_, vc.exerciseType(), flatStrikeExtrap,
                flatStrikeExtrap, flatTimeExtrap);
            vol_ = eoss->volSurface();

        } else if (vc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL) {

            if (callExpiries.size() == 1 && callStrikes.size() == 1) {
                LOG("EquityVolCurve: Building BlackConstantVol");
                vol_ = boost::shared_ptr<BlackVolTermStructure>(
                    new BlackConstantVol(asof, Calendar(), callData[0], dayCounter_));
            } else {
                // create a vol surface from the calls
                boost::shared_ptr<BlackVarianceSurfaceSparse> callSurface =
                    boost::make_shared<BlackVarianceSurfaceSparse>(asof, calendar_, callExpiries, callStrikes, callData,
                                                                   dayCounter_, flatStrikeExtrap, flatStrikeExtrap,
                                                                   flatTimeExtrap);

                if (callSurfaceOnly) {
                    // if only a call surface provided use that
                    vol_ = callSurface;
                } else {
                    // otherwise create a vol surface from puts and strip for a final surface
                    boost::shared_ptr<BlackVarianceSurfaceSparse> putSurface =
                        boost::make_shared<BlackVarianceSurfaceSparse>(asof, calendar_, putExpiries, putStrikes,
                                                                       putData, dayCounter_, flatStrikeExtrap,
                                                                       flatStrikeExtrap, flatTimeExtrap);

                    boost::shared_ptr<EquityOptionSurfaceStripper> eoss =
                        boost::make_shared<EquityOptionSurfaceStripper>(
                            callSurface, putSurface, eqIndex, calendar_, dayCounter_, QuantLib::Exercise::European,
                            flatStrikeExtrap, flatStrikeExtrap, flatTimeExtrap);
                    vol_ = eoss->volSurface();
                }
            }
        } else {
            QL_FAIL("EquityVolatility: Invalid quote type provided.");
        }
        DLOG("Setting BlackVarianceSurfaceSparse extrapolation to " << to_string(vssc.extrapolation()));
        vol_->enableExtrapolation(vssc.extrapolation());

    } catch (std::exception& e) {
        QL_FAIL("equity vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("equity vol curve building failed: unknown error");
    }
}

void EquityVolCurve::buildVolatility(const QuantLib::Date& asof, EquityVolatilityCurveConfig& vc,
                                     const VolatilityMoneynessSurfaceConfig& vmsc, const Loader& loader) {
    LOG("EquityVolCurve: start building 2-D volatility moneyness strike surface");

    try {
        // For background to implementation, see original in 
        // commodityvolcurve.cpp::buildVolatility() for MoneynessSurface configs

        // Expiries may be configured with a wildcard or given explicitly
        bool expWc = find(vmsc.expiries().begin(), vmsc.expiries().end(), "*") != vmsc.expiries().end();
        bool mnyWc =
            find(vmsc.moneynessLevels().begin(), vmsc.moneynessLevels().end(), "*") != vmsc.moneynessLevels().end();
        if (expWc) {
            QL_REQUIRE(vmsc.expiries().size() == 1, "Wildcard expiry specified but more expiries also specified.");
            DLOG("Have expiry wildcard pattern " << vmsc.expiries()[0]);
        }
        if (mnyWc) {
            ELOG("Moneyness level does not support configuration by wildcard (*). Please specify levels explicitly.");
            QL_FAIL("Moneyness level does not support configuration by wildcard (*). Please specify levels explicitly.");
        }

        // Parse, sort and check the vector of configured moneyness levels
        vector<Real> moneynessLevels = checkMoneyness(vmsc.moneynessLevels());

        // Map to hold the rows of the equity volatility matrix. The keys are the expiry dates and the values are the
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

            // Go to next quote if not a equity option quote.
            auto q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);
            if (!q)
                continue;

            // Go to next quote if not a equity name or currency do not match config.
            if (vc.curveID() != q->eqName() || vc.ccy() != q->ccy())
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
                strikeIt = find_if(strikes.begin(), strikes.end(), [&q](boost::shared_ptr<BaseStrike> s) {
                    return *s == *q->strike();
                });
                QL_REQUIRE(
                    strikeIt != strikes.end(),
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
            Date expiryDate = getDateFromDateOrPeriod(q->expiry(), asof, calendar_);

            QL_REQUIRE(expiryDate > asof, "Option quote for a past date (" << ore::data::to_string(expiryDate) << ")");

            // Add quote to surface
            if (surfaceData.count(expiryDate) == 0)
                surfaceData[expiryDate] = vector<Real>(moneynessLevels.size(), Null<Real>());

            QL_REQUIRE(surfaceData[expiryDate][pos] == Null<Real>(),
                       "Quote " << q->name() << " provides a duplicate quote for the date " << io::iso_date(expiryDate)
                                << " and strike " << *q->strike());
            surfaceData[expiryDate][pos] = q->quote()->value();
            quotesAdded++;

            TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," << *q->strike() << fixed
                                << setprecision(9) << "," << q->quote()->value() << ")");
        }

        LOG("EquityVolCurve: added " << quotesAdded << " quotes in building moneyness strike surface.");
        QL_REQUIRE(quotesAdded > 0, "EquityVolCurve requires 1 or more quotes for surface creation.");

        // Check the data gathered.
        if (!expWc) {
            // If expiries were configured explicitly, the number of configured quotes should equal the
            // number of quotes added.
            QL_REQUIRE(vc.quotes().size() == quotesAdded, "Found " << quotesAdded << " quotes, but "
                                                                   << vc.quotes().size()
                                                                   << " quotes required by config.");
        } else {
            // If the expiries were configured via a wildcard, check that no surfaceData element has a Null<Real>().
            for (const auto& kv : surfaceData) {
                for (Size j = 0; j < moneynessLevels.size(); j++) {
                    QL_REQUIRE(kv.second[j] != Null<Real>(),
                               "Volatility for expiry date " << io::iso_date(kv.first) << " and strike " << *strikes[j]
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
                DLOG("BlackVarianceSurfaceMoneyness only supports flat volatility extrapolation in the time direction.");
            }
        } else {
            DLOG("Extrapolation is turned off for the whole surface so the time and"
                 << " strike extrapolation settings are ignored.");
        }

        // Time interpolation
        if (vmsc.timeInterpolation() != "Linear") {
            DLOG("BlackVarianceSurfaceMoneyness only supports linear time interpolation in variance.");
        }

        // Strike interpolation
        if (vmsc.strikeInterpolation() != "Linear") {
            DLOG("BlackVarianceSurfaceMoneyness only supports linear strike interpolation in variance.");
        }

        // Verify that the equity index holds required term structures
        QL_REQUIRE(!eqInd_.empty(), "Expected the equity index with forecast and dividend yts to be \
            populated for a moneyness surface.");

        // Both moneyness surfaces need a spot quote.
        Handle<Quote> spot(eqInd_->equitySpot());

        // The choice of false here is important for forward moneyness. It means that we use the ceqInd and yts in the
        // BlackVarianceSurfaceMoneynessForward to get the forward value at all times and in particular at times that
        // are after the last expiry time. If we set it to true, BlackVarianceSurfaceMoneynessForward uses a linear
        // interpolated forward curve on the expiry times internally which is poor.
        bool stickyStrike = false;

        if (moneynessType == MoneynessStrike::Type::Forward) {
            QL_REQUIRE(!yts_.empty(), "Expected yield term structure to be populated for a forward moneyness surface.");
            Handle<YieldTermStructure> divyts = Handle<YieldTermStructure>(eqInd_->equityDividendCurve());
            divyts->enableExtrapolation();
            
            if (vmsc.interpolationVariable() == VolatilityMoneynessSurfaceConfig::InterpolationVariable::Variance) {
                DLOG("Creating BlackVarianceSurfaceMoneynessForward object.");
                vol_ = boost::make_shared<BlackVarianceSurfaceMoneynessForward>(
                    calendar_, spot, expiryTimes, moneynessLevels, vols, dayCounter_, divyts, yts_, stickyStrike,
                    flatExtrapolation);
            } else if (vmsc.interpolationVariable() ==
                       VolatilityMoneynessSurfaceConfig::InterpolationVariable::Volatility) {
                DLOG("Creating BlackVolatilitySurfaceMoneynessForward object.");
                vol_ = boost::make_shared<BlackVolatilitySurfaceMoneynessForward>(
                    calendar_, spot, expiryTimes, moneynessLevels, vols, dayCounter_, divyts, yts_, stickyStrike,
                    flatExtrapolation);
            }
        } else {
            if (vmsc.interpolationVariable() == VolatilityMoneynessSurfaceConfig::InterpolationVariable::Variance) {
                DLOG("Creating BlackVarianceSurfaceMoneynessSpot object.");
                vol_ = boost::make_shared<BlackVarianceSurfaceMoneynessSpot>(
                    calendar_, spot, expiryTimes, moneynessLevels, vols, dayCounter_, stickyStrike, flatExtrapolation);
            } else if (vmsc.interpolationVariable() ==
                       VolatilityMoneynessSurfaceConfig::InterpolationVariable::Volatility) {
                DLOG("Creating BlackVolatilitySurfaceMoneynessSpot object.");
                vol_ = boost::make_shared<BlackVolatilitySurfaceMoneynessSpot>(
                    calendar_, spot, expiryTimes, moneynessLevels, vols, dayCounter_, stickyStrike, flatExtrapolation);
            }
        }

        DLOG("Setting BlackVarianceSurfaceMoneyness/BlackVolatilitySurfaceMoneyness extrapolation to "
             << to_string(vmsc.extrapolation()));
        vol_->enableExtrapolation(vmsc.extrapolation());

        LOG("EquityVolCurve: finished building 2-D volatility moneyness strike surface.");
    } catch (const std::exception& e) {
        QL_FAIL("equity vol curve building failed :" << e.what());
    }
    catch (...) {
        QL_FAIL("equity vol curve building failed: unknown error");
    }
}

void EquityVolCurve::buildVolatility(const QuantLib::Date& asof, const EquityVolatilityCurveSpec& spec,
                                     const CurveConfigurations& curveConfigs,
                                     const map<string, boost::shared_ptr<EquityCurve>>& eqCurves,
                                     const map<string, boost::shared_ptr<EquityVolCurve>>& eqVolCurves) {

    // get all the configurations and the curve needed for proxying
    auto config = *curveConfigs.equityVolCurveConfig(spec.curveConfigID());

    auto proxy = config.proxySurface();
    auto eqConfig = *curveConfigs.equityCurveConfig(spec.curveConfigID());
    auto proxyConfig = *curveConfigs.equityCurveConfig(proxy);
    auto proxyVolConfig = *curveConfigs.equityVolCurveConfig(proxy);

    // create dummy specs to look up the required curves
    EquityCurveSpec eqSpec(eqConfig.currency(), spec.curveConfigID());
    EquityCurveSpec proxySpec(proxyConfig.currency(), proxy);
    EquityVolatilityCurveSpec proxyVolSpec(proxyVolConfig.ccy(), proxy);

    // Get all necessary curves
    auto curve = eqCurves.find(eqSpec.name());
    QL_REQUIRE(curve != eqCurves.end(), "Failed to find equity curve, when building equity vol curve " << spec.name());
    auto proxyCurve = eqCurves.find(proxySpec.name());
    QL_REQUIRE(proxyCurve != eqCurves.end(), "Failed to find equity curve for proxy "
                                                 << proxySpec.name() << ", when building equity vol curve "
                                                 << spec.name());
    auto proxyVolCurve = eqVolCurves.find(proxyVolSpec.name());
    QL_REQUIRE(proxyVolCurve != eqVolCurves.end(), "Failed to find equity vol curve for proxy "
                                                       << proxyVolSpec.name() << ", when building equity vol curve "
                                                       << spec.name());

    vol_ = boost::make_shared<EquityBlackVolatilitySurfaceProxy>(
        proxyVolCurve->second->volTermStructure(), curve->second->equityIndex(), proxyCurve->second->equityIndex());
}

void EquityVolCurve::populateCurves(const EquityVolatilityCurveConfig& config,
                                    const std::map<std::string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                                    const std::map<std::string, boost::shared_ptr<EquityCurve>>& equityCurves,
                                    const bool deltaOrFwdMoneyness) {

    if (deltaOrFwdMoneyness) {
        QL_REQUIRE(!config.yieldCurveId().empty(), "YieldCurveId must be populated to build delta or "
                                                       << "forward moneyness surface.");
        auto itYts = yieldCurves.find(config.yieldCurveId());
        QL_REQUIRE(itYts != yieldCurves.end(), "Can't find yield curve with id " << config.yieldCurveId());
        yts_ = itYts->second->handle();
    }

    QL_REQUIRE(!config.equityCurveId().empty(), "EquityCurveId must be populated to build delta or moneyness surface.");
    auto itEqc = equityCurves.find(config.equityCurveId());
    QL_REQUIRE(itEqc != equityCurves.end(), "Can't find equity curve with id " << config.equityCurveId());
    eqInd_ = Handle<EquityIndex>(itEqc->second->equityIndex());
}

vector<Real> EquityVolCurve::checkMoneyness(const vector<string>& strMoneynessLevels) const {
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
