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
    QuantLib::Size numIds() const override;
    QuantLib::Size numDates() const override;
    QuantLib::Size samples() const override;
    QuantLib::Size depth() const override;

    const std::map<std::string, QuantLib::Size>& idsAndIndexes() const override;
    const std::vector<QuantLib::Date>& dates() const override;
    QuantLib::Date asof() const override;

    QuantLib::Real getT0(QuantLib::Size id, QuantLib::Size depth = 0) const override;
    void setT0(QuantLib::Real value, QuantLib::Size id, QuantLib::Size depth = 0) override;

    QuantLib::Real get(QuantLib::Size id, QuantLib::Size date, QuantLib::Size sample,
                       QuantLib::Size depth = 0) const override;
    void set(QuantLib::Real value, QuantLib::Size id, QuantLib::Size date, QuantLib::Size sample,
             QuantLib::Size depth = 0) override;

    std::map<QuantLib::Size, QuantLib::Real> getTradeNPVs(QuantLib::Size tradeIdx) const override;
    std::set<QuantLib::Size> relevantScenarios() const override;

    void removeT0(QuantLib::Size id) override;
    void remove(QuantLib::Size id, QuantLib::Size sample, bool useT0) override;

    bool usesDoublePrecision() const override;

private:
    const std::pair<QuantLib::ext::shared_ptr<NPVSensiCube>, QuantLib::Size>& cubeAndId(QuantLib::Size id) const;
    std::map<std::string, QuantLib::Size> idIdx_;
    std::vector<std::pair<QuantLib::ext::shared_ptr<NPVSensiCube>, QuantLib::Size>> cubeAndId_;
    const std::vector<QuantLib::ext::shared_ptr<NPVSensiCube>> cubes_;
};

} // namespace analytics
} // namespace ore
