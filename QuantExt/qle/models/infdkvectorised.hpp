/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/models/lgmvectorised.hpp
    \brief vectorised lgm model calculations
    \ingroup models
*/

#pragma once


#include <qle/math/randomvariable.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <ql/indexes/interestrateindex.hpp>

namespace QuantExt {

using namespace QuantLib;

class InfDkVectorised {
public:
    InfDkVectorised(const QuantLib::ext::shared_ptr<CrossAssetModel>& cam);

    std::pair<RandomVariable, RandomVariable> infdkI(const Size i, const Time t, const Time T, const RandomVariable& z,
                                                     const RandomVariable& y, bool indexIsInterpolated) const;

private:
    const QuantLib::ext::shared_ptr<CrossAssetModel> cam_;
};

} // namespace QuantExt
