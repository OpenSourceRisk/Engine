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

/*! \file orea/cube/inmemorycube.hpp
    \brief A cube implementation that stores the cube in memory
    \ingroup cube
*/

#pragma once

#include <orea/cube/inmemorycubeopt.hpp>

// #include <fstream>
// #include <vector>

// #include <ql/errors.hpp>

// #include <boost/make_shared.hpp>
// #include <orea/cube/npvcube.hpp>
// #include <set>

namespace ore {
namespace analytics {

// use drop-in replacement InMemoryCubeOpt, which has a better memory footprint

template <typename T> using InMemoryCubeBase = InMemoryCubeOpt<T>;
template <typename T> using InMemoryCubeN = InMemoryCubeOpt<T>;
template <typename T> using InMemoryCube1 = InMemoryCubeOpt<T>;

using SinglePrecisionInMemoryCube = InMemoryCubeOpt<float>;
using SinglePrecisionInMemoryCubeN = InMemoryCubeOpt<float>;
using DoublePrecisionInMemoryCube = InMemoryCubeOpt<double>;
using DoublePrecisionInMemoryCubeN = InMemoryCubeOpt<double>;

// using QuantLib::Date;
// using QuantLib::Real;
// using QuantLib::Size;
// using std::vector;

//! InMemoryCube stores the cube in memory using nested STL vectors
/*! InMemoryCube stores the cube in memory using nested STL vectors, this class is a template
 *  to allow both single and double precision implementations.
 *
 *  The use of nested STL vectors is adds a small memory overhead (~ 1 to 2%)

 \ingroup cube
 */
// template <typename T> class InMemoryCubeBase : public NPVCube {
// public:
//     //! default ctor
//     InMemoryCubeBase(const Date& asof, const std::set<std::string>& ids, const vector<Date>& dates, Size samples,
//                      Size depth, const T& t = T())
//         : asof_(asof), dates_(dates), samples_(samples), depth_(depth), t0Data_(depth, vector<T>(ids.size(), t)),
//           data_(depth, vector<vector<vector<T>>>(dates.size(), vector(ids.size(), vector<T>(samples, t)))) {
//         QL_REQUIRE(ids.size() > 0, "InMemoryCube::InMemoryCube no ids specified");
//         QL_REQUIRE(dates.size() > 0, "InMemoryCube::InMemoryCube no dates specified");
//         QL_REQUIRE(samples > 0, "InMemoryCube::InMemoryCube samples must be > 0");
//         QL_REQUIRE(depth > 0, "InMemoryCube::InMemoryCube depth must be > 0");
//         size_t pos = 0;
//         for (const auto& id : ids) {
//             idIdx_[id] = pos++;
//         }
        
//     }

//     //! default constructor
//     InMemoryCubeBase() {}

//     //! Return the length of each dimension
//     Size numIds() const override { return idIdx_.size(); }
//     Size numDates() const override { return dates_.size(); }
//     Size samples() const override { return samples_; }
//     Size depth() const override { return depth_; }

//     //! Return a map of all ids and their position in the cube
//     const std::map<std::string, Size>& idsAndIndexes() const override { return idIdx_; }
//     //! Get the vector of dates for this cube
//     const std::vector<QuantLib::Date>& dates() const override { return dates_; }

//     //! Return the asof date (T0 date)
//     QuantLib::Date asof() const override { return asof_; }

// protected:
//     void check(Size i, Size j, Size k, Size d) const {
//         QL_REQUIRE(i < numIds(), "Out of bounds on ids (i=" << i << ", numIds=" << numIds() << ")");
//         QL_REQUIRE(j < numDates(), "Out of bounds on dates (j=" << j << ", numDates=" << numDates() << ")");
//         QL_REQUIRE(k < samples(), "Out of bounds on samples (k=" << k << ", samples=" << samples() << ")");
//         QL_REQUIRE(d < depth(), "Out of bounds on depth (d=" << d << ", depth=" << depth() << ")");
//     }

//     QuantLib::Date asof_;
//     vector<QuantLib::Date> dates_;
//     Size samples_;
//     Size depth_;
//     vector<vector<T>> t0Data_;
//     vector<vector<vector<vector<T>>>> data_;

//     std::map<std::string, Size> idIdx_;
// };

//! InMemoryCube of variable depth
/*! This implementation stores a vector an InMemoryCubeBase
 */
// template <typename T> class InMemoryCubeN : public InMemoryCubeBase<T> {
// public:
//     //! ctor
//     InMemoryCubeN(const Date& asof, const std::set<std::string>& ids, const vector<Date>& dates, Size samples, Size depth,
//                   const T& t = T())
//         : InMemoryCubeBase<T>(asof, ids, dates, samples, depth, t) {}

//     //! default
//     InMemoryCubeN() {}

//     //! Get a T0 value from the cube
//     virtual Real getT0(Size i, Size d) const override {
//         this->check(i, 0, 0, d);
//         return this->t0Data_[d][i];
//     }

//     //! Set a value in the cube
//     virtual void setT0(Real value, Size i, Size d) override {
//         this->check(i, 0, 0, d);
//         this->t0Data_[d][i] = static_cast<T>(value);
//     }

//     //! Get a value from the cube
//     Real get(Size i, Size j, Size k, Size d) const override {
//         this->check(i, j, k, d);
//         return this->data_[d][j][i][k];
//     }

//     //! Set a value in the cube
//     void set(Real value, Size i, Size j, Size k, Size d) override {
//         this->check(i, j, k, d);
//         this->data_[d][j][i][k] = static_cast<T>(value);
//     }
// };

//! InMemoryCube of fixed depth 1
// template <typename T> class InMemoryCube1 : public InMemoryCubeN<T> {
// public:
//     //! ctor
//     InMemoryCube1(const Date& asof, const std::set<std::string>& ids, const vector<Date>& dates, Size samples,
//                   const T& t = T())
//         : InMemoryCubeN<T>(asof, ids, dates, samples, 1, t) {}

//     //! default
//     InMemoryCube1() {}
// };

//! InMemoryCube of depth 1 with single precision floating point numbers.
// using SinglePrecisionInMemoryCube = InMemoryCube1<float>;

//! InMemoryCube of depth 1 with double precision floating point numbers.
// using DoublePrecisionInMemoryCube = InMemoryCube1<double>;

//! InMemoryCube of depth N with single precision floating point numbers.
// using SinglePrecisionInMemoryCubeN = InMemoryCubeN<float>;

//! InMemoryCube of depth N with double precision floating point numbers.
// using DoublePrecisionInMemoryCubeN = InMemoryCubeN<double>;
} // namespace analytics
} // namespace ore
