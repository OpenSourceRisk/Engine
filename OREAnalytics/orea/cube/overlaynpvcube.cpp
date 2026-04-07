/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file orea/cube/overlaynpvcube.hpp
    \brief npv cube corrected by difference pricing t0 npv and sim t0 npv
    \ingroup cube
*/

#include <orea/cube/overlaynpvcube.hpp>

namespace ore {
namespace analytics {

OverlayNPVCube::OverlayNPVCube(const QuantLib::ext::shared_ptr<NPVCube>& cube,
                               const std::map<std::string, double>& pricingNpvs)
    : cube_(cube) {
    pricingNpvs_.resize(cube_->numIds());
    for (auto const& [id, index] : cube_->idsAndIndexes()) {
        QL_REQUIRE(index < pricingNpvs_.size(), "OverlayNPVCube(): numIds (" << cube_->numIds()
                                                                             << ") does not cover index (" << index
                                                                             << ") for id " << id);
        if (auto p = pricingNpvs.find(id); p != pricingNpvs.end())
            pricingNpvs_[index] = p->second;
        else {
            QL_FAIL("OverlayNPVCube(): no pricingNpv given for id " << id);
        }
    }
}

Size OverlayNPVCube::numIds() const { return cube_->numIds(); }

Size OverlayNPVCube::numDates() const { return cube_->numDates(); }

Size OverlayNPVCube::samples() const { return cube_->samples(); }

Size OverlayNPVCube::depth() const { return cube_->depth(); }

const std::map<std::string, Size>& OverlayNPVCube::idsAndIndexes() const { return cube_->idsAndIndexes(); }

const std::vector<QuantLib::Date>& OverlayNPVCube::dates() const { return cube_->dates(); }

QuantLib::Date OverlayNPVCube::asof() const { return cube_->asof(); }

Real OverlayNPVCube::getT0(Size id, Size depth) const { return cube_->getT0(id, depth) + correction(id, depth); }

void OverlayNPVCube::setT0(Real value, Size id, Size depth) { cube_->setT0(value - correction(id, depth), id, depth); }

Real OverlayNPVCube::get(Size id, Size date, Size sample, Size depth) const {
    return cube_->get(id, date, sample, depth) + correction(id, depth);
}

void OverlayNPVCube::set(Real value, Size id, Size date, Size sample, Size depth) {
    cube_->set(value - correction(id, depth), id, date, sample, depth);
}

bool OverlayNPVCube::usesDoublePrecision() const { return cube_->usesDoublePrecision(); }

double OverlayNPVCube::correction(Size id, Size depth) const {
    return depth == 0 ? pricingNpvs_[id] - cube_->getT0(id, depth) : 0.0;
}

} // namespace analytics
} // namespace ore
