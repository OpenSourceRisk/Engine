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
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <regex>

using namespace QuantLib;
using namespace std;

namespace ore {
    namespace data {

        EquityVolCurve::EquityVolCurve(Date asof, EquityVolatilityCurveSpec spec, const Loader& loader,
            const CurveConfigurations& curveConfigs) {

            try {
                const boost::shared_ptr<EquityVolatilityCurveConfig>& config =
                    curveConfigs.equityVolCurveConfig(spec.curveConfigID());
                QL_REQUIRE(config->dimension() == EquityVolatilityCurveConfig::Dimension::ATM ||
                    config->dimension() == EquityVolatilityCurveConfig::Dimension::Smile,
                    "Unkown Equity curve building dimension");

                // daycounter used for interpolation in time.
                DayCounter dc = config->dayCounter();

                bool isSurface = config->dimension() == EquityVolatilityCurveConfig::Dimension::Smile;

                vector<string> expiries = config->expiries();
                vector<string> strikes({ "ATMF" });
                if (isSurface)
                    strikes = config->strikes();
                QL_REQUIRE(expiries.size() > 0, "No expiries defined");
                QL_REQUIRE(strikes.size() > 0, "No strikes defined");

                // check for wild cards
                bool strikes_wc = false;
                bool expiries_wc = false;
                vector<Real> ostrikes;
                vector<Date> oexpiries;
                if (strikes.size() == 1 && strikes[0] == "*") {
                    strikes_wc = true;
                }
                if (expiries.size() == 1 && expiries[0] == "*") {
                    expiries_wc = true;
                }
                bool no_wild_card = (!strikes_wc && !expiries_wc) ? true : false;

                // We store them all in a matrix, we start with all values negative and use
                // this to check if they have been set.
                Matrix d_vols, sparse_vols, final_vols;
                vector<vector<map<string, Real>>> wc_m;
                map<string, map<string, Real>> wc_mat;
                if (no_wild_card) {
                    d_vols = Matrix(strikes.size(), expiries.size(), -1.0);
                }

                // We loop over all market data, looking for quotes that match the configuration
                bool strike_relevant, expiry_relevant;
                strike_relevant, expiry_relevant = true;
                for (auto& md : loader.loadQuotes(asof)) {
                    // skip irrelevant data
                    if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::EQUITY_OPTION) {
                        boost::shared_ptr<EquityOptionQuote> q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);

                        if (q->eqName() == spec.curveConfigID() && q->ccy() == spec.ccy()) {
                            // no wild cards
                            if (no_wild_card) {
                                Size i = std::find(strikes.begin(), strikes.end(), q->strike()) - strikes.begin();
                                Size j = std::find(expiries.begin(), expiries.end(), q->expiry()) - expiries.begin();

                                if (i < strikes.size() && j < expiries.size()) {
                                    QL_REQUIRE(d_vols[i][j] < 0, "Error vol (" << spec << ") for " << strikes[i] << ", "
                                        << expiries[j] << " already set");
                                    d_vols[i][j] = q->quote()->value();
                                }
                            }
                            // some wild card
                            else {
                                if (strikes_wc && !expiries_wc) {
                                    // all strikes are valid
                                    strike_relevant = true;
                                    vector<string>::iterator j = std::find(expiries.begin(), expiries.end(), q->expiry());
                                    expiry_relevant = (j != expiries.end()) ? true : false;
                                } else if (!strikes_wc && expiries_wc) {
                                    // all expiries are valid
                                    expiry_relevant = true;
                                    vector<string>::iterator i = std::find(strikes.begin(), strikes.end(), q->strike());
                                    strike_relevant = (i != strikes.end()) ? true : false;
                                }

                                // strike found?
                                if (wc_mat.find(q->strike()) != wc_mat.end()) {
                                    // add expiry if requested and not already there
                                    if (expiry_relevant) {
                                        if (wc_mat[q->strike()].find(q->expiry()) == wc_mat[q->strike()].end()) {
                                            wc_mat[q->strike()].insert(make_pair(q->expiry(), q->quote()->value()));
                                        }
                                    }
                                }
                                else if (strike_relevant){
                                    //add strike and expiry if relevant
                                    map<string, Real> tmp_inner;
                                    if (expiry_relevant) {
                                        tmp_inner.insert(pair<string, Real>(q->expiry(), q->quote()->value()));
                                    }
                                    wc_mat.insert(pair<string, map<string, Real>>(q->strike(), tmp_inner));
                                }
                            }
                        }
                    }
                }

