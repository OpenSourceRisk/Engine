/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file orea/cube/jointnpvsensicube.hpp
    \brief join n sensi cubes in terms of stored ids
    \ingroup cube
*/

#pragma once

#include <orea/cube/npvsensicube.hpp>

#include <set>

namespace ore {
namespace analytics {

using namespace ore::analytics;

using QuantLib::Real;
using QuantLib::Size;

class JointNPVSensiCube : public NPVSensiCube {
public:
    /*! ctor for two input cubes */
    JointNPVSensiCube(const QuantLib::ext::shared_ptr<NPVSensiCube>& cube1, const QuantLib::ext::shared_ptr<NPVSensiCube>& cube2,
                      const std::set<std::string>& ids = {});

    /*! ctor for n input cubes
        If no ids are given, the ids in the input cubes define theids in the resulting cube, the ids must be unique
        in this case. If ids are given they define the ids in the output cube.
     */
    JointNPVSensiCube(const std::vector<QuantLib::ext::shared_ptr<NPVSensiCube>>& cubes,
                      const std::set<std::string>& ids = {});

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

    std::map<QuantLib::Size, QuantLib::Real> getTradeNPVs(Size tradeIdx) const override;
    std::set<QuantLib::Size> relevantScenarios() const override;

    void remove(Size id) override;
    void remove(Size id, Size sample) override;

private:
    const std::pair<QuantLib::ext::shared_ptr<NPVSensiCube>, Size>& cubeAndId(Size id) const;
    std::map<std::string, Size> idIdx_;
    std::vector<std::pair<QuantLib::ext::shared_ptr<NPVSensiCube>, Size>> cubeAndId_;
    const std::vector<QuantLib::ext::shared_ptr<NPVSensiCube>> cubes_;
};

} // namespace analytics
} // namespace ore
