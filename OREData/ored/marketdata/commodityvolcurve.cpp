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

#include <ored/marketdata/commodityvolcurve.hpp>

#include <algorithm>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/math/matrix.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <qle/math/fillemptymatrix.hpp>

#include <regex>

using namespace std;
using namespace QuantLib;

namespace ore {
namespace data {

CommodityVolCurve::CommodityVolCurve(const Date& asof, const CommodityVolatilityCurveSpec& spec, const Loader& loader,
                                     const CurveConfigurations& curveConfigs) {

    try {
        boost::shared_ptr<CommodityVolatilityCurveConfig> config =
            curveConfigs.commodityVolatilityCurveConfig(spec.curveConfigID());

        // Do different things depending on the type of volatility configured
        switch (config->type()) {
        case CommodityVolatilityCurveConfig::Type::Constant:
            buildConstantVolatility(asof, *config, loader);
            break;
        case CommodityVolatilityCurveConfig::Type::Curve:
            buildVolatilityCurve(asof, *config, loader);
            break;
        case CommodityVolatilityCurveConfig::Type::Surface:
            buildVolatilitySurface(asof, *config, loader);
            break;
        default:
            QL_FAIL("Commodity volatility type not recognised");
        }

        // Enable/disable extrapolation as per config
        volatility_->enableExtrapolation(config->extrapolate());

    } catch (std::exception& e) {
        QL_FAIL("Commodity volatility curve building failed : " << e.what());
    } catch (...) {
        QL_FAIL("Commodity volatility curve building failed: unknown error");
    }
}

void CommodityVolCurve::buildConstantVolatility(const Date& asof, CommodityVolatilityCurveConfig& config,
                                                const Loader& loader) {
    // Should have a single quote for constant volatility structure
    QL_REQUIRE(config.quotes().size() == 1, "Expected a single quote when the commodity volatility type is constant");
    string quoteId = config.quotes()[0];

    // Loop over all market datums and find the single quote
    // Return error if there are duplicates (this is why we do not use loader.get() method)
    Real quoteValue = Null<Real>();
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION) {

            boost::shared_ptr<CommodityOptionQuote> q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);

            if (q->name() == quoteId) {
                QL_REQUIRE(quoteValue == Null<Real>(), "Duplicate quote found for quote with id " << quoteId);
                quoteValue = q->quote()->value();
            }
        }
    }
    QL_REQUIRE(quoteValue != Null<Real>(), "Quote not found for id " << quoteId);

    // Build the constant volatility structure
    DayCounter dayCounter = parseDayCounter(config.dayCounter());
    Calendar calendar = parseCalendar(config.calendar());
    LOG("CommodityVolatilityCurve: Building BlackConstantVol term structure");
    volatility_ = boost::make_shared<BlackConstantVol>(asof, calendar, quoteValue, dayCounter);
}

void CommodityVolCurve::buildVolatilityCurve(const Date& asof, CommodityVolatilityCurveConfig& config,
                                             const Loader& loader) {

    // Loop over all market datums and find the quotes *in* the config
    // Return error if there are duplicate quotes (this is why we do not use loader.get() method)

    QL_REQUIRE(config.quotes().size() > 0, "No quotes specified in config " << config.curveID());

    size_t found;
    bool found_regex = false;
    regex reg1;

    // check for regex string in config
    for (int i = 0; i < config.quotes().size(); i++) {
        found = config.quotes()[i].find("*"); // find '*' char in quote
        found_regex = (found != string::npos) ? true : found_regex;
        if (found_regex) {
            break;
        }
    }

    if (found_regex) {
        QL_REQUIRE(config.quotes().size() == 1,
                   "Wild card specified in config " << config.curveID << " but more quotes also specified.")
        found = config.quotes()[0].find('*');

        // build regex string
        regex re("(\\*)");
        string regexstr = config.quotes[0];
        regexstr = regex_replace(regexstr, re, ".*");
        reg1 = regex(regexstr);
    }

    map<Date, Real> curveData;
    Calendar calendar = parseCalendar(config.calendar());
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION) {

            boost::shared_ptr<CommodityOptionQuote> q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);

            // relevant quote?
            bool add_quote = false;
            string q_name;                     // for printing quote name (wild-card or not)
            vector<string>::const_iterator it; // if not wild-card -> find quote.
            if (found_regex) {
                if (regex_match(q->name(), reg1)) {
                    add_quote = true;
                    q_name = config.quotes()[0];
                }
            } else {
                it = find(config.quotes().begin(), config.quotes().end(), q->name()); // find in config
                if (it != config.quotes().end()) {
                    add_quote = true;
                    q_name = *it;
                }
            }

            // add quote if relevant
            if (add_quote) {
                // Convert expiry to a date if necessary
                Date aDate;
                Period aPeriod;
                bool isDate;
                parseDateOrPeriod(q->expiry(), aDate, aPeriod, isDate);
                if (isDate) {
                    QL_REQUIRE(aDate > asof, "Commodity volatility quote '" << q_name << "' is for a past date ("
                                                                            << io::iso_date(aDate) << ")");
                } else {
                    aDate = calendar.adjust(asof + aPeriod);
                }

                // Add the quote to the curve data
                QL_REQUIRE(curveData.count(aDate) == 0,
                           "Quote " << *it << " provides a duplicate quote for the date " << io::iso_date(aDate));
                curveData[aDate] = q->quote()->value();
            }
        }
    }
    LOG("CommodityVolatilityCurve: read " << curveData.size() << " quotes.");

    if (found_regex) {
        QL_REQUIRE(curveData.size() > 0, "No quotes found matching " << config.quotes()[0]);
    } else {
        QL_REQUIRE(curveData.size() == config.quotes().size(), "Found " << curveData.size() << " quotes, but "
                                                                        << config.quotes().size()
                                                                        << " quotes given in config.");
    }

    // Create the dates and volatility vector
    vector<Date> dates;
    vector<Volatility> volatilities;
    for (const auto& datum : curveData) {
        dates.push_back(datum.first);
        volatilities.push_back(datum.second);
    }

    // Build the volatility structure curve
    DayCounter dayCounter = parseDayCounter(config.dayCounter());
    LOG("CommodityVolatilityCurve: Building BlackVarianceCurve term structure");
    volatility_ = boost::make_shared<BlackVarianceCurve>(asof, dates, volatilities, dayCounter);
}

