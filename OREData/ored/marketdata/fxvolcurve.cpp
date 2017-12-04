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
#include <ored/marketdata/fxvolcurve.hpp>
#include <ored/utilities/log.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/termstructures/fxblackvolsurface.hpp>
#include <ql/time/calendars/target.hpp>

using namespace QuantLib;
using namespace std;

namespace {

// utility to get a handle out of a Curve object
template<class T, class K>
Handle<T> getHandle(const string& spec, const map<string, boost::shared_ptr<K>>& m) {
    auto it = m.find(spec);
    QL_REQUIRE(it != m.end(), "FXVolCurve: Can't find spec " << spec);
    return it->second->handle();
}

}

namespace ore {
namespace data {

FXVolCurve::FXVolCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                       const CurveConfigurations& curveConfigs,
                       const map<string, boost::shared_ptr<FXSpot>>& fxSpots,
                       const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves) {

    try {

        const boost::shared_ptr<FXVolatilityCurveConfig>& config = curveConfigs.fxVolCurveConfig(spec.curveConfigID());

        QL_REQUIRE(config->dimension() == FXVolatilityCurveConfig::Dimension::ATM ||
                   config->dimension() == FXVolatilityCurveConfig::Dimension::Smile,
                   "Unkown FX curve building dimension");

        bool isATM = config->dimension() == FXVolatilityCurveConfig::Dimension::ATM;

        // We loop over all market data, looking for quotes that match the configuration
        // every time we find a matching expiry we remove it from the list
        // we replicate this for all 3 types of quotes were applicable.
        Size n = isATM ? 1 : 3; // [0] = ATM, [1] = RR, [2] = BF
        vector<vector<boost::shared_ptr<FXOptionQuote>>> quotes(n);
        vector<vector<Period>> expiries(n, config->expiries());
        for (auto& md : loader.loadQuotes(asof)) {
            // skip irrelevant data
            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::FX_OPTION) {

                boost::shared_ptr<FXOptionQuote> q = boost::dynamic_pointer_cast<FXOptionQuote>(md);

                if (q->unitCcy() == spec.unitCcy() && q->ccy() == spec.ccy()) {

                    Size idx = 999999;
                    if (q->strike() == "ATM")
                        idx = 0;
                    else if (q->strike() == "25RR")
                        idx = 1;
                    else if (q->strike() == "25BF")
                        idx = 2;

                    // silently skip unknown strike strings
                    if ((isATM && idx == 0) || (!isATM && idx <= 2)) {
                        auto it = std::find(expiries[idx].begin(), expiries[idx].end(), q->expiry());
                        if (it != expiries[idx].end()) {
                            // we have a hit
                            quotes[idx].push_back(q);
                            // remove it from the list
                            expiries[idx].erase(it);
                        }

                        // check if we are done
                        // for ATM we just check expiries[0], otherwise we check all 3
                        if (expiries[0].empty() &&
                            (isATM || (expiries[1].empty() && expiries[2].empty())))
                            break;
                    }

                }
            }
        }

        // Check ATM first
        // Check that we have all the expiries we need
        LOG("FXVolCurve: read " << quotes[0].size() << " ATM vols");
        QL_REQUIRE(expiries[0].size() == 0, "No ATM quote found for spec " << spec << " with expiry " << expiries[0].front());
        QL_REQUIRE(quotes[0].size() > 0, "No ATM quotes found for spec " << spec);
        // No check the rest
        if (!isATM) {
            LOG("FXVolCurve: read " << quotes[1].size() << " RR and " << quotes[2].size() << " BF quotes");
            QL_REQUIRE(expiries[1].size() == 0, "No RR quote found for spec " << spec << " with expiry " << expiries[1].front());
            QL_REQUIRE(expiries[2].size() == 0, "No BF quote found for spec " << spec << " with expiry " << expiries[2].front());

        }

        // daycounter used for interpolation in time.
        // TODO: push into conventions or config
        DayCounter dc = config->dayCounter();
        Calendar cal = config->calendar();

        // sort all quotes
        for (Size i = 0; i < n; i++) {
            std::sort(quotes[i].begin(), quotes[i].end(),
                      [](const boost::shared_ptr<FXOptionQuote>& a, const boost::shared_ptr<FXOptionQuote>& b) -> bool {
                          return a->expiry() < b->expiry();
                      });
        }

        // build vol curve
        if (isATM && quotes[0].size() == 1) {
            vol_ = boost::shared_ptr<BlackVolTermStructure>(
                new BlackConstantVol(asof, Calendar(), quotes[0].front()->quote()->value(), dc));
        } else {

            Size numExpiries = quotes[0].size();
            vector<Date> dates(numExpiries);
            vector<vector<Volatility>> vols(n, vector<Volatility>(numExpiries)); // same as above: [0] = ATM, etc.

            for (Size i = 0; i < numExpiries; i++) {
                dates[i] = asof + quotes[0][i]->expiry();
                for (Size idx = 0; idx < n; idx++) 
                    vols[idx][i] = quotes[idx][i]->quote()->value();
            }

            if (isATM) {
                // ATM
                vol_ = boost::shared_ptr<BlackVolTermStructure>(new BlackVarianceCurve(asof, dates, vols[0], dc));
            } else {
                // Smile
                auto fxSpot = getHandle<Quote>(config->fxSpotID(), fxSpots);
                auto domYTS = getHandle<YieldTermStructure>(config->fxDomesticYieldCurveID(), yieldCurves);
                auto forYTS = getHandle<YieldTermStructure>(config->fxForeignYieldCurveID(), yieldCurves);

                vol_ = boost::shared_ptr<BlackVolTermStructure>(
                    new QuantExt::FxBlackVannaVolgaVolatilitySurface(
                        asof, dates, vols[0], vols[1], vols[2], dc, cal, fxSpot, domYTS, forYTS));

            }
        }
        vol_->enableExtrapolation();

    } catch (std::exception& e) {
        QL_FAIL("fx vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("fx vol curve building failed: unknown error");
    }
}
} // namespace data
} // namespace ore
