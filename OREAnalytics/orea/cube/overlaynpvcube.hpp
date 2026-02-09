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

#pragma once

#include <orea/cube/npvcube.hpp>

namespace ore {
namespace analytics {

class OverlayNPVCube : public NPVCube {
public:
    OverlayNPVCube(const QuantLib::ext::shared_ptr<NPVCube>& cube, const std::map<std::string, double>& pricingNpvs);
    Size numIds() const override;
    Size numDates() const override;
    Size samples() const override;
    Size depth() const override;
    const std::map<std::string, Size>& idsAndIndexes() const override;
    const std::vector<QuantLib::Date>& dates() const override;
    QuantLib::Date asof() const override;
    Real getT0(Size id, Size depth = 0) const override;
    void setT0(Real value, Size id, Size depth = 0) override;
    Real get(Size id, Size date, Size sample, Size depth = 0) const override;
    void set(Real value, Size id, Size date, Size sample, Size depth = 0) override;
    bool usesDoublePrecision() const override;

private:
    double correction(Size id, Size depth) const;

    QuantLib::ext::shared_ptr<NPVCube> cube_;
    std::vector<double> pricingNpvs_;
};

} // namespace analytics
} // namespace ore
