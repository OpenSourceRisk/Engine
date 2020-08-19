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
    \brief get cross asset model with a subset of the original components
    \ingroup crossassetmodel
*/

#pragma once

#include <qle/models/crossassetmodel.hpp>

namespace QuantExt {

boost::shared_ptr<CrossAssetModel>
getProjectedCrossAssetModel(const boost::shared_ptr<CrossAssetModel>& model,
                            const std::vector<std::pair<CrossAssetModelTypes::AssetType, Size> >& selectedComponents,
                            std::vector<Size>& projectedStateProcessIndices);

} // namespace QuantExt