void CommodityVolCurve::buildVolatilitySurface(const Date& asof, CommodityVolatilityCurveConfig& config,
                                               const Loader& loader) {

    // To hold dates x strike surface volatilities
    // dates will be sorted since map key
    map<Date, vector<Real>> surfaceData; // implicit order is assumed here for known stirkes and expiries from config.
    map<string, map<string, Real>> wc_mat; // same as "surfaceDate but in case of wild cards (unknows size => need to preserve more information)

    // Assume the config has required strike strings to be sorted by value
    const vector<string>& configStrikes = config.strikes();

    // wild cards?
    vector<string>::iterator expr_wc_it = find(config.expiries().begin(), config.expiries().end(), "*");
    vector<string>::iterator strk_wc_it = find(config.strikes().begin(), config.strikes().end(), "*");
    bool expr_wc = (expr_wc_it != config.expiries().end()) ? true : false;
    bool strk_wc = (strk_wc_it != config.strikes().end()) ? true : false;
    bool wild_card = (expr_wc || strk_wc) ? true : false;
    if (expr_wc) {
        QL_REQUIRE(config.expiries().size() == 1, "Wild card expiry specified but more expiries also specified.");
    }
    if (strk_wc) {
        QL_REQUIRE(config.strikes().size() == 1, "Wild card strike specified but more strikes also specified.");
    }

    // Loop over all market datums and find the quotes in the config
    // Return error if there are duplicate quotes (this is why we do not use loader.get() method)
    bool relevant_quote = false;
    bool strike_relevant = (strk_wc) ? true : false;
    bool expiry_relevant = (expr_wc) ? true : false;
    Size quotesAdded = 0;
    Calendar calendar = parseCalendar(config.calendar());
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION) {
            relevant_quote = false;
            boost::shared_ptr<CommodityOptionQuote> q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);

            // no wild cards
            if (!wild_card) {
                vector<string>::const_iterator it = find(config.quotes().begin(), config.quotes().end(), q->name());
                relevant_quote = (it != config.quotes().end()) ? true : false;

                if (relevant_quote) {
                    // The quote expiry is a string that may be a period or a date
                    // Convert it to a date here
                    Date aDate;
                    Period aPeriod;
                    bool isDate;
                    parseDateOrPeriod(q->expiry(), aDate, aPeriod, isDate);
                    if (isDate) {
                        QL_REQUIRE(aDate > asof, "Commodity volatility quote '" << q->name() << "' is for a past date ("
                                                                                << io::iso_date(aDate) << ")");
                    } else {
                        aDate = calendar.adjust(asof + aPeriod);
                    }

                    // Position of quote in vector
                    Size pos =
                        distance(configStrikes.begin(), find(configStrikes.begin(), configStrikes.end(), q->strike()));
                    QL_REQUIRE(pos < configStrikes.size(), "The quote '"
                                                               << q->name()
                                                               << "' is in the list of configured quotes "
                                                                  "but does not match any of the configured strikes");

                    // Add quote to surface
                    if (surfaceData.count(aDate) == 0)
                        surfaceData[aDate] = vector<Real>(configStrikes.size(), Null<Real>());
                    QL_REQUIRE(surfaceData[aDate][pos] == Null<Real>(),
                               "Quote " << q->name() << " provides a duplicate quote for the date "
                                        << io::iso_date(aDate) << " and the strike " << configStrikes[pos]);
                    surfaceData[aDate][pos] = q->quote()->value();
                    quotesAdded++;
                }
            // some wild card
            } else { 
                vector<string>::iterator i;
                if (!expr_wc) {
                    // strike only wild card
                    i = find(config.expiries().begin(), config.expiries().end(), q->expiry());
                    expiry_relevant = (i != config.expiries().end()) ? true : false;
                } else {
                    expiry_relevant = true;
                }
                if (!strk_wc) {
                    // expiry only wild card
                    i = find(config.strikes().begin(), config.strikes().end(), q->strike());
                    strike_relevant = (i != config.strikes().end()) ? true : false;
                } else {
                    // for now we ignore ATMF quotes
                    strike_relevant = (q->strike() != "ATMF") ? true : false;
                }
                relevant_quote = (strike_relevant && expiry_relevant) ? true : false;

                if (relevant_quote) {
                    // strike found?
                    if (wc_mat.find(q->strike()) != wc_mat.end()) {
                        // add expiry
                        QL_REQUIRE(wc_mat[q->strike()].find(q->expiry()) == wc_mat[q->strike()].end(),
                                   "quote " << q->name() << "duplicate"); 
                        wc_mat[q->strike()].insert(make_pair(q->expiry(), q->quote()->value()));
                    } else {
                        // add strike and expiry 
                        map<string, Real> tmp_inner = {{q->expiry(), q->quote()->value()}};
                        wc_mat.insert(pair<string, map<string, Real>>(q->strike(), tmp_inner));
                    }
                    quotesAdded++;
                }
            }
        }
    }

    LOG("CommodityVolatilityCurve: read " << quotesAdded << " quotes.");

    // check loaded quotes
    if (!wild_card) {
        QL_REQUIRE(config.quotes().size() == quotesAdded, "Found " << quotesAdded << " quotes, but "
                                                                   << config.quotes().size()
                                                                   << " quotes required by config.");
    } else {
        QL_REQUIRE(quotesAdded > 0, "No quotes loaded for" << config.curveID);
    }

    DayCounter dayCounter = parseDayCounter(config.dayCounter());
    if (!wild_card) {
        // Create the volatility matrix (rows are strikes, columns are dates)
        Matrix volatilities(configStrikes.size(), surfaceData.size());
        vector<Date> dates;
        dates.reserve(surfaceData.size());
        vector<Real> strikes;
        strikes.reserve(configStrikes.size());
        Size counter = 0;
        for (const auto& datum : surfaceData) {
            dates.push_back(datum.first);
            for (Size i = 0; i < configStrikes.size(); i++) {
                if (counter == 0)
                    strikes.push_back(parseReal(configStrikes[i]));
                volatilities[i][counter] = datum.second[i];
            }
            counter++;
        }
        // Build the volatility surface
        BlackVarianceSurface::Extrapolation lowerStrikeExtrap =
            config.lowerStrikeConstantExtrapolation()
                ? BlackVarianceSurface::Extrapolation::ConstantExtrapolation
                : BlackVarianceSurface::Extrapolation::InterpolatorDefaultExtrapolation;
        BlackVarianceSurface::Extrapolation upperStrikeExtrap =
            config.upperStrikeConstantExtrapolation()
                ? BlackVarianceSurface::Extrapolation::ConstantExtrapolation
                : BlackVarianceSurface::Extrapolation::InterpolatorDefaultExtrapolation;

        LOG("CommodityVolatilityCurve: Building BlackVarianceSurface term structure");
        volatility_ = boost::make_shared<BlackVarianceSurface>(asof, calendar, dates, strikes, volatilities, dayCounter,
                                                               lowerStrikeExtrap, upperStrikeExtrap);
    } else {

        // Create matrix
        int wc_mat_rows = wc_mat.size();
        int wc_mat_cols;
        vector<string> tmpLenVect;
        for (map<string, map<string, Real>>::iterator itr = wc_mat.begin(); itr != wc_mat.end(); itr++) {
            for (map<string, Real>::iterator itr2 = itr->second.begin(); itr2 != itr->second.end(); itr2++) {
                vector<string>::iterator foundItr;
                foundItr = find(tmpLenVect.begin(), tmpLenVect.end(), itr2->first);
                if (foundItr == tmpLenVect.end()) {
                    tmpLenVect.push_back(itr2->first); // if not found add
                }
            }
        }
        wc_mat_cols = tmpLenVect.size();
        Matrix sparse_vols = Matrix(wc_mat_rows, wc_mat_cols, -1.0);

        // populate sparse_vols matrix with contents of wc_mat map. (make sure strikes and dates are ordered first!)
        vector<Date> oexpiries = PopulateMatrixFromMap(
            sparse_vols, wc_mat, asof); // Builds matrix and returns expiries (NOTE: this also takes care of dates vs periods)
        QuantExt::fillIncompleteMatrix(sparse_vols, true, -1.0);
        Matrix final_vols = sparse_vols;    // used in blacksurface
        map<string, map<string, Real>>::iterator itr;
        vector<Real> ostrikes;
        for (itr = wc_mat.begin(); itr != wc_mat.end(); itr++) {
            ostrikes.push_back(parseReal(itr->first)); // not ordered but the strks vect in PopulateMatrixFromMap
                                                       // is. Maybe return this instead. or mimic order logic
        }
        sort(ostrikes.begin(), ostrikes.end()); //
        

        // set up surface
        vector<Date> dates(final_vols.columns());
        dates = oexpiries;
        LOG("CommodityVolCurve: Building BlackVarianceSurface");

        Calendar cal = NullCalendar(); // why do we need this?

        // This can get wrapped in a QuantExt::BlackVolatilityWithATM later on
        volatility_ = boost::make_shared<BlackVarianceSurface>(asof, cal, dates, ostrikes, final_vols, dayCounter);
    }
  
    
}

