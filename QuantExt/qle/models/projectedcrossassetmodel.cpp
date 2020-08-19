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

boost::shared_ptr<CrossAssetModel>
getProjectedCrossAssetModel(const boost::shared_ptr<CrossAssetModel>& model,
                            const std::vector<std::pair<CrossAssetModelTypes::AssetType, Size> >& selectedComponents,
                            std::vector<Size>& projectedStateProcessIndices) {

    // vectors holding the selected parametrizations and associated indices in the correlation matrix

    std::vector<boost::shared_ptr<Parametrization> > parametrizations;
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

    return boost::make_shared<CrossAssetModel>(parametrizations, correlation, model->salvagingAlgorithm());
}

} // namespace QuantExt
