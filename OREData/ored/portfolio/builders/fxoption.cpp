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

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/crossassetmodeldata.hpp>
#include <ored/model/fxbsdata.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/pricingengines/mccamfxoptionengine.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

QuantLib::ext::shared_ptr<PricingEngine> CamAmcFxOptionEngineBuilder::engineImpl(const string& assetName,
                                                                         const Currency& domCcy,
                                                                         const AssetClass& assetClassUnderlying,
                                                                         const Date& expiryDate, const bool useFxSpot) {

    QL_REQUIRE(assetClassUnderlying == AssetClass::FX, "FX Option required");
    Currency forCcy = parseCurrency(assetName);

    std::string ccysStr = forCcy.code() + "_" + domCcy.code();

    DLOG("Building AMC FX option engine for ccys " << ccysStr << " (from externally given CAM)");

    QL_REQUIRE(domCcy != forCcy, "CamAmcFxOptionEngineBuilder: domCcy = forCcy = " << domCcy.code());

    std::vector<Size> externalModelIndices;
    std::vector<Handle<YieldTermStructure>> discountCurves;
    std::vector<Size> cIdx;
    std::vector<QuantLib::ext::shared_ptr<IrModel>> lgm;
    std::vector<QuantLib::ext::shared_ptr<FxBsParametrization>> fx;

    // add the IR and FX components in the order they appear in the CAM; this way
    // we can sort the external model indices and be sure that they match up with
    // the indices 0,1,2,3,... of the projected model we build here
    // keep the base ccy in every case
    for (Size i = 0; i < cam_->components(CrossAssetModel::AssetType::IR); ++i) {
        if (i == 0 || cam_->irlgm1f(i)->currency() == domCcy || cam_->irlgm1f(i)->currency() == forCcy) {
            lgm.push_back(cam_->lgm(i));
            externalModelIndices.push_back(cam_->pIdx(CrossAssetModel::AssetType::IR, i));
            cIdx.push_back(cam_->cIdx(CrossAssetModel::AssetType::IR, i));
            if (i > 0) {
                fx.push_back(cam_->fxbs(i - 1));
                externalModelIndices.push_back(cam_->pIdx(CrossAssetModel::AssetType::FX, i - 1));
                cIdx.push_back(cam_->cIdx(CrossAssetModel::AssetType::FX, i - 1));
            }
        }
    }

    std::sort(externalModelIndices.begin(), externalModelIndices.end());
    std::sort(cIdx.begin(), cIdx.end());

    // build correlation matrix
    Matrix corr(cIdx.size(), cIdx.size(), 1.0);
    for (Size i = 0; i < cIdx.size(); ++i) {
        for (Size j = 0; j < i; ++j) {
            corr(i, j) = corr(j, i) = cam_->correlation()(cIdx[i], cIdx[j]);
        }
    }
    Handle<CrossAssetModel> model(QuantLib::ext::make_shared<CrossAssetModel>(lgm, fx, corr));
    // we assume that the model has the pricing discount curves attached already, so
    // we leave the discountCurves vector empty here

    // build the pricing engine

    // NPV should be in domCcy, consistent with the npv currency of an ORE FX Option Trade
    auto engine = QuantLib::ext::make_shared<McCamFxOptionEngine>(
        model, domCcy, forCcy, domCcy, parseSequenceType(engineParameter("Training.Sequence")),
        parseSequenceType(engineParameter("Pricing.Sequence")), parseInteger(engineParameter("Training.Samples")),
        parseInteger(engineParameter("Pricing.Samples")), parseInteger(engineParameter("Training.Seed")),
        parseInteger(engineParameter("Pricing.Seed")), parseInteger(engineParameter("Training.BasisFunctionOrder")),
        parsePolynomType(engineParameter("Training.BasisFunction")),
        parseSobolBrownianGeneratorOrdering(engineParameter("BrownianBridgeOrdering")),
        parseSobolRsgDirectionIntegers(engineParameter("SobolDirectionIntegers")), discountCurves, simulationDates_,
        externalModelIndices, parseBool(engineParameter("MinObsDate")),
        parseRegressorModel(engineParameter("RegressorModel", {}, false, "Simple")),
        parseRealOrNull(engineParameter("RegressionVarianceCutoff", {}, false, std::string())));

    return engine;
}

} // namespace data
} // namespace ore
