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
#include <qle/termstructures/blackvariancesurfacesparse.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;
using namespace QuantExt;
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
        bool noWildCard = !strikesWc && !expiriesWc;

        Matrix dVols;               // no wild card case
        vector<Real> sStrikes;      // wild card case
        vector<Volatility> sVols;   // wild card case 
        vector<Date> sExpiries;     // wild card case
        if (noWildCard) {
            dVols = Matrix(strikes.size(), expiries.size(), -1.0);
        }

        // In case of wild card we need the following granularity within the mkt data loop
        bool strikeRelevant = strikesWc;
        bool expiryRelevant = expiriesWc;
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
                            expiryRelevant = j != expiries.end();
                        }
                        if (!strikesWc) {
                            vector<string>::iterator i = std::find(strikes.begin(), strikes.end(), q->strike());
                            strikeRelevant = i != strikes.end();
                        } else {
                            // todo - for now we will ignore ATMF quotes in case of strike wild card. ----
                            strikeRelevant = q->strike() != "ATMF";
                        }
                        quoteRelevant = strikeRelevant && expiryRelevant;

                        // add quote to vectors, if relevant
                        if (quoteRelevant) {
                            sStrikes.push_back(parseReal(q->strike()));
                            sVols.push_back(q->quote()->value());
                            Date tmpDate;
                            Period tmpPer;
                            bool tmpIsDate;
                            parseDateOrPeriod(q->expiry(), tmpDate, tmpPer, tmpIsDate);
                            if (!tmpIsDate)
                                tmpDate = WeekendsOnly().adjust(asof + tmpPer);
                            QL_REQUIRE(tmpDate > asof, "Vol quote for a past date ("
                                                           << io::iso_date(tmpDate) << ")");
                            sExpiries.push_back(tmpDate);

                            quotesAdded++;
                        }
                    }
                }
            }
        }
        LOG("EquityVolatilityCurve: read " << quotesAdded << " quotes.")

        // Check loaded quotes
        if (noWildCard) {
            for (Size i = 0; i < strikes.size(); i++) {
                for (Size j = 0; j < expiries.size(); j++) {
                    QL_REQUIRE(dVols[i][j] >= 0,
                               "Error vol (" << spec << ") for " << strikes[i] << ", " << expiries[j] << " not set");
                }
            }

        } else {
            QL_REQUIRE(sStrikes.size() == sVols.size() && sVols.size() == sExpiries.size(),
                       "Quotes loaded don't produces strike,vol,expiry vectors of equal length.");
        }

        // set up vols
        // no wild cards
        if (noWildCard) {
            if (dVols.rows() == 1 && dVols.columns() == 1){
                LOG("EquityVolCurve: Building BlackConstantVol");
                vol_ = boost::shared_ptr<BlackVolTermStructure>(
                    new BlackConstantVol(asof, Calendar(), dVols[0][0], dc));
            } else {
                vector<Date> dates(dVols.columns());
                vector<Real> strikesReal;
                // expiries
                for (Size i = 0; i < dVols.columns(); i++) {
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
                    QL_REQUIRE(dVols.rows() == 1, "Matrix error, should only have 1 row (ATMF)");
                    vector<Volatility> atmVols(dVols.begin(), dVols.end());
                    vol_ = boost::make_shared<BlackVarianceCurve>(asof, dates, atmVols, dc);
                } else {
                    LOG("EquityVolCurve: Building BlackVarianceSurface");
                    // convert strike strings to Reals
                    for (string k : strikes)
                        strikesReal.push_back(parseReal(k));

                    Calendar cal = NullCalendar(); // why do we need this?

                    // This can get wrapped in a QuantExt::BlackVolatilityWithATM later on
                    vol_ = boost::make_shared<BlackVarianceSurface>(asof, cal, dates, strikesReal, dVols, dc);
                }
            }
            vol_->enableExtrapolation();

            // some wild card
        } else {
            Calendar cal = NullCalendar(); 
            vol_ = boost::make_shared<BlackVarianceSurfaceSparse>(asof, cal, sExpiries, sStrikes, sVols, dc);
        }

    } catch (std::exception& e) {
        QL_FAIL("equity vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("equity vol curve building failed: unknown error");
    }
}


} // namespace data
} // namespace ore