                // No wild card - Check that we have all the quotes in config in case of 
                if (no_wild_card) {
                    for (Size i = 0; i < strikes.size(); i++) {
                        for (Size j = 0; j < expiries.size(); j++) {
                        QL_REQUIRE(d_vols[i][j] >= 0,
                            "Error vol (" << spec << ") for " << strikes[i] << ", " << expiries[j] << " not set");
                        }
                    }
                    final_vols = d_vols;
                }
                //wild card
                else {
                    // no wild card for strikes
                    if (!strikes_wc) {
                        QL_REQUIRE(wc_mat.size() == strikes.size(), "Error vol (" << spec << ") all required strikes not set");
                    }
                    // no wild card for expiries
                    if (!expiries_wc) {
                        map<string, map<string, Real>>::iterator tmpItr;
                        for (tmpItr = wc_mat.begin(); tmpItr != wc_mat.end(); tmpItr++) {
                            QL_REQUIRE(tmpItr->second.size() == expiries.size(),
                                       "Error vol (" << spec << ") all required expiries not set");
                        }
                    }

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
                    sparse_vols = Matrix(wc_mat_rows, wc_mat_cols, -1.0);
                    
                    // populate sparse_vols matrix with contents of wc_mat map. (make sure strikes and dates are ordered first!)
                    oexpiries = PopulateMatrixFromMap(sparse_vols, wc_mat, asof); // Builds matrix and returns expiries (NOTE: this also takes care of dates vs periods)
                    ore::data::FillIncompleteMatrix(sparse_vols); // still need to define this function. Make sure this is the same size as the sparse vols. 
                    final_vols = sparse_vols;
                    map<string, map<string, Real>>::iterator itr;

                    for (itr = wc_mat.begin(); itr != wc_mat.end();itr++) {
                        ostrikes.push_back(parseReal(itr->first)); // not ordered but the strks vect in PopulateMatrixFromMap is. Maybe return this instead. or mimic order logic
                    }
                    sort(ostrikes.begin(), ostrikes.end()); // 
                }
                    
