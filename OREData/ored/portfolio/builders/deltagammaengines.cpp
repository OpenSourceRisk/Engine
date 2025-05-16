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

#include <ored/portfolio/builders/deltagammaengines.hpp>
#include <qle/pricingengines/blackswaptionenginedeltagamma.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<PricingEngine>
EuropeanSwaptionEngineBuilderDeltaGamma::engineImpl(const string& id, const string& key, const std::vector<Date>& dates,
                                          const std::vector<Date>& maturities, const std::vector<Real>& strikes, const bool isAmerican,
                                          const std::string& discountCurve, const std::string& securitySpread) {

    std::vector<Time> bucketTimesDeltaGamma = parseListOfValues<Time>(engineParameter("BucketTimesDeltaGamma"), &parseReal);
    std::vector<Time> bucketTimesVegaOpt = parseListOfValues<Time>(engineParameter("BucketTimesVegaOpt"), &parseReal);
    std::vector<Time> bucketTimesVegaUnd = parseListOfValues<Time>(engineParameter("BucketTimesVegaUnd"), &parseReal);
    bool computeDeltaVega = parseBool(engineParameter("ComputeDeltaVega"));
    bool computeGamma = parseBool(engineParameter("ComputeGamma"));

    QuantLib::ext::shared_ptr<IborIndex> index;
    string ccyCode = tryParseIborIndex(key, index) ? index->currency().code() : key;
    //const string& ccyCode = ccy.code();
    Handle<YieldTermStructure> yts = market_->discountCurve(ccyCode, configuration(MarketContext::pricing));
    QL_REQUIRE(!yts.empty(), "engineFactory error: yield term structure not found for currency " << ccyCode);
    Handle<SwaptionVolatilityStructure> svts = market_->swaptionVol(ccyCode, configuration(MarketContext::pricing));
    QL_REQUIRE(!svts.empty(), "engineFactory error: swaption vol structure not found for currency " << ccyCode);

    switch (svts->volatilityType()) {
    case ShiftedLognormal:
        LOG("Build BlackSwaptionEngineDeltaGamma for currency " << ccyCode);
        return boost::make_shared<QuantExt::BlackSwaptionEngineDeltaGamma>(yts, svts, bucketTimesDeltaGamma,
                                                                           bucketTimesVegaOpt, bucketTimesVegaUnd,
                                                                           computeDeltaVega, computeGamma);
    case Normal:
        LOG("Build BachelierSwaptionEngineDeltaGamma for currency " << ccyCode);
        return boost::make_shared<QuantExt::BachelierSwaptionEngineDeltaGamma>(yts, svts, bucketTimesDeltaGamma,
                                                                               bucketTimesVegaOpt, bucketTimesVegaUnd,
                                                                               computeDeltaVega, computeGamma);
    default:
        QL_FAIL("Swaption volatility type " << svts->volatilityType() << "not covered in EngineFactory");
        break;
    }
}
  
} // namespace data
} // namespace ore
