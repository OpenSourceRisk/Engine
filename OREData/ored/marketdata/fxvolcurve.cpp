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

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

FXVolCurve::FXVolCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                       const CurveConfigurations& curveConfigs) {

    try {

        const boost::shared_ptr<FXVolatilityCurveConfig>& config = curveConfigs.fxVolCurveConfig(spec.curveConfigID());

        QL_REQUIRE(config->dimension() == FXVolatilityCurveConfig::Dimension::ATM,
                   "Unkown FX curve building dimension");

        // We loop over all market data, looking for quotes that match the configuration
        // every time we find a matching expiry we remove it from the list
        vector<boost::shared_ptr<FXOptionQuote>> quotes;
        vector<Period> expiries = config->expiries();
        for (auto& md : loader.loadQuotes(asof)) {
            // skip irrelevant data
            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::FX_OPTION) {

                boost::shared_ptr<FXOptionQuote> q = boost::dynamic_pointer_cast<FXOptionQuote>(md);

                if (q->unitCcy() == spec.unitCcy() && q->ccy() == spec.ccy() && q->strike() == "ATM") {

                    auto it = std::find(expiries.begin(), expiries.end(), q->expiry());
                    if (it != expiries.end()) {
                        // we have a hit
                        quotes.push_back(q);

                        // remove it from the list
                        expiries.erase(it);

                        // check if we are done
                        if (expiries.empty())
                            break;
                    }
                }
            }
        }

        LOG("FXVolCurve: read " << quotes.size() << " vols");

        // Check that we have all the expiries we need
        QL_REQUIRE(expiries.size() == 0, "No quote found for spec " << spec << " with expiry " << expiries.front());
        QL_REQUIRE(quotes.size() > 0, "No quotes found for spec " << spec);

        // daycounter used for interpolation in time.
        // TODO: push into conventions or config
        DayCounter dc = Actual365Fixed();

        // build vol curve
        if (quotes.size() == 1) {
            vol_ = boost::shared_ptr<BlackVolTermStructure>(
                new BlackConstantVol(asof, Calendar(), quotes.front()->quote()->value(), dc));
        } else {

            vector<Date> dates(quotes.size());
            vector<Volatility> atmVols(quotes.size());

            std::sort(quotes.begin(), quotes.end(),
                      [](const boost::shared_ptr<FXOptionQuote>& a, const boost::shared_ptr<FXOptionQuote>& b) -> bool {
                          return a->expiry() < b->expiry();
                      });

            for (Size i = 0; i < quotes.size(); i++) {
                dates[i] = asof + quotes[i]->expiry();
                atmVols[i] = quotes[i]->quote()->value();
            }

            vol_ = boost::shared_ptr<BlackVolTermStructure>(new BlackVarianceCurve(asof, dates, atmVols, dc));
        }
        vol_->enableExtrapolation();

    } catch (std::exception& e) {
        QL_FAIL("fx vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("fx vol curve building failed: unknown error");
    }
}
}
}
