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

#include <ored/portfolio/builders/cms.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

GFunctionFactory::YieldCurveModel ycmFromString(const string& s) {
    if (s == "Standard")
        return GFunctionFactory::Standard;
    else if (s == "ExactYield")
        return GFunctionFactory::ExactYield;
    else if (s == "ParallelShifts")
        return GFunctionFactory::ParallelShifts;
    else if (s == "NonParallelShifts")
        return GFunctionFactory::NonParallelShifts;
    else
        QL_FAIL("unknown string for YieldCurveModel");
}

QuantLib::ext::shared_ptr<FloatingRateCouponPricer> AnalyticHaganCmsCouponPricerBuilder::engineImpl(const string& key) {

    std::string ccyCode = key;
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (tryParseIborIndex(key, index))
        ccyCode = index->currency().code();
    Real rev = parseReal(engineParameter("MeanReversion", {key, ccyCode}, true));
    string ycmstr = engineParameter("YieldCurveModel");
    GFunctionFactory::YieldCurveModel ycm = ycmFromString(ycmstr);

    Handle<Quote> revQuote(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(rev)));
    Handle<SwaptionVolatilityStructure> vol = market_->swaptionVol(key, configuration(MarketContext::pricing));

    QuantLib::ext::shared_ptr<FloatingRateCouponPricer> pricer = QuantLib::ext::make_shared<AnalyticHaganPricer>(vol, ycm, revQuote);

    // Return the cached pricer
    return pricer;
}

QuantLib::ext::shared_ptr<FloatingRateCouponPricer> NumericalHaganCmsCouponPricerBuilder::engineImpl(const string& key) {

    std::string ccyCode = key;
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (tryParseIborIndex(key, index))
        ccyCode = index->currency().code();
    Real rev = parseReal(engineParameter("MeanReversion", {key, ccyCode}, true));
    string ycmstr = engineParameter("YieldCurveModel");
    GFunctionFactory::YieldCurveModel ycm = ycmFromString(ycmstr);
    Rate llim = parseReal(engineParameter("LowerLimit"));
    Rate ulim = parseReal(engineParameter("UpperLimit"));
    Real prec = parseReal(engineParameter("Precision"));

    Handle<Quote> revQuote(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(rev)));
    Handle<SwaptionVolatilityStructure> vol = market_->swaptionVol(key, configuration(MarketContext::pricing));

    QuantLib::ext::shared_ptr<FloatingRateCouponPricer> pricer =
        QuantLib::ext::make_shared<NumericHaganPricer>(vol, ycm, revQuote, llim, ulim, prec);

    // Return the cached pricer
    return pricer;
}

QuantLib::ext::shared_ptr<FloatingRateCouponPricer> LinearTSRCmsCouponPricerBuilder::engineImpl(const string& key) {

    std::string ccyCode = key;
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (tryParseIborIndex(key, index))
        ccyCode = index->currency().code();
    Real rev = parseReal(engineParameter("MeanReversion", {key, ccyCode}, true));
    string policy = engineParameter("Policy");

    Handle<Quote> revQuote(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(rev)));
    Handle<SwaptionVolatilityStructure> vol = market_->swaptionVol(key, configuration(MarketContext::pricing));
    Handle<YieldTermStructure> yts = market_->discountCurve(ccyCode, configuration(MarketContext::pricing));

    string lowerBoundStr =
        (vol->volatilityType() == ShiftedLognormal) ? "LowerRateBoundLogNormal" : "LowerRateBoundNormal";
    string upperBoundStr =
        (vol->volatilityType() == ShiftedLognormal) ? "UpperRateBoundLogNormal" : "UpperRateBoundNormal";

    LinearTsrPricer::Settings settings;
    if (policy == "RateBound") {
        Real lower = parseReal(engineParameter(lowerBoundStr));
        Real upper = parseReal(engineParameter(upperBoundStr));
        settings.withRateBound(lower, upper);
    } else if (policy == "VegaRatio") {
        Real lower = parseReal(engineParameter(lowerBoundStr));
        Real upper = parseReal(engineParameter(upperBoundStr));
        Real vega = parseReal(engineParameter("VegaRatio"));
        settings.withVegaRatio(vega, lower, upper);
    } else if (policy == "PriceThreshold") {
        Real lower = parseReal(engineParameter(lowerBoundStr));
        Real upper = parseReal(engineParameter(upperBoundStr));
        Real threshold = parseReal(engineParameter("PriceThreshold"));
        settings.withPriceThreshold(threshold, lower, upper);
    } else if (policy == "BsStdDev") {
        Real lower = parseReal(engineParameter(lowerBoundStr));
        Real upper = parseReal(engineParameter(upperBoundStr));
        Real stddevs = parseReal(engineParameter("BSStdDevs"));
        settings.withPriceThreshold(stddevs, lower, upper);
    } else
        QL_FAIL("unknown string for policy parameter");

    QuantLib::ext::shared_ptr<FloatingRateCouponPricer> pricer =
        QuantLib::ext::make_shared<LinearTsrPricer>(vol, revQuote, yts, settings);

    // Return the cached pricer
    return pricer;
}
} // namespace data
} // namespace ore
