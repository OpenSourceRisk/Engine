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

        
        LOG("loading quotes");
        vector<Period> optionTenors = config->optionTenors();
        vector<Handle<Quote>> corrs(optionTenors.size());
        bool failed = false;
        
        //search market data loader for quotes, logging missing ones
        for (auto& q : config->quotes()) {
            if (loader.has(q, asof)) {
                boost::shared_ptr<CorrelationQuote> c = boost::dynamic_pointer_cast<CorrelationQuote>(loader.get(q, asof));
                
                Size i = std::find(optionTenors.begin(), optionTenors.end(), parsePeriod(c->expiry())) - optionTenors.begin();
                QL_REQUIRE(i < optionTenors.size(), "correlation tenor not found for " << q);
                corrs[i] = c->quote();
            } else {
                DLOGGERSTREAM << "could not find correlation quote " << q << std::endl;
                failed = true;
            }
        }

        //fail if any quotes missing
        if (failed) {
            QL_FAIL("could not build correlation curve");
        }



        //build correlation termsructure
        bool flat = (config->dimension() == CorrelationCurveConfig::Dimension::Constant);
        LOG("building " << (flat ? "flat" : "interpolated curve") << " correlation termstructure");
        
        boost::shared_ptr<QuantExt::CorrelationTermStructure> corr;
        if (flat) {
            corr = boost::shared_ptr<QuantExt::CorrelationTermStructure>(new QuantExt::FlatCorrelation(
                0, config->calendar(), corrs[0],
                config->dayCounter()));

            corr->enableExtrapolation(config->extrapolate());
        } else {
            vector<Real> times(optionTenors.size());
            
            for (Size i = 0; i < optionTenors.size(); ++i) {
                Date d = config->calendar().advance(asof,
                            optionTenors[i],
                            config->businessDayConvention());

                times[i] = config->dayCounter().yearFraction(asof, d);

                LOG("asof " << asof << ", tenor " << optionTenors[i] << ", date " << d << ", time " << times[i]);
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
