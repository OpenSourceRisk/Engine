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

#include <orea/cube/jointnpvsensicube.hpp>

#include <ql/errors.hpp>

#include <numeric>
#include <set>

namespace ore {
namespace analytics {

JointNPVSensiCube::JointNPVSensiCube(const boost::shared_ptr<NPVSensiCube>& cube1,
                                     const boost::shared_ptr<NPVSensiCube>& cube2, const std::vector<std::string>& ids)
    : JointNPVSensiCube({cube1, cube2}, ids) {}

JointNPVSensiCube::JointNPVSensiCube(const std::vector<boost::shared_ptr<NPVSensiCube>>& cubes,
                                     const std::vector<std::string>& ids)
    : NPVSensiCube(), cubes_(cubes) {

    // check we have at least one input cube

    QL_REQUIRE(!cubes.empty(), "JointNPVSensiCube: at least one cube must be given");

    // check that the dimensions are consistent

    for (Size i = 1; i < cubes.size(); ++i) {
        QL_REQUIRE(cubes_[i]->numDates() == cubes_[0]->numDates(),
                   "JointNPVSensiCube: numDates do not match for cube #"
                       << i << " (" << cubes[i]->numDates() << " vs. cube #0 (" << cubes_[0]->numDates() << ")");
        QL_REQUIRE(cubes_[i]->samples() == cubes_[0]->samples(),
                   "JointNPVSensiCube: samples do not match for cube #"
                       << i << " (" << cubes_[i]->samples() << " vs. cube #0 (" << cubes_[0]->samples() << ")");
        QL_REQUIRE(cubes_[i]->depth() == cubes_[0]->depth(), "JointNPVSensiCube: depth do not match for cube #"
                                                                 << i << " (" << cubes_[i]->depth() << " vs. cube #0 ("
                                                                 << cubes[0]->depth() << ")");
    }

    // check that the input ids (if given) are unique

    std::set<std::string> tmp(ids.begin(), ids.end());
    QL_REQUIRE(tmp.size() == ids.size(), "JoingNPVCube: given input ids (" << ids.size() << ") are not unique (got "
                                                                           << tmp.size() << " unique id)");

    // build list of result cube ids

    if (!ids.empty()) {
        // if ids are given, these define the order in the result cube
        ids_ = ids;
    } else {
        // otherwise the ids in the source cubes define the order in the result cube
        for (Size i = 0; i < cubes_.size(); ++i) {
            const std::vector<std::string>& tmp = cubes_[i]->ids();
            for (auto const& id : tmp) {
                auto f = std::find(ids_.begin(), ids_.end(), id);
                QL_REQUIRE(f == ids_.end(),
                           "JointNPVSensiCube: input cubes have duplicate id '" << id << "', this is not allowed");
                if (f == ids_.end())
                    ids_.push_back(id);
            }
        }
    }

    // populate cubeAndId_ vector which is the basis for the lookup

    cubeAndId_.resize(ids_.size());
    for (Size i = 0; i < ids_.size(); ++i) {
        bool foundUniqueId = false;
        for (auto const& c : cubes_) {
            const std::vector<std::string>& tmp = c->ids();
            for (Size j = 0; j < tmp.size(); ++j) {
                if (ids_[i] == tmp[j]) {
                    QL_REQUIRE(!foundUniqueId, "JointNPVSensiCube: internal error, found id '"
                                                   << ids_[i] << "' in more than one input cubes");
                    cubeAndId_[i] = std::make_pair(c, j);
                    foundUniqueId = true;
                }
            }
        }
        // internal consistency checks
        QL_REQUIRE(foundUniqueId, "JointNPVSensiCube: internal error, got no input cube for id '" << ids_[i] << "'");
    }
}

Size JointNPVSensiCube::numIds() const { return ids_.size(); }

Size JointNPVSensiCube::numDates() const { return cubes_[0]->numDates(); }

Size JointNPVSensiCube::samples() const { return cubes_[0]->samples(); }

Size JointNPVSensiCube::depth() const { return cubes_[0]->depth(); }

const std::vector<std::string>& JointNPVSensiCube::ids() const { return ids_; }

const std::vector<QuantLib::Date>& JointNPVSensiCube::dates() const { return cubes_[0]->dates(); }

QuantLib::Date JointNPVSensiCube::asof() const { return cubes_[0]->asof(); }

const std::pair<boost::shared_ptr<NPVSensiCube>, Size>& JointNPVSensiCube::cubeAndId(Size id) const {
    QL_REQUIRE(id < cubeAndId_.size(),
               "JointNPVSensiCube: id (" << id << ") out of range, have " << cubeAndId_.size() << " ids");
    return cubeAndId_[id];
}

Real JointNPVSensiCube::getT0(Size id, Size depth) const {
    const auto& c = cubeAndId(id);
    return c.first->getT0(c.second, depth);
}

void JointNPVSensiCube::setT0(Real value, Size id, Size depth) {
    const auto& c = cubeAndId(id);
    c.first->setT0(value, c.second, depth);
}

Real JointNPVSensiCube::get(Size id, Size date, Size sample, Size depth) const {
    const auto& c = cubeAndId(id);
    return c.first->get(c.second, date, sample, depth);
}

void JointNPVSensiCube::set(Real value, Size id, Size date, Size sample, Size depth) {
    const auto& c = cubeAndId(id);
    c.first->set(value, c.second, date, sample, depth);
}

std::map<QuantLib::Size, QuantLib::Real> JointNPVSensiCube::getTradeNPVs(Size tradeIdx) const {
    const auto& c = cubeAndId(tradeIdx);
    return c.first->getTradeNPVs(c.second);
}

std::set<QuantLib::Size> JointNPVSensiCube::relevantScenarios() const {
    std::set<QuantLib::Size> tmp;
    for (auto const& c : cubes_) {
        auto r = c->relevantScenarios();
        tmp.insert(r.begin(), r.end());
    }
    return tmp;
}

void JointNPVSensiCube::remove(Size id) {
    const auto& c = cubeAndId(id);
    c.first->remove(c.second);
}

void JointNPVSensiCube::remove(Size id, Size sample) {
    const auto& c = cubeAndId(id);
    c.first->remove(c.second, sample);
}

} // namespace analytics
} // namespace ore