vector<Date> CommodityVolCurve::PopulateMatrixFromMap(Matrix& mt, map<string, map<string, Real>>& mp, Date asf) {
    bool r_check, c_check;
    map<string, map<string, Real>>::iterator outItr;
    map<string, Real>::iterator inItr;
    vector<string> tmpLenVect;
    for (outItr = mp.begin(); outItr != mp.end(); outItr++) {
        for (inItr = outItr->second.begin(); inItr != outItr->second.end(); inItr++) {
            vector<string>::iterator foundItr;
            foundItr = find(tmpLenVect.begin(), tmpLenVect.end(), inItr->first);
            if (foundItr == tmpLenVect.end()) {
                tmpLenVect.push_back(inItr->first); // if not found add
            }
        }
    }
    r_check = (mt.rows() == mp.size()) ? true : false;            // input matrix rows match map size
    c_check = (mt.columns() == tmpLenVect.size()) ? true : false; // input matrix cols match map longest entry
    QL_REQUIRE((r_check && c_check), "Matrix and map incompatible sizes.");

    vector<Date> exprs;
    vector<Real> strks;

    // get unique strikes and expiries in map
    for (outItr = mp.begin(); outItr != mp.end(); outItr++) {
        // strikes
        if (outItr->first == "ATMF" && mp.size() == 1) {
            strks.push_back(9999); // make sure this worked. (no duplicates)
        } else {
            strks.push_back(stof(outItr->first)); // make sure this worked. (no duplicates)
        }

        // expiries
        for (inItr = outItr->second.begin(); inItr != outItr->second.end(); inItr++) {
            Date tmpDate;
            Period tmpPer;
            bool tmpIsDate;
            parseDateOrPeriod(inItr->first, tmpDate, tmpPer, tmpIsDate);
            if (!tmpIsDate)
                tmpDate = WeekendsOnly().adjust(asf + tmpPer);
            QL_REQUIRE(tmpDate > asf,
                       "Equity Vol Curve cannot contain a vol quote for a past date (" << io::iso_date(tmpDate) << ")");
            // if not found, add
            vector<Date>::iterator found = find(exprs.begin(), exprs.end(), tmpDate);
            if (found == exprs.end()) {
                exprs.push_back(tmpDate);
            }
        }
    }

    // sort
    sort(strks.begin(), strks.end());
    sort(exprs.begin(), exprs.end());

    // populate matrix
    int rw, cl;
    vector<Real>::iterator rfind;
    vector<Date>::iterator cfind;
    for (outItr = mp.begin(); outItr != mp.end(); outItr++) {
        // which row
        if (outItr->first == "ATMF" && mp.size() == 1) {
            rw = 0;
        } else {
            rfind = find(strks.begin(), strks.end(), stof(outItr->first));
            rw = rfind - strks.begin();
        }

        // which column
        for (inItr = outItr->second.begin(); inItr != outItr->second.end(); inItr++) {
            // date/period string to date obj
            Date tmpDate;
            Period tmpPer;
            bool tmpIsDate;
            parseDateOrPeriod(inItr->first, tmpDate, tmpPer, tmpIsDate);
            if (!tmpIsDate)
                tmpDate = WeekendsOnly().adjust(asf + tmpPer);

            cfind = find(exprs.begin(), exprs.end(), tmpDate);
            cl = cfind - exprs.begin();

            mt[rw][cl] = inItr->second;
        }
    }

    // return
    return exprs;
}


} // namespace data
} // namespace ore
