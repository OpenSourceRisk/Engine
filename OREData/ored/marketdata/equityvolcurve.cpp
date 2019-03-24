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
                if (strikes.size() == 1 && strikes[0] == "*") {
                    strikes_wc = true;
                }
                if (expiries.size() == 1 && expiries[0] == "*") {
                    expiries_wc = true;
                }

                // We store them all in a matrix, we start with all values negative and use
                // this to check if they have been set.
                Matrix vols, sparse_vols, vols_filled, final_vols;
                vector<vector<map<string, Real>>> wc_m;
                map<string, map<string, Real>> wc_mat;
                if (!strikes_wc && !expiries_wc) {
                    vols = Matrix(strikes.size(), expiries.size(), -1.0);
                }

                // We loop over all market data, looking for quotes that match the configuration
                for (auto& md : loader.loadQuotes(asof)) {
                    // skip irrelevant data
                    if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::EQUITY_OPTION) {
                        boost::shared_ptr<EquityOptionQuote> q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);

                        if (q->eqName() == spec.curveConfigID() && q->ccy() == spec.ccy()) {
                            if (!strikes_wc && !expiries_wc) {
                                //direct quotes only
                                Size i = std::find(strikes.begin(), strikes.end(), q->strike()) - strikes.begin();
                                Size j = std::find(expiries.begin(), expiries.end(), q->expiry()) - expiries.begin();

                                if (i < strikes.size() && j < expiries.size()) {
                                    QL_REQUIRE(vols[i][j] < 0, "Error vol (" << spec << ") for " << strikes[i] << ", "
                                        << expiries[j] << " already set");
                                    vols[i][j] = q->quote()->value();
                                }
                            }
                            else if(strikes_wc && expiries_wc) {
                                bool found = false;

                                // look for strike
                                if (wc_mat.find(q->strike()) != wc_mat.end()) {
                                    if (wc_mat[q->strike()].find(q->expiry()) == wc_mat[q->strike()].end()){
                                        //add value for expiry
                                        wc_mat[q->strike()].insert(make_pair(q->expiry(), q->quote()->value()));
                                        auto kk = wc_mat.find("dd");
                                    }
                                }
                                else {
                                    //add strike and expiry
                                    map<string, Real> tmp_inner;
                                    tmp_inner.insert(pair<string, Real>(q->expiry(), q->quote()->value()));
                                    wc_mat.insert(pair<string, map<string, Real>>(q->strike(), tmp_inner));

                                }
                            }
                            else {
                                //add cases of one wild card one not. Check in wild card cases the expiries are after (dates)
                            }
                        }
                    }
                }

        // No wild card - Check that we have all the quotes in config in case of 
        if (!strikes_wc && !expiries_wc) {
            for (Size i = 0; i < strikes.size(); i++) {
                for (Size j = 0; j < expiries.size(); j++) {
                QL_REQUIRE(vols[i][j] >= 0,
                    "Error vol (" << spec << ") for " << strikes[i] << ", " << expiries[j] << " not set");
                }
            }
            final_vols = vols;
        }
        //wild card
        else {
            // Create matrix (make sure map.size retunrs number of elements)
            int wc_mat_rows = wc_mat.size();
            int wc_mat_cols = 0;

            for (map<string, map<string, Real>>::iterator itr = wc_mat.begin(); itr != wc_mat.end(); itr++) {
                wc_mat_cols = (itr->second.size() > wc_mat_cols) ? itr->second.size() : wc_mat_cols;
            }
            sparse_vols = Matrix(wc_mat_rows, wc_mat_cols, -1.0);
            
            // populate sparse_vols matrix with contents of wc_mat map. (make sure strikes and dates are ordered first!)
            ore::data::PopulateMatrixFromMap(sparse_vols, wc_mat); // still need to define. Pass by reference!
            final_vols = ore::data::FillSparseMatrix(sparse_vols); // still need to define this function
        }
            
        // take from here.... now we are doing all our checks on final_vols!! Make sure dates vs expiries work

        if (final_vols.rows() == 1 && final_vols.columns() == 1) {
            LOG("EquityVolCurve: Building BlackConstantVol");
            vol_ = boost::shared_ptr<BlackVolTermStructure>(new BlackConstantVol(asof, Calendar(), vols[0][0], dc));
        } else {

            // get dates
            vector<Date> dates(expiries.size());
            for (Size i = 0; i < expiries.size(); i++) {
                Date tmpDate;
                Period tmpPer;
                bool tmpIsDate;
                parseDateOrPeriod(expiries[i], tmpDate, tmpPer, tmpIsDate);
                if (!tmpIsDate)
                    tmpDate = WeekendsOnly().adjust(asof + tmpPer);
                QL_REQUIRE(tmpDate > asof, "Equity Vol Curve cannot contain a vol quote for a past date ("
                                               << io::iso_date(tmpDate) << ")");
                dates[i] = tmpDate;
            }

            if (!isSurface) {
                LOG("EquityVolCurve: Building BlackVarianceCurve");
                QL_REQUIRE(vols.rows() == 1, "Matrix error, should only have 1 row (ATMF)");
                vector<Volatility> atmVols(vols.begin(), vols.end());
                vol_ = boost::make_shared<BlackVarianceCurve>(asof, dates, atmVols, dc);
            } else {
                LOG("EquityVolCurve: Building BlackVarianceSurface");
                // convert strike strings to Reals
                vector<Real> strikesReal;
                for (string k : strikes)
                    strikesReal.push_back(parseReal(k));

                Calendar cal = NullCalendar(); // why do we need this?

                // This can get wrapped in a QuantExt::BlackVolatilityWithATM later on
                vol_ = boost::make_shared<BlackVarianceSurface>(asof, cal, dates, strikesReal, vols, dc);
            }
        }
        vol_->enableExtrapolation();
    } catch (std::exception& e) {
        QL_FAIL("equity vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("equity vol curve building failed: unknown error");
    }
}
} // namespace data
} // namespace ore
