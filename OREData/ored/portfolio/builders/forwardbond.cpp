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

#include <ored/portfolio/builders/forwardbond.hpp>
#include <qle/pricingengines/mclgmfwdbondengine.hpp>
#include <ored/utilities/parsers.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

QuantLib::ext::shared_ptr<PricingEngine> CamAmcFwdBondEngineBuilder::buildMcEngine(
    const QuantLib::ext::shared_ptr<LGM>& lgm, const Handle<YieldTermStructure>& discountCurve,
    const std::vector<Date>& simulationDates, const std::vector<Size>& externalModelIndices,
    const Handle<YieldTermStructure>& incomeCurve, const Handle<YieldTermStructure>& contractCurve,
    const Handle<YieldTermStructure>& referenceCurve, const Handle<Quote>& conversionFactor) {

    return QuantLib::ext::make_shared<QuantExt::McLgmFwdBondEngine>(
        lgm, parseSequenceType(engineParameter("Training.Sequence")),
        parseSequenceType(engineParameter("Pricing.Sequence")), parseInteger(engineParameter("Training.Samples")),
        parseInteger(engineParameter("Pricing.Samples")), parseInteger(engineParameter("Training.Seed")),
        parseInteger(engineParameter("Pricing.Seed")), parseInteger(engineParameter("Training.BasisFunctionOrder")),
        parsePolynomType(engineParameter("Training.BasisFunction")),
        parseSobolBrownianGeneratorOrdering(engineParameter("BrownianBridgeOrdering")),
        parseSobolRsgDirectionIntegers(engineParameter("SobolDirectionIntegers")), discountCurve, simulationDates,
        externalModelIndices, parseBool(engineParameter("MinObsDate")),
        parseRegressorModel(engineParameter("RegressorModel", {}, false, "Simple")),
        parseRealOrNull(engineParameter("RegressionVarianceCutoff", {}, false, std::string())), incomeCurve,
        contractCurve, referenceCurve, conversionFactor);
}

QuantLib::ext::shared_ptr<PricingEngine>
CamAmcFwdBondEngineBuilder::engineImpl(const string& id, const Currency& ccy, const string& discountCurveName,
                                       const string& creditCurveId, const bool hasCreditRisk, const string& securityId,
                                       const string& referenceCurveId, const string& incomeCurveId, const bool dirty) {

    DLOG("Building AMC Fwd Bond engine for ccy " << ccy << " (from externally given CAM)");

    QL_REQUIRE(cam_ != nullptr, "CamAmcFwdBondEngineBuilder::engineImpl: cam is null");
    Size currIdx = cam_->ccyIndex(ccy);
    auto lgm = cam_->lgm(currIdx);
    std::vector<Size> modelIndex(1, cam_->pIdx(CrossAssetModel::AssetType::IR, currIdx));

    WLOG("CamAmcFwdBondEngineBuilder : creditCurveId not used at present");

    // for discounting underlying bond make use of reference curve
    Handle<YieldTermStructure> referenceCurve =
        referenceCurveId.empty() ? market_->discountCurve(ccy.code(), configuration(MarketContext::pricing))
                                 : indexOrYieldCurve(market_, referenceCurveId, configuration(MarketContext::pricing));

    Handle<YieldTermStructure> spreadedCurve = referenceCurve;
    if (!securityId.empty())
        spreadedCurve = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
            spreadedCurve, market_->securitySpread(securityId, configuration(MarketContext::pricing))));

    // fall back on reference curve if no income curve is specified
    Handle<YieldTermStructure> incomeCurve = market_->yieldCurve(
        incomeCurveId.empty() ? referenceCurveId : incomeCurveId, configuration(MarketContext::pricing));

    // to discount the contract, might be a OIS curve
    Handle<YieldTermStructure> contractCurve =
        discountCurveName.empty()
            ? market_->discountCurve(ccy.code(), configuration(MarketContext::pricing))
            : indexOrYieldCurve(market_, discountCurveName, configuration(MarketContext::pricing));

    Handle<Quote> conversionFactor;
    try {
        conversionFactor = market_->conversionFactor(securityId, configuration(MarketContext::pricing));
    } catch (...) {
        conversionFactor = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    }
    if (dirty && conversionFactor->value() != 1.0) {
        WLOG("conversionFactor for " << securityId << " is overwritten to 1.0, settlement is dirty")
        conversionFactor = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    }

    return buildMcEngine(lgm, spreadedCurve, simulationDates_, modelIndex, incomeCurve, contractCurve, referenceCurve,
                         conversionFactor);
}

} // namespace data
} // namespace ore
