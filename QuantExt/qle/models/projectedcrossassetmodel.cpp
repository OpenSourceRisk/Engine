/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <qle/models/projectedcrossassetmodel.hpp>

namespace QuantExt {

QuantLib::ext::shared_ptr<CrossAssetModel>
getProjectedCrossAssetModel(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
                            const std::vector<std::pair<CrossAssetModel::AssetType, Size>>& selectedComponents,
                            std::vector<Size>& projectedStateProcessIndices) {

    projectedStateProcessIndices.clear();

    // vectors holding the selected parametrizations and associated indices in the correlation matrix

    std::vector<QuantLib::ext::shared_ptr<Parametrization>> parametrizations;
    std::vector<Size> correlationIndices;

    // loop over selected components and fill
    // - parametrizations
    // - correlation indices
    // - state process indices

    for (auto const& c : selectedComponents) {
        parametrizations.push_back(model->parametrizations().at(model->idx(c.first, c.second)));
        for (Size b = 0; b < model->brownians(c.first, c.second); ++b)
            correlationIndices.push_back(model->cIdx(c.first, c.second, b));
        for (Size p = 0; p < model->stateVariables(c.first, c.second); ++p)
            projectedStateProcessIndices.push_back(model->pIdx(c.first, c.second, p));
    }

    // build correlation matrix

    Matrix correlation(correlationIndices.size(), correlationIndices.size());
    for (Size j = 0; j < correlationIndices.size(); ++j) {
        correlation(j, j) = 1.0;
        for (Size k = 0; k < j; ++k) {
            correlation(k, j) = correlation(j, k) =
                model->correlation()(correlationIndices.at(j), correlationIndices.at(k));
        }
    }

    // build projected cam and return it

    return QuantLib::ext::make_shared<CrossAssetModel>(parametrizations, correlation, model->salvagingAlgorithm(),
                                               model->measure(), model->discretization());
}

std::vector<Size> getStateProcessProjection(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
                                            const QuantLib::ext::shared_ptr<CrossAssetModel>& projectedModel) {

    std::vector<Size> stateProcessProjection(projectedModel->stateProcess()->size(), Null<Size>());

    for (Size i = 0; i < model->components(CrossAssetModel::AssetType::IR); ++i) {
        for (Size j = 0; j < projectedModel->components(CrossAssetModel::AssetType::IR); ++j) {
            if (projectedModel->ir(j)->currency() == model->ir(i)->currency()) {
                for (Size k = 0; k < projectedModel->stateVariables(CrossAssetModel::AssetType::IR, j); ++k) {
                    stateProcessProjection[projectedModel->pIdx(CrossAssetModel::AssetType::IR, j, k)] =
                        model->pIdx(CrossAssetModel::AssetType::IR, i, k);
                }
            }
        }
    }

    for (Size i = 0; i < model->components(CrossAssetModel::AssetType::FX); ++i) {
        for (Size j = 0; j < projectedModel->components(CrossAssetModel::AssetType::FX); ++j) {
            if (projectedModel->fx(j)->currency() == model->fx(i)->currency()) {
                for (Size k = 0; k < projectedModel->stateVariables(CrossAssetModel::AssetType::FX, j); ++k) {
                    stateProcessProjection[projectedModel->pIdx(CrossAssetModel::AssetType::FX, j, k)] =
                        model->pIdx(CrossAssetModel::AssetType::FX, i, k);
                }
            }
        }
    }

    for (Size i = 0; i < model->components(CrossAssetModel::AssetType::INF); ++i) {
        for (Size j = 0; j < projectedModel->components(CrossAssetModel::AssetType::INF); ++j) {
            if (projectedModel->inf(j)->name() == model->inf(i)->name()) {
                for (Size k = 0; k < projectedModel->stateVariables(CrossAssetModel::AssetType::INF, j); ++k) {
                    stateProcessProjection[projectedModel->pIdx(CrossAssetModel::AssetType::INF, j, k)] =
                        model->pIdx(CrossAssetModel::AssetType::INF, i, k);
                }
            }
        }
    }

    for (Size i = 0; i < model->components(CrossAssetModel::AssetType::CR); ++i) {
        for (Size j = 0; j < projectedModel->components(CrossAssetModel::AssetType::CR); ++j) {
            if (projectedModel->cr(j)->name() == model->cr(i)->name()) {
                for (Size k = 0; k < projectedModel->stateVariables(CrossAssetModel::AssetType::CR, j); ++k) {
                    stateProcessProjection[projectedModel->pIdx(CrossAssetModel::AssetType::CR, j, k)] =
                        model->pIdx(CrossAssetModel::AssetType::CR, i, k);
                }
            }
        }
    }

    for (Size i = 0; i < model->components(CrossAssetModel::AssetType::EQ); ++i) {
        for (Size j = 0; j < projectedModel->components(CrossAssetModel::AssetType::EQ); ++j) {
            if (projectedModel->eq(j)->name() == model->eq(i)->name()) {
                for (Size k = 0; k < projectedModel->stateVariables(CrossAssetModel::AssetType::EQ, j); ++k) {
                    stateProcessProjection[projectedModel->pIdx(CrossAssetModel::AssetType::EQ, j, k)] =
                        model->pIdx(CrossAssetModel::AssetType::EQ, i, k);
                }
            }
        }
    }

    return stateProcessProjection;
}

} // namespace QuantExt
