/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file scenario/aggregationscenariodata.hpp
    \brief this class holds data associated to scenarios
    \ingroup scenario
*/

#pragma once

#include <ql/errors.hpp>
#include <ql/types.hpp>
#include <ql/patterns/observable.hpp>

#include <fstream>
#include <map>
#include <vector>

namespace ore {
namespace analytics {
using QuantLib::Real;
using QuantLib::Size;
using std::map;
using std::string;
using std::vector;

enum class AggregationScenarioDataType : unsigned int {
    IndexFixing = 0,
    FXSpot = 1,
    Numeraire = 2,
    CreditState = 3,
    SurvivalWeight = 4,
    RecoveryRate = 5,
    Generic = 6
};

//! Container for storing simulated market data
/*! The indexes for dates and samples are (by convention) the
    same as in the npv cube

        \ingroup scenario
*/
class AggregationScenarioData : public QuantLib::Observable {
public:
    //! default ctor
    AggregationScenarioData() : dIndex_(0), sIndex_(0) {}
    //! Do not allow copying
    AggregationScenarioData(AggregationScenarioData&) = delete;
    AggregationScenarioData& operator=(AggregationScenarioData const&) = delete;
    //! dtor
    virtual ~AggregationScenarioData() {}

    //! Return the length of each dimension
    virtual Size dimDates() const = 0;
    virtual Size dimSamples() const = 0;

    //! Check whether data is available for the given type
    virtual bool has(const AggregationScenarioDataType& type, const string& qualifier = "") const = 0;

    //! Get a value from the cube
    virtual Real get(Size dateIndex, Size sampleIndex, const AggregationScenarioDataType& type,
                     const string& qualifier = "") const = 0;
    //! Set a value in the cube
    virtual void set(Size dateIndex, Size sampleIndex, Real value, const AggregationScenarioDataType& type,
                     const string& qualifier = "") = 0;

    // Get available keys (type, qualifier)
    virtual std::vector<std::pair<AggregationScenarioDataType, std::string>> keys() const = 0;

    //! Set a value in the cube, assumes normal traversal of the cube (dates then samples)
    virtual void set(Real value, const AggregationScenarioDataType& type, const string& qualifier = "") {
        set(dIndex_, sIndex_, value, type, qualifier);
    }
    //! Go to the next point on the cube
    /*! Go to the next point on the cube, assumes we do date, then samples
     */
    virtual void next() {
        if (++dIndex_ == dimDates()) {
            dIndex_ = 0;
            sIndex_++;
        }
    }

private:
    Size dIndex_, sIndex_;
};

//! A concrete in memory implementation of AggregationScenarioData
/*! \ingroup scenario
 */
class InMemoryAggregationScenarioData : public AggregationScenarioData {
public:
    InMemoryAggregationScenarioData() : AggregationScenarioData(), dimDates_(0), dimSamples_(0) {}
    InMemoryAggregationScenarioData(Size dimDates, Size dimSamples)
        : AggregationScenarioData(), dimDates_(dimDates), dimSamples_(dimSamples) {}
    Size dimDates() const override { return dimDates_; }
    Size dimSamples() const override { return dimSamples_; }

    bool has(const AggregationScenarioDataType& type, const string& qualifier = "") const override {
        return data_.find(std::make_pair(type, qualifier)) != data_.end();
    }

    /*! might throw (the underlying's container exception),
      if type is not known */
    Real get(Size dateIndex, Size sampleIndex, const AggregationScenarioDataType& type,
             const string& qualifier = "") const override {
        check(dateIndex, sampleIndex, type, qualifier);
        return data_.at(std::make_pair(type, qualifier))[dateIndex][sampleIndex];
    }

    std::vector<std::pair<AggregationScenarioDataType, std::string>> keys() const override {
        std::vector<std::pair<AggregationScenarioDataType, std::string>> res;
        for (auto const& k : data_)
            res.push_back(k.first);
        return res;
    }

    void set(Size dateIndex, Size sampleIndex, Real value, const AggregationScenarioDataType& type,
             const string& qualifier = "") override {
        check(dateIndex, sampleIndex, type, qualifier);
        auto key = std::make_pair(type, qualifier);
        auto it = data_.find(key);
        if (it == data_.end()) {
            data_.insert(make_pair(key, vector<vector<Real>>(dimDates_, vector<Real>(dimSamples_, 0.0))));
        }
        data_[key][dateIndex][sampleIndex] = value;
    }

private:
    void check(Size dateIndex, Size sampleIndex, const AggregationScenarioDataType& type,
               const string& qualifier) const {
        QL_REQUIRE(dateIndex < dimDates_, "dateIndex (" << dateIndex << ") out of range 0..." << dimDates_ - 1);
        QL_REQUIRE(sampleIndex < dimSamples_,
                   "sampleIndex (" << sampleIndex << ") out of range 0..." << dimSamples_ - 1);
        return;
    }
    Size dimDates_, dimSamples_;
    map<std::pair<AggregationScenarioDataType, string>, vector<vector<Real>>> data_;
};

inline std::ostream& operator<<(std::ostream& out, const AggregationScenarioDataType& t) {
    switch (t) {
    case AggregationScenarioDataType::IndexFixing:
        return out << "IndexFixing";
    case AggregationScenarioDataType::FXSpot:
        return out << "FXSpot";
    case AggregationScenarioDataType::Numeraire:
        return out << "Numeraire";
    case AggregationScenarioDataType::CreditState:
        return out << "CreditState";
    case AggregationScenarioDataType::SurvivalWeight:
        return out << "SurvivalWeight";
    case AggregationScenarioDataType::RecoveryRate:
        return out << "RecoveryRate";
    case AggregationScenarioDataType::Generic:
        return out << "Generic";
    default:
        return out << "Unknown aggregation scenario data type";
    }
}

} // namespace analytics
} // namespace ore
