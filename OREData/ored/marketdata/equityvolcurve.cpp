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
#include <ored/marketdata/equityvolcurve.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>
#include <qle/termstructures/optionpricesurface.hpp>
#include <qle/termstructures/equityoptionsurfacestripper.hpp>
#include <regex>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

EquityVolCurve::EquityVolCurve(Date asof, EquityVolatilityCurveSpec spec, const Loader& loader,
    const CurveConfigurations& curveConfigs, const Handle<EquityIndex>& eqIndex) {

    try {
        LOG("EquityVolCurve: start building equity volatility structure with ID " << spec.curveConfigID());

        auto config = *curveConfigs.equityVolCurveConfig(spec.curveConfigID());

        QL_REQUIRE(config.quoteType() == MarketDatum::QuoteType::PRICE || config.quoteType() == MarketDatum::QuoteType::RATE_LNVOL,
            "EquityVolCurve: Only lognormal volatilities and option premiums supported for equity volatility surfaces.");
        
        calendar_ = parseCalendar(config.calendar());
        // if calendar is null use WeekdaysOnly
        if (calendar_ == NullCalendar())
            calendar_ = WeekendsOnly();
        dayCounter_ = parseDayCounter(config.dayCounter());

        // Do different things depending on the type of volatility configured
        boost::shared_ptr<VolatilityConfig> vc = config.volatilityConfig();
        if (auto cvc = boost::dynamic_pointer_cast<ConstantVolatilityConfig>(vc)) {
            buildVolatility(asof, config, *cvc, loader);
        } else if (auto vcc = boost::dynamic_pointer_cast<VolatilityCurveConfig>(vc)) {
            buildVolatility(asof, config, *vcc, loader);
        } else if (auto vssc = boost::dynamic_pointer_cast<VolatilityStrikeSurfaceConfig>(vc)) {
            buildVolatility(asof, config, *vssc, loader, eqIndex);
        } else {
            QL_FAIL("Unexpected VolatilityConfig in EquityVolatilityConfig");
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

    QL_REQUIRE(cvc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL || cvc.quoteType() == MarketDatum::QuoteType::RATE_SLNVOL ||
        cvc.quoteType() == MarketDatum::QuoteType::RATE_NVOL, "Quote for Equity Constant Volatility Config must be a Volatility");

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

    QL_REQUIRE(vcc.quoteType() == MarketDatum::QuoteType::RATE_LNVOL || vcc.quoteType() == MarketDatum::QuoteType::RATE_SLNVOL ||
        vcc.quoteType() == MarketDatum::QuoteType::RATE_NVOL, "Quote for Equity Constant Volatility Config must be a Volatility");


    // Must have at least one quote
    QL_REQUIRE(vcc.quotes().size() > 0, "No quotes specified in config " << vc.curveID());

    // Check if we are using a regular expression to select the quotes for the curve. If we are, the quotes should 
    // contain exactly one element.
    bool isRegex = false;
    for (Size i = 0; i < vcc.quotes().size(); i++) {
        if ((isRegex = vcc.quotes()[i].find("*") != string::npos)) {
            QL_REQUIRE(i == 0 && vcc.quotes().size() == 1, "Wild card config, " <<
                vc.curveID() << ", should have exactly one quote.");
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
                    QL_REQUIRE(curveData.count(expiryDate) == 0, "Duplicate quote for the expiry date " <<
                        io::iso_date(expiryDate) << " provided by equity volatility config " << vc.curveID());
                    curveData[expiryDate] = q->quote()->value();

                    TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," <<
                        fixed << setprecision(9) << q->quote()->value() << ")");
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
                    QL_REQUIRE(expiryDate > asof, "Equity volatility quote '" << q->name() <<
                        "' has expiry in the past (" << io::iso_date(expiryDate) << ")");
                    QL_REQUIRE(curveData.count(expiryDate) == 0, "Duplicate quote for the date " <<
                        io::iso_date(expiryDate) << " provided by equity volatility config " << vc.curveID());
                    curveData[expiryDate] = q->quote()->value();

                    TLOG("Added quote " << q->name() << ": (" << io::iso_date(expiryDate) << "," <<
                        fixed << setprecision(9) << q->quote()->value() << ")");
                }
            }
        }

        // Check that we have found all of the explicitly configured quotes
        QL_REQUIRE(curveData.size() == vcc.quotes().size(), "Found " << curveData.size() << " quotes, but "
            << vcc.quotes().size() << " quotes were given in config.");
    }

    // Create the dates and volatility vector
    vector<Date> dates;
    vector<Volatility> volatilities;
    for (const auto& datum : curveData) {
        dates.push_back(datum.first);
        volatilities.push_back(datum.second);
        TLOG("Added data point (" << io::iso_date(dates.back()) <<
            "," << fixed << setprecision(9) << volatilities.back() << ")");
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
        DLOG("BlackVarianceCurve does not support using interpolator for extrapolation " <<
            "so default to flat volatility extrapolation.");
        vol_->enableExtrapolation();
    } else {
        DLOG("Unexpected extrapolation so default to flat volatility extrapolation.");
        vol_->enableExtrapolation();
    }

    LOG("EquityVolCurve: finished building 1-D volatility curve");
}

