/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file orea/cube/npvsensicube.hpp
    \brief An NPV cube for storing NPVs resulting from risk factor shifts
    \ingroup cube
*/

#pragma once

#include <map>
#include <orea/cube/npvcube.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>
#include <set>

namespace ore {
namespace analytics {
//! NPVSensiCube class stores NPVs resulting from risk factor shifts on an as of date.
/*! This class is a restriction of the NPVCube to a grid of values where the ids are
    trade IDs and the samples are risk factor shifts

    \ingroup cube
 */
class NPVSensiCube : public NPVCube {
public:
    using NPVCube::get;
    using NPVCube::set;

    //! Number of dates in the NPVSensiCube is exactly one i.e. the as of date
    QuantLib::Size numDates() const override { return 1; }

    //! The depth in the NPVSensiCube is exactly one
    QuantLib::Size depth() const override { return 1; }

    //! Convenience method to get a value from the cube using \p id and \p sample only
    Real get(QuantLib::Size id, QuantLib::Size sample) const { return this->get(id, 0, sample, 0); }

    //! Convenience method to get a value from the cube using \p id and \p sample only
    Real get(const std::string& id, QuantLib::Size sample) const { return this->get(id, asof(), sample, 0); };

    //! Convenience method to set a value in the cube using \p id and \p sample only
    void set(QuantLib::Real value, QuantLib::Size id, QuantLib::Size sample) { this->set(value, id, 0, sample, 0); }

    //! Convenience method to set a value in the cube using \p id and \p sample only
    void set(QuantLib::Real value, const std::string& id, QuantLib::Size sample) {
        this->set(value, id, asof(), sample, 0);
    }

    //! Return the index of the trade in the cube
    Size getTradeIndex(const std::string& tradeId) const { return index(tradeId); }

    /*! Return a map for the trade ID at index \p tradeIdx where the map key is the index of the
        risk factor shift and the map value is the NPV under that shift
    */
    virtual std::map<QuantLib::Size, QuantLib::Real> getTradeNPVs(Size tradeIdx) const = 0;

    /*! Return a map for the \p tradeId where the map key is the index of the
        risk factor shift and the map value is the NPV under that shift
    */
    std::map<QuantLib::Size, QuantLib::Real> getTradeNPVs(const std::string& tradeId) const {
        return getTradeNPVs(index(tradeId));
    }

    /*! Return the set of scenario indices with non-zero result */
    virtual std::set<QuantLib::Size> relevantScenarios() const = 0;
};

} // namespace analytics
} // namespace ore
