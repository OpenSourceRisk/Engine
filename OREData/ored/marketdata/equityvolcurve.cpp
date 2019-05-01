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
#include <qle/math/fillemptymatrix.hpp>

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
        vector<string> strikes({"ATMF"});
        if (isSurface)
            strikes = config->strikes();
        QL_REQUIRE(expiries.size() > 0, "No expiries defined");
        QL_REQUIRE(strikes.size() > 0, "No strikes defined");

        // check for wild cards
        vector<string>::iterator exprWcIt = find(expiries.begin(), expiries.end(), "*");
        vector<string>::iterator strkWcIt = find(strikes.begin(), strikes.end(), "*");
        bool strikesWc = false;
        bool expiriesWc = false;
        if (exprWcIt != expiries.end()) {
            QL_REQUIRE(expiries.size() == 1, "Wild card expiriy specified but more expiries also specified.")
            expiriesWc = true;
        }
        if (strkWcIt != strikes.end()) {
            QL_REQUIRE(strikes.size() == 1, "Wild card strike specified but more strikes also specified.")
            strikesWc = true;
        }
        bool noWildCard = (!strikesWc && !expiriesWc) ? true : false;

        // We store them all in a matrix, we start with all values negative and use
        // this to check if they have been set.
        Matrix dVols, sparseVols, finalVols;
        vector<vector<map<string, Real>>> wcM;
        map<string, map<string, Real>> wcMat;
        if (noWildCard) {
            dVols = Matrix(strikes.size(), expiries.size(), -1.0);
        }

        // In case of wild card we need the following granularity within the mkt data loop
        bool strikeRelevant = (strikesWc) ? true : false;
        bool expiryRelevant = (expiriesWc) ? true : false;
        bool quoteRelevant;

        // We loop over all market data, looking for quotes that match the configuration
        Size quotesAdded = 0;
        for (auto& md : loader.loadQuotes(asof)) {
            // skip irrelevant data
            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::EQUITY_OPTION) {
                boost::shared_ptr<EquityOptionQuote> q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);

                if (q->eqName() == spec.curveConfigID() && q->ccy() == spec.ccy()) {
                    // no wild cards
                    if (noWildCard) {
                        Size i = std::find(strikes.begin(), strikes.end(), q->strike()) - strikes.begin();
                        Size j = std::find(expiries.begin(), expiries.end(), q->expiry()) - expiries.begin();

                        if (i < strikes.size() && j < expiries.size()) {
                            QL_REQUIRE(dVols[i][j] < 0, "Error vol (" << spec << ") for " << strikes[i] << ", "
                                                                       << expiries[j] << " already set");
                            dVols[i][j] = q->quote()->value();
                            quotesAdded++;
                        }
                    }
                    // some wild card
                    else {
                        if (!expiriesWc) {
                            vector<string>::iterator j = std::find(expiries.begin(), expiries.end(), q->expiry());
                            expiryRelevant = (j != expiries.end()) ? true : false;
                        }
                        if (!strikesWc) {
                            vector<string>::iterator i = std::find(strikes.begin(), strikes.end(), q->strike());
                            strikeRelevant = (i != strikes.end()) ? true : false;
                        } else {
                            // todo - for now we will ignore ATMF quotes in case of strike wild card. ----
                            strikeRelevant = (q->strike() != "ATMF") ? true : false;
                        }
                        quoteRelevant = (strikeRelevant && expiryRelevant) ? true : false;

                        // add quote to matrix map, if relevant
                        if (quoteRelevant) {
                            // strike found?
                            if (wcMat.find(q->strike()) != wcMat.end()) {
                                // add expiry
                                QL_REQUIRE(wcMat[q->strike()].find(q->expiry()) == wcMat[q->strike()].end(),
                                           "quote" << q->name() << "duplicate");
                                wcMat[q->strike()].insert(make_pair(q->expiry(), q->quote()->value()));
                            } else {
                                // add strike and expiry
                                map<string, Real> tmpInner;
                                tmpInner.insert(pair<string, Real>(q->expiry(), q->quote()->value()));
                                wcMat.insert(pair<string, map<string, Real>>(q->strike(), tmpInner));
                            }
                            quotesAdded++;
                        }
                    }
                }
            }
        }
        LOG("EquityVolatilityCurve: read " << quotesAdded << " quotes.")

        // Check loaded quotes + build vol matrix
        pair<vector<Real>, vector<Date>> sAndE;
        if (noWildCard) {
            for (Size i = 0; i < strikes.size(); i++) {
                for (Size j = 0; j < expiries.size(); j++) {
                    QL_REQUIRE(dVols[i][j] >= 0,
                               "Error vol (" << spec << ") for " << strikes[i] << ", " << expiries[j] << " not set");
                }
            }
            finalVols = dVols;
        } else {
            // no wild card for strikes
            if (!strikesWc) {
                QL_REQUIRE(wcMat.size() == strikes.size(), "Error vol (" << spec << ") all required strikes not set");
            }
            // no wild card for expiries
            if (!expiriesWc) {
                map<string, map<string, Real>>::iterator tmpItr;
                for (tmpItr = wcMat.begin(); tmpItr != wcMat.end(); tmpItr++) {
                    QL_REQUIRE(tmpItr->second.size() == expiries.size(),
                               "Error vol (" << spec << ") all required expiries not set");
                }
            }

            // Create matrix
            int wcMatRows = wcMat.size();
            int wcMatCols;
            set<string> tmpLenSet;
            for (map<string, map<string, Real>>::iterator itr = wcMat.begin(); itr != wcMat.end(); itr++) {
                for (map<string, Real>::iterator itr2 = itr->second.begin(); itr2 != itr->second.end(); itr2++) {
                    tmpLenSet.insert(itr2->first); // if not found add
                }
            }
            wcMatCols = tmpLenSet.size();
            sparseVols = Matrix(wcMatRows, wcMatCols, -1.0);

            // populate sparse_vols matrix with contents of wc_mat map.
            sAndE = populateMatrixFromMap(
                sparseVols, wcMat,
                asof); // Builds matrix and returns expiries (NOTE: this also takes care of dates vs periods)
            QuantExt::fillIncompleteMatrix(sparseVols, true, -1.0);
            finalVols = sparseVols;
        }

        // set up vols
        if (finalVols.rows() == 1 && finalVols.columns() == 1) {
            LOG("EquityVolCurve: Building BlackConstantVol");
            vol_ =
                boost::shared_ptr<BlackVolTermStructure>(new BlackConstantVol(asof, Calendar(), finalVols[0][0], dc));
        } else {
            if (noWildCard) {
                vector<Date> dates(finalVols.columns());
                vector<Real> strikesReal;
                // expiries
                for (Size i = 0; i < finalVols.columns(); i++) {
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

                // strikes + surface
                if (!isSurface) {
                    LOG("EquityVolCurve: Building BlackVarianceCurve");
                    QL_REQUIRE(finalVols.rows() == 1, "Matrix error, should only have 1 row (ATMF)");
                    vector<Volatility> atmVols(finalVols.begin(),
                                               finalVols.end()); 
                    vol_ = boost::make_shared<BlackVarianceCurve>(asof, dates, atmVols, dc);
                } else {
                    LOG("EquityVolCurve: Building BlackVarianceSurface");
                    // convert strike strings to Reals
                    for (string k : strikes)
                        strikesReal.push_back(parseReal(k));

                    Calendar cal = NullCalendar(); // why do we need this?

                    // This can get wrapped in a QuantExt::BlackVolatilityWithATM later on
                    vol_ = boost::make_shared<BlackVarianceSurface>(asof, cal, dates, strikesReal, finalVols, dc);
                }

            // some wild card
            } else {
                if (!isSurface) {
                    LOG("EquityVolCurve: Building BlackVarianceCurve");
                    QL_REQUIRE(finalVols.rows() == 1, "Matrix error, should only have 1 row (ATMF)");
                    vector<Volatility> atmVols(finalVols.begin(), finalVols.end()); 
                    vol_ = boost::make_shared<BlackVarianceCurve>(asof, get<1>(sAndE), atmVols, dc);
                } else {
                    LOG("EquityVolCurve: Building BlackVarianceSurface");

                    Calendar cal = NullCalendar(); // why do we need this?

                    // This can get wrapped in a QuantExt::BlackVolatilityWithATM later on
                    vol_ = boost::make_shared<BlackVarianceSurface>(asof, cal, get<1>(sAndE), get<0>(sAndE),
                                                                    finalVols, dc);
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

pair<vector<Real>, vector<Date>> EquityVolCurve::populateMatrixFromMap(Matrix& mt, map<string, map<string, Real>>& mp,
                                                                       Date asf) {
    map<string, map<string, Real>>::iterator outItr;
    map<string, Real>::iterator inItr;
    bool isATM = false;
    set<Date> exprsSet;
    set<Real> strksSet;
    // Get and sort unique strikes and expiries in map. To create matrix with ordered cols and rows.
    for (outItr = mp.begin(); outItr != mp.end(); outItr++) {
        // strikes
        if (outItr->first == "ATMF" && mp.size() == 1) {
            strksSet.insert(9999);
            isATM = true;
        } else {
            strksSet.insert(parseReal(outItr->first));
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
            exprsSet.insert(tmpDate);
        }
    }
    QL_REQUIRE(mt.rows() == mp.size(), "Matrix and map incompatible number of rows");           // input matrix rows match map size
    QL_REQUIRE(mt.columns() == exprsSet.size(), "Matrix and map incompatible number of  columns"); // input matrix cols match map unique expires

    // sort
    vector<Real> strksVect(strksSet.begin(), strksSet.end());
    vector<Date> exprsVect(exprsSet.begin(), exprsSet.end());
    sort(strksVect.begin(), strksVect.end());
    sort(exprsVect.begin(), exprsVect.end());

    // populate matrix
    ptrdiff_t rw;
    ptrdiff_t cl;
    vector<Real>::iterator rfind;
    vector<Date>::iterator cfind;
    for (outItr = mp.begin(); outItr != mp.end(); outItr++) {
        // which row
        if (isATM) {
            rw = 0;
        } else {
            Real a = parseReal(outItr->first);
            rfind = find_if(strksVect.begin(), strksVect.end(), [a](Real b) { return close(a, b, QL_EPSILON); });
            rw = distance(strksVect.begin(), rfind);
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

            cfind = find(exprsVect.begin(), exprsVect.end(), tmpDate);
            cl = distance(exprsVect.begin(), cfind);

            mt[rw][cl] = inItr->second;
        }
    }

    // return
    return std::make_pair(strksVect, exprsVect);
}
} // namespace data
} // namespace ore
