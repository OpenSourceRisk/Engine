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
#include <ored/utilities/parsers.hpp>
#include <ored/marketdata/bondspreadimplymarket.hpp>

#include <qle/pricingengines/mclgmfwdbondengine.hpp>
#include <qle/models/projectedcrossassetmodel.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

FwdBondEngineBuilder::Curves FwdBondEngineBuilder::getCurves(const string& id, const Currency& ccy,
                                                             const string& contractId,
                                                             const std::string& discountCurveName,
                                                             const string& creditCurveId, const string& securityId,
                                                             const string& referenceCurveId,
                                                             const string& incomeCurveId, const bool dirty) {
    FwdBondEngineBuilder::Curves curves;
    // for discounting underlying bond make use of reference curve
    curves.referenceCurve_ = referenceCurveId.empty()
                          ? market_->discountCurve(ccy.code(), configuration(MarketContext::pricing))
                          : indexOrYieldCurve(market_, referenceCurveId, configuration(MarketContext::pricing));

    // include bond spread, if any
    try {
        auto bondspreadimplymarket = QuantLib::ext::dynamic_pointer_cast<BondSpreadImplyMarket>(market_);
        if (bondspreadimplymarket) {
            // this is the ctd spread -- require for bondspreadimply
            // if the spread imply is fullfilled, this spread is stored under the contract id -> the else case
            curves.bondSpread_ = market_->securitySpread(securityId, configuration(MarketContext::pricing));
            DLOG("FwdBondEngineBuilder::getCurves : id  " << id << " using ctd id " << securityId << " with spread of " << curves.bondSpread_->value());
        } else {
            // this is the contact spread -- required for anything else
            curves.bondSpread_ = market_->securitySpread(contractId, configuration(MarketContext::pricing));
            DLOG("FwdBondEngineBuilder::getCurves : id  " << id << " using contract id " << contractId << " with spread of " << curves.bondSpread_->value());
        }
    } catch (...) {
        curves.bondSpread_ = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    }

    // include spread
    curves.spreadedReferenceCurve_ = Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(curves.referenceCurve_, curves.bondSpread_));

    // income curve, fallback reference curve (w/o spread)
    curves.incomeCurve_ = market_->yieldCurve(incomeCurveId.empty() ? referenceCurveId : incomeCurveId,
                                             configuration(MarketContext::pricing));

    bool spreadOnIncome = parseBool(engineParameter("SpreadOnIncomeCurve", {}, false, "false"));

    if (spreadOnIncome) {
        curves.incomeCurve_ = Handle<YieldTermStructure>(
            QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(curves.incomeCurve_, curves.bondSpread_));
    }

    // to discount the forward contract, might be a OIS curve
    curves.discountCurve_ = discountCurveName.empty()
                               ? market_->discountCurve(ccy.code(), configuration(MarketContext::pricing))
                               : indexOrYieldCurve(market_, discountCurveName, configuration(MarketContext::pricing));
    // // include contract spread, if any
    // try {
    //     curves.contractSpread_ = market_->securitySpread(contractId, configuration(MarketContext::pricing));
    // } catch (...) {
    //     curves.contractSpread_ = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    // }
    // // include spread
    // curves.discountCurve_ = Handle<YieldTermStructure>(
    //     QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(curves.discountCurve_, curves.contractSpread_));

    try {
        curves.conversionFactor_ = market_->conversionFactor(securityId, configuration(MarketContext::pricing));
    } catch (...) {
        curves.conversionFactor_ = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    }
    if (dirty && curves.conversionFactor_->value() != 1.0) {
        WLOG("conversionFactor for " << securityId << " is overwritten to 1.0, settlement is dirty")
        curves.conversionFactor_ = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    }

    // credit curve may not always be used. If credit curve ID is empty proceed without it
    if (!creditCurveId.empty()){
        curves.dpts_ =
            securitySpecificCreditCurve(market_, securityId, creditCurveId, configuration(MarketContext::pricing))
                ->curve();
    } else {
        curves.dpts_ = Handle<DefaultProbabilityTermStructure>();
    }

    try {
        curves.recovery_ = market_->recoveryRate(securityId, configuration(MarketContext::pricing));
    } catch (...) {
        // otherwise fall back on curve recovery
        WLOG("security specific recovery rate not found for security ID "
             << securityId << ", falling back on the recovery rate for credit curve Id " << creditCurveId);
        if (!creditCurveId.empty())
            curves.recovery_ = market_->recoveryRate(creditCurveId, configuration(MarketContext::pricing));
    }
    return curves;
};

QuantLib::ext::shared_ptr<PricingEngine> CamAmcFwdBondEngineBuilder::buildMcEngine(
    const QuantLib::ext::shared_ptr<LGM>& lgm, const Handle<YieldTermStructure>& discountCurve,
    const std::vector<Date>& simulationDates, const std::vector<Size>& externalModelIndices,
    const Handle<YieldTermStructure>& incomeCurve, const Handle<YieldTermStructure>& contractCurve, const Handle<Quote>& conversionFactor) {

    return QuantLib::ext::make_shared<QuantExt::McLgmFwdBondEngine>(
        lgm, parseSequenceType(engineParameter("Training.Sequence")),
        parseSequenceType(engineParameter("Pricing.Sequence")), parseInteger(engineParameter("Training.Samples")),
        parseInteger(engineParameter("Pricing.Samples")), parseInteger(engineParameter("Training.Seed")),
        parseInteger(engineParameter("Pricing.Seed")), parseInteger(engineParameter("Training.BasisFunctionOrder")),
        parsePolynomType(engineParameter("Training.BasisFunction")),
        parseSobolBrownianGeneratorOrdering(engineParameter("BrownianBridgeOrdering")),
        parseSobolRsgDirectionIntegers(engineParameter("SobolDirectionIntegers")), discountCurve, incomeCurve,
        contractCurve, conversionFactor, simulationDates, stickyCloseOutDates_, externalModelIndices,
        parseBool(engineParameter("MinObsDate")),
        parseRegressorModel(engineParameter("RegressorModel", {}, false, "Simple")),
        parseRealOrNull(engineParameter("RegressionVarianceCutoff", {}, false, std::string())),
        parseBool(engineParameter("RecalibrateOnStickyCloseOutDates", {}, false, "false")),
        parseBool(engineParameter("ReevaluateExerciseInStickyRun", {}, false, "false")));
}

QuantLib::ext::shared_ptr<PricingEngine>
CamAmcFwdBondEngineBuilder::engineImpl(const string& id, const Currency& ccy, const string& contractId,
                                       const string& discountCurveName, const string& creditCurveId,
                                       const string& securityId, const string& referenceCurveId,
                                       const string& incomeCurveId, const bool dirty) {

    DLOG("Building AMC Fwd Bond engine for ccy " << ccy << " (from externally given CAM)");

    std::vector<Size> externalModelIndices;
    Handle<CrossAssetModel> model(getProjectedCrossAssetModel(
        cam_, {std::make_pair(CrossAssetModel::AssetType::IR, cam_->ccyIndex(ccy))}, externalModelIndices));

    auto curves = getCurves(id, ccy, contractId, discountCurveName, creditCurveId, securityId, referenceCurveId,
                            incomeCurveId, dirty);

    return buildMcEngine(model->lgm(0), curves.spreadedReferenceCurve_, simulationDates_, externalModelIndices,
                         curves.incomeCurve_, curves.discountCurve_, curves.conversionFactor_);
}

} // namespace data
} // namespace ore
