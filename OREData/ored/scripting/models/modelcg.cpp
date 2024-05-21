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

#include <ored/scripting/models/modelcg.hpp>

#include <qle/ad/computationgraph.hpp>

namespace ore {
namespace data {

ModelCG::ModelCG(const QuantLib::Size n) : n_(n) { g_ = QuantLib::ext::make_shared<QuantExt::ComputationGraph>(); }

std::size_t ModelCG::dt(const Date& d1, const Date& d2) const {
    return cg_const(*g_, QuantLib::ActualActual(QuantLib::ActualActual::ISDA).yearFraction(d1, d2));
}

} // namespace data
} // namespace ore
