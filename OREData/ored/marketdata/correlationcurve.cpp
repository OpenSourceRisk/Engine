/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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
#include <ored/marketdata/correlationcurve.hpp>
#include <ored/utilities/log.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/termstructures/interpolatedcorrelationcurve.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

CorrelationCurve::CorrelationCurve(Date asof, CorrelationCurveSpec spec, const Loader& loader,
                                   const CurveConfigurations& curveConfigs) {

    try {

        const boost::shared_ptr<CorrelationCurveConfig>& config =
            curveConfigs.correlationCurveConfig(spec.curveConfigID());

        QL_REQUIRE(config->dimension() == CorrelationCurveConfig::Dimension::ATM ||
                       config->dimension() == CorrelationCurveConfig::Dimension::Constant,
                   "Unsupported correlation curve building dimension");

        // We loop over all market data, looking for quotes that match the configuration
        // until we found the whole matrix or do not have more quotes in the market data

        bool flat = (config->dimension() == CorrelationCurveConfig::Dimension::Constant);
    
        LOG("building " << (flat ? "flat" : "interpolated curve") << " correlation termstructure");
        
        vector<Period> optionTenors = config->optionTenors();
        vector<Handle<Quote>> corrs(flat ? 1 : optionTenors.size());
        vector<bool> found(corrs.size(), false);
        Size remainingQuotes = corrs.size();
        Size quotesRead = 0;

        LOG("loading quotes");

        for (auto& md : loader.loadQuotes(asof)) {

            if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::CORRELATION) {

                boost::shared_ptr<CorrelationQuote> q = boost::dynamic_pointer_cast<CorrelationQuote>(md);

                if (remainingQuotes > 0 && q != NULL && q->quoteType() == ore::data::MarketDatum::QuoteType::RATE) {

                    quotesRead++;

                    if (flat && q->expiry() == "FLAT") {
                        corrs[0] = q->quote();
                    } else {
                        Size i = std::find(optionTenors.begin(), optionTenors.end(), parsePeriod(q->expiry())) - optionTenors.begin();

                        if (i < optionTenors.size()) {
                            corrs[i] = q->quote();

                            if (!found[i]) {
                                found[i] = true;
                            }
                        }
                    }
                    if (--remainingQuotes == 0)
                        break;
                }

            }
        }

        LOG("CorrelationCurve: read " << quotesRead << " corrs");

        // Check that we have all the data we need
        if (remainingQuotes != 0 ) {
            ostringstream missingQuotes;
            missingQuotes << "Not all quotes found for spec " << spec << endl;
            if (!flat) {
                for (Size i = 0; i < optionTenors.size(); ++i) {
                    if (!found[i]) {
                        missingQuotes << "Missing corr: (" << optionTenors[i] << ")"
                                          << endl;
                    }
                }
                DLOGGERSTREAM << missingQuotes.str();
            }
            QL_FAIL("could not build correlation curve");
        }

        boost::shared_ptr<QuantExt::CorrelationTermStructure> corr;

        if (flat) {
            corr = boost::shared_ptr<QuantExt::CorrelationTermStructure>(new QuantExt::FlatCorrelation(
                asof, corrs[0],
                config->dayCounter()));

            corr->enableExtrapolation(config->extrapolate());
        } else {
            vector<Real> times(optionTenors.size());
            
            for (Size i = 0; i < optionTenors.size(); ++i) {
                Date d = config->calendar().advance(asof,
                            optionTenors[i],
                            config->businessDayConvention());

                times[i] = static_cast<Real>(d.serialNumber());
            }

            corr = boost::shared_ptr<QuantExt::CorrelationTermStructure>(new QuantExt::InterpolatedCorrelationCurve<Linear>(
                times, corrs, config->dayCounter(),
                config->calendar()));
        }

        LOG("Returning correlation surface for config " << spec);
        corr_ = corr;
           
    } catch (std::exception& e) {
        QL_FAIL("correlation curve building failed for curve " << spec.curveConfigID() << " on date " << io::iso_date(asof) << ": " << e.what());
    } catch (...) {
        QL_FAIL("correlation curve building failed: unknown error");
    }
}
} // namespace data
} // namespace ore
