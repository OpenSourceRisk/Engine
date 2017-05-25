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

#include <ored/marketdata/cdsvolcurve.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

#include <algorithm>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

CDSVolCurve::CDSVolCurve(Date asof, CDSVolatilityCurveSpec spec, const Loader& loader,
                         const CurveConfigurations& curveConfigs) {

    try {
        const boost::shared_ptr<CDSVolatilityCurveConfig>& config =
            curveConfigs.cdsVolCurveConfig(spec.curveConfigID());
        // We loop over all market data, looking for quotes that match the configuration
        // every time we find a matching expiry we remove it from the list
        vector<boost::shared_ptr<IndexCDSOptionQuote>> quotes;
        vector<string> expiries = config->expiries();
        for (auto& md : loader.loadQuotes(asof)) {
            // skip irrelevant data
            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::INDEX_CDS_OPTION) {
                boost::shared_ptr<IndexCDSOptionQuote> q = boost::dynamic_pointer_cast<IndexCDSOptionQuote>(md);
                if (q->indexName() == spec.curveConfigID()) {
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
        LOG("CDSVolCurve: read " << quotes.size() << " vols");
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
                      [asof](const boost::shared_ptr<IndexCDSOptionQuote>& a,
                             const boost::shared_ptr<IndexCDSOptionQuote>& b) -> bool {
                          Date a_tmp_date, b_tmp_date;
                          Period a_tmp_per, b_tmp_per;
                          bool a_tmp_isDate, b_tmp_isDate;
                          parseDateOrPeriod(a->expiry(), a_tmp_date, a_tmp_per, a_tmp_isDate);
                          if (!a_tmp_isDate)
                              a_tmp_date = WeekendsOnly().adjust(asof + a_tmp_per);
                          parseDateOrPeriod(b->expiry(), b_tmp_date, b_tmp_per, b_tmp_isDate);
                          if (!b_tmp_isDate)
                              b_tmp_date = WeekendsOnly().adjust(asof + b_tmp_per);
                          return a_tmp_date < b_tmp_date;
                      });
            for (Size i = 0; i < quotes.size(); i++) {
                Date tmpDate;
                Period tmpPer;
                bool tmpIsDate;
                parseDateOrPeriod(quotes[i]->expiry(), tmpDate, tmpPer, tmpIsDate);
                // FIXME, is the CDS Option expiry simply asof + ten or is this following
                // the standard CDS date schedule
                if (!tmpIsDate)
                    tmpDate = WeekendsOnly().adjust(asof + tmpPer);
                QL_REQUIRE(tmpDate > asof, "Credit Vol Curve cannot contain a vol quote for a past date ("
                                               << io::iso_date(tmpDate) << ")");
                dates[i] = tmpDate;
                atmVols[i] = quotes[i]->quote()->value();
            }
            vol_ = boost::shared_ptr<BlackVolTermStructure>(new BlackVarianceCurve(asof, dates, atmVols, dc));
        }
        vol_->enableExtrapolation();
    } catch (std::exception& e) {
        QL_FAIL("cds vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("cds vol curve building failed: unknown error");
    }
}
} // namespace data
} // namespace ore
