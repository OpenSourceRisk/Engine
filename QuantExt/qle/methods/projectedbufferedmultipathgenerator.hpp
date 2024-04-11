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

/*! \file projectedbufferedmultipathgenerator.hpp
    \brief multi path generator projecting paths from a buffered state processr
    \ingroup methods
*/

#pragma once

#include <qle/methods/multipathvariategenerator.hpp>

#include <qle/math/randomvariable.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>

namespace QuantExt {

class ProjectedBufferedMultiPathGenerator : public MultiPathGeneratorBase {
public:
    /*! If projection(j) = i for state process indices i from the projected process and j from the original process,
      then stateProcessProjection[i] = j, i.e. a state process component index from the projected model is mapped to
      the state process component index of the original model. */
    ProjectedBufferedMultiPathGenerator(
        const std::vector<Size>& stateProcessProjection,
        const QuantLib::ext::shared_ptr<std::vector<std::vector<QuantLib::Path>>>& bufferedPaths);
    const Sample<MultiPath>& next() const override;
    void reset() override;

private:
    const std::vector<Size> stateProcessProjection_;
    const QuantLib::ext::shared_ptr<std::vector<std::vector<QuantLib::Path>>> bufferedPaths_;
    Size maxTargetIndex_;
    mutable Size currentPath_;
    mutable Sample<MultiPath> next_;
};

} // namespace QuantExt
