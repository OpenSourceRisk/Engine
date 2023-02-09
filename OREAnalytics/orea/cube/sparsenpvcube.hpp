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

/*! \file orea/cube/sparsenpvcube.hpp
    \brief in memory cube, storing only non-zero entries for (id, date, depth)
*/

#pragma once

#include <orea/cube/npvcube.hpp>

#include <map>
#include <set>

namespace ore {
namespace analytics {
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;

template <typename T> class SparseNpvCube : public ore::analytics::NPVCube {
public:
    SparseNpvCube();
    SparseNpvCube(const Date& asof, const std::set<std::string>& ids, const std::vector<Date>& dates, Size samples,
                  Size depth, const T& t = T());
    Size numIds() const override;
    Size numDates() const override;
    Size samples() const override;
    Size depth() const override;
    Date asof() const override;
    const std::map<std::string, Size>& idsAndIndexes() const override;
    const std::vector<QuantLib::Date>& dates() const override;
    Real getT0(Size i, Size d) const override;
    void setT0(Real value, Size i, Size d) override;
    Real get(Size i, Size j, Size k, Size d) const override;
    void set(Real value, Size i, Size j, Size k, Size d) override;

private:
    void check(Size i, Size j, Size k, Size d) const;
    Size pos(Size i, Size j, Size d) const;

    QuantLib::Date asof_;
    std::map<std::string, Size> ids_;
    std::vector<QuantLib::Date> dates_;
    Size samples_;
    Size depth_;
    std::map<Size, std::vector<T>> data_;
};

using SinglePrecisionSparseNpvCube = SparseNpvCube<float>;
using RealPrecisionSparseNpvCube = SparseNpvCube<Real>;

} // namespace analytics
} // namespace ore
