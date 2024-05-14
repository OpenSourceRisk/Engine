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

/*! \file models/projectedcrossassetmodel.hpp
    \brief cross asset model projection utils
    \ingroup crossassetmodel
*/

#pragma once

#include <qle/models/crossassetmodel.hpp>

namespace QuantExt {

/* Input is a "big" model from which we select components to build a "small" model.
   The projectedStateProcessIndices vector size is the number of state variables of the small model and maps
   a state process component index of the small model to the corresponding index of the big model. */
QuantLib::ext::shared_ptr<CrossAssetModel>
getProjectedCrossAssetModel(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
                            const std::vector<std::pair<CrossAssetModel::AssetType, Size>>& selectedComponents,
                            std::vector<Size>& projectedStateProcessIndices);

/* Input is a "big" and a "small" (projected) model, where the small model's components are assumed to be a subset of
  the big model's components. The result is a vector of size equal to the number of state variables of the small model.
  Each state process component index of the small model is mapped to the corresponding index of the big model. */
std::vector<Size> getStateProcessProjection(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
                                            const QuantLib::ext::shared_ptr<CrossAssetModel>& projectedModel);

} // namespace QuantExt
