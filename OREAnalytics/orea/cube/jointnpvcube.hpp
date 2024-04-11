/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file orea/cube/jointnpvcube.hpp
    \brief join n cubes in terms of stored ids
    \ingroup cube
*/

#pragma once

#include <orea/cube/npvcube.hpp>

#include <set>

namespace ore {
namespace analytics {

using namespace ore::analytics;

using QuantLib::Real;
using QuantLib::Size;

class JointNPVCube : public NPVCube {
public:
    /*! ctor for two input cubes */
    JointNPVCube(
        const QuantLib::ext::shared_ptr<NPVCube>& cube1, const QuantLib::ext::shared_ptr<NPVCube>& cube2,
        const std::set<std::string>& ids = {}, const bool requireUniqueIds = true,
        const std::function<Real(Real a, Real x)>& accumulator = [](Real a, Real x) { return a + x; },
        const Real accumulatorInit = 0.0);

    /*! ctor for n input cubes
        - If no ids are given, the ids in the input cubes define the ids in the resulting cube. The order is
       lexicographic. If ids are given they define the ids in the output cube.
        - If requireUniqueIds is true, there must be no duplicate ids in the input cubes. If requireUniqueIds is false,
          they may be duplicate ids in which case get() will return an aggregate of the entries in the input cubes over
       the matching ids using the accumulator function and accumulator initialization.
        - If one id in the result cube corresponds to several input cubes, it is not allowed to call set on this id,
          this will result in an exception.
     */
    JointNPVCube(
        const std::vector<QuantLib::ext::shared_ptr<NPVCube>>& cubes, const std::set<std::string>& ids = {},
        const bool requireUniqueIds = true,
        const std::function<Real(Real a, Real x)>& accumulator = [](Real a, Real x) { return a + x; },
        const Real accumulatorInit = 0.0);

    //! Return the length of each dimension
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

private:
    std::set<std::pair<QuantLib::ext::shared_ptr<NPVCube>, Size>> cubeAndId(Size id) const;

    const std::vector<QuantLib::ext::shared_ptr<NPVCube>> cubes_;
    const std::function<Real(Real a, Real x)> accumulator_;
    const Real accumulatorInit_;

    std::map<std::string, Size> idIdx_;
    std::vector<std::set<std::pair<QuantLib::ext::shared_ptr<NPVCube>, Size>>> cubeAndId_;
};

} // namespace analytics
} // namespace ore