                if (final_vols.rows() == 1 && final_vols.columns() == 1) {
                    LOG("EquityVolCurve: Building BlackConstantVol");
                    vol_ = boost::shared_ptr<BlackVolTermStructure>(new BlackConstantVol(asof, Calendar(), final_vols[0][0], dc));
                } else {
                    vector<Date> dates(final_vols.columns());
                    vector<Real> strikesReal;

                    // expiries
                    if (!strikes_wc && !expiries_wc) {
                        for (Size i = 0; i < final_vols.columns(); i++) {
                            Date tmpDate;
                            Period tmpPer;
                            bool tmpIsDate;
                            parseDateOrPeriod(expiries[i], tmpDate, tmpPer, tmpIsDate);
                            if (!tmpIsDate)
                                tmpDate = WeekendsOnly().adjust(asof + tmpPer); // todo the QL require below should be implemented for the wc case as well (no need will always be the case)
                            QL_REQUIRE(tmpDate > asof, "Equity Vol Curve cannot contain a vol quote for a past date ("
                                                           << io::iso_date(tmpDate) << ")");
                            dates[i] = tmpDate;
                        }

                    } else {
                        dates = oexpiries;
                    }
                    
                    // strikes
                    if (!strikes_wc && !expiries_wc) {
                        if (!isSurface) {
                            LOG("EquityVolCurve: Building BlackVarianceCurve");
                            QL_REQUIRE(final_vols.rows() == 1, "Matrix error, should only have 1 row (ATMF)");
                            vector<Volatility> atmVols(final_vols.begin(), final_vols.end());
                            vol_ = boost::make_shared<BlackVarianceCurve>(asof, dates, atmVols, dc);
                        } else {
                            LOG("EquityVolCurve: Building BlackVarianceSurface");
                            // convert strike strings to Reals
                            for (string k : strikes)
                                strikesReal.push_back(parseReal(k));

                            Calendar cal = NullCalendar(); // why do we need this?

                            // This can get wrapped in a QuantExt::BlackVolatilityWithATM later on
                            vol_ = boost::make_shared<BlackVarianceSurface>(asof, cal, dates, strikesReal, final_vols, dc);
                        }
                    
                    } else {
                            if (!isSurface) {
                            LOG("EquityVolCurve: Building BlackVarianceCurve");
                            QL_REQUIRE(final_vols.rows() == 1, "Matrix error, should only have 1 row (ATMF)");
                            vector<Volatility> atmVols(final_vols.begin(), final_vols.end());
                            vol_ = boost::make_shared<BlackVarianceCurve>(asof, dates, atmVols, dc);
                        } else {
                            LOG("EquityVolCurve: Building BlackVarianceSurface");

                            Calendar cal = NullCalendar(); // why do we need this?

                            // This can get wrapped in a QuantExt::BlackVolatilityWithATM later on
                            vol_ = boost::make_shared<BlackVarianceSurface>(asof, cal, dates, ostrikes, final_vols, dc);
                        }
                    }
                }
                vol_->enableExtrapolation();
            } catch (std::exception& e) {
                QL_FAIL("equity vol curve building failed :" << e.what());
            } catch (...) {
                QL_FAIL("equity vol curve building failed: unknown error");
            }
        }


        vector<Date> EquityVolCurve::PopulateMatrixFromMap(Matrix& mt, map<string, map<string, Real>>& mp, Date asf) { 
            bool r_check, c_check;
            vector<string> tmpLenVect;
            for (map<string, map<string, Real>>::iterator itr = mp.begin(); itr != mp.end(); itr++) {
                for (map<string, Real>::iterator itr2 = itr->second.begin(); itr2 != itr->second.end(); itr2++) {
                    vector<string>::iterator foundItr;
                    foundItr = find(tmpLenVect.begin(), tmpLenVect.end(), itr2->first);
                    if (foundItr == tmpLenVect.end()) {
                        tmpLenVect.push_back(itr2->first); // if not found add
                    }
                }
            }
            r_check = (mt.rows() == mp.size()) ? true : false;              // input matrix rows match map size
            c_check = (mt.columns() == tmpLenVect.size()) ? true : false;   // input matrix cols match map longest entry
            QL_REQUIRE((r_check && c_check), "Matrix and map incompatible sizes.");
        
            vector<Date> exprs;
            vector<Real> strks;
        
            map<string, map<string, Real>>::iterator outItr;
            map<string, Real>::iterator inItr;
        
            for (outItr = mp.begin(); outItr != mp.end(); outItr++) {
                // strikes
                strks.push_back(stof(outItr->first)); // make sure this worked. (no duplicates)
        
                //expiries
                for (inItr = outItr->second.begin(); inItr != outItr->second.end(); inItr++) {
                    Date tmpDate;
                    Period tmpPer;
                    bool tmpIsDate;
                    parseDateOrPeriod(inItr->first, tmpDate, tmpPer, tmpIsDate);
                    if (!tmpIsDate)
                        tmpDate = WeekendsOnly().adjust(asf + tmpPer);
                    QL_REQUIRE(tmpDate > asf,
                               "Equity Vol Curve cannot contain a vol quote for a past date (" << io::iso_date(tmpDate) << ")");
                    //if not found, add
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
                rfind = find(strks.begin(), strks.end(), stof(outItr->first));
                rw = rfind - strks.begin();
        
                //which column
                for (inItr = outItr->second.begin(); inItr != outItr->second.end(); inItr++) {
                    cfind = find(exprs.begin(), exprs.end(), inItr->first);
                    cl = cfind - exprs.begin();
                }
        
                mt[rw][cl] = inItr->second;
            }
        
        
            // return
            return exprs;
            
        }


} // namespace data
} // namespace ore