void EquityVolCurve::buildVolatility(const Date& asof, EquityVolatilityCurveConfig& vc,
    const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
    const QuantLib::Handle<EquityIndex>& eqIndex){
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

        // We loop over all market data, looking for quotes that match the configuration
        Size callQuotesAdded = 0;
        Size putQuotesAdded = 0;
        for (auto& md : loader.loadQuotes(asof)) {
            // skip irrelevant data
            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::EQUITY_OPTION &&
                md->quoteType() == vc.quoteType() ) {
                boost::shared_ptr<EquityOptionQuote> q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);

                if (q->eqName() == vc.curveID() && q->ccy() == vc.ccy()) {
                    if (!expiriesWc) {
                        auto j = std::find(vssc.expiries().begin(), vssc.expiries().end(), q->expiry());
                        expiryRelevant = j != vssc.expiries().end();
                    }
                    if (!strikesWc) {
                        auto i = std::find(vssc.strikes().begin(), vssc.strikes().end(), q->strike());
                        strikeRelevant = i != vssc.strikes().end();
                    } else {
                        // todo - for now we will ignore ATMF quotes in case of strike wild card. ----
                        strikeRelevant = q->strike() != "ATMF";
                    }
                    quoteRelevant = strikeRelevant && expiryRelevant;
                    
                    // add quote to vectors, if relevant
                    // If a quote doesn't include a call/put flag (an Implied Vol for example), it
                    // defaults to a call. For an explicit surface we expect either a call and put
                    // for every point, or just a vol at every point
                    if (quoteRelevant) {
                        Date tmpDate = getDateFromDateOrPeriod(q->expiry(), asof, calendar_);
                        QL_REQUIRE(tmpDate > asof, "Option quote for a past date (" << ore::data::to_string(tmpDate) << ")");
                        if (q->isCall()) {
                            callStrikes.push_back(parseReal(q->strike()));
                            callData.push_back(q->quote()->value());
                            callExpiries.push_back(tmpDate);
                            callQuotesAdded++;
                        } else {
                            putStrikes.push_back(parseReal(q->strike()));
                            putData.push_back(q->quote()->value());
                            putExpiries.push_back(tmpDate);
                            putQuotesAdded++;
                        }
                    }                    
                }
            }
        }

        bool callSurfaceOnly = false;
        if (callQuotesAdded > 0 && putQuotesAdded == 0) {
            QL_REQUIRE(vc.quoteType() != MarketDatum::QuoteType::PRICE, "For Premium quotes, call and put quotes must be supplied.");
            DLOG("EquityVolatilityCurve " << vc.curveID() << ": Only one set of quotes, can build surface directly");
            callSurfaceOnly = true;
        }
        // Check loaded quotes
        if (!wildCard) {
            Size explicitGridSize = vssc.expiries().size() * vssc.strikes().size();
            QL_REQUIRE(callQuotesAdded == explicitGridSize, "EquityVolatilityCurve " << vc.curveID() << ": " << callQuotesAdded <<
                " quotes provided but " << explicitGridSize << " expected.");
            if (!callSurfaceOnly) {
                QL_REQUIRE(callQuotesAdded == putQuotesAdded, "Call and Put quotes must match for explicitly defined surface, "
                    << callQuotesAdded << " call quotes, and " << putQuotesAdded << " put quotes");
                DLOG("EquityVolatilityCurve " << vc.curveID() << ": Complete set of " << callQuotesAdded << ", call and put quotes found.");
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
            DLOG("Extrapolation is turned off for the whole surface so the time and" <<
                " strike extrapolation settings are ignored");
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
                        dayCounter_, flatStrikeExtrap, flatStrikeExtrap, flatTimeExtrap);

                if (callSurfaceOnly) {
                    // if only a call surface provided use that
                    vol_ = callSurface;
                } else {
                    // otherwise create a vol surface from puts and strip for a final surface
                    boost::shared_ptr<BlackVarianceSurfaceSparse> putSurface =
                        boost::make_shared<BlackVarianceSurfaceSparse>(asof, calendar_, putExpiries, putStrikes, putData, 
                            dayCounter_, flatStrikeExtrap, flatStrikeExtrap, flatTimeExtrap);

                    boost::shared_ptr<EquityOptionSurfaceStripper> eoss = boost::make_shared<EquityOptionSurfaceStripper>(
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


} // namespace data
} // namespace ore
