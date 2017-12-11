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

        // We store them all in a matrix, we start with all values negative and use
        // this to check if they have been set.
        Matrix vols(strikes.size(), expiries.size(), -1.0);

        // We loop over all market data, looking for quotes that match the configuration
        for (auto& md : loader.loadQuotes(asof)) {
            // skip irrelevant data
            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::EQUITY_OPTION) {
                boost::shared_ptr<EquityOptionQuote> q = boost::dynamic_pointer_cast<EquityOptionQuote>(md);
                if (q->eqName() == spec.curveConfigID() && q->ccy() == spec.ccy()) {

                    Size i = std::find(strikes.begin(), strikes.end(), q->strike()) - strikes.begin();
                    Size j = std::find(expiries.begin(), expiries.end(), q->expiry()) - expiries.begin();

                    if (i < strikes.size() && j < expiries.size()) {
                        QL_REQUIRE(vols[i][j] < 0, "Error vol (" << spec << ") for " << strikes[i] << ", "
                                                                 << expiries[j] << " already set");
                        vols[i][j] = q->quote()->value();
                    }
                }
            }
        }
        // Check that we have all the quotes we need
        for (Size i = 0; i < strikes.size(); i++) {
            for (Size j = 0; j < expiries.size(); j++) {
                QL_REQUIRE(vols[i][j] >= 0,
                           "Error vol (" << spec << ") for " << strikes[i] << ", " << expiries[j] << " not set");
            }
        }

        if (expiries.size() == 1 && strikes.size() == 1) {
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
