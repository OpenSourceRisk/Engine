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

#include <fstream>
#include <vector>

#include <ql/errors.hpp>

#include <boost/make_shared.hpp>
#include <boost/serialization/vector.hpp>
#include <orea/cube/npvcube.hpp>
#include <ored/utilities/serializationdate.hpp>

namespace ore {
namespace analytics {
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;
using std::vector;

//! InMemoryCube stores the cube in memory using nested STL vectors
/*! InMemoryCube stores the cube in memory using nested STL vectors, this class is a template
 *  to allow both single and double precision implementations.
 *
 *  The use of nested STL vectors is adds a small memory overhead (~ 1 to 2%)

 \ingroup cube
 */
template <typename T> class InMemoryCubeBase : public NPVCube {
public:
    //! default ctor
    InMemoryCubeBase(const Date& asof, const vector<std::string>& ids, const vector<Date>& dates, Size samples,
                     const T& t = T())
        : asof_(asof), ids_(ids), dates_(dates), samples_(samples), t0Data_(ids.size(), t),
          data_(ids.size(), vector<vector<T>>(dates.size(), vector<T>(samples, t))) {
        QL_REQUIRE(ids.size() > 0, "InMemoryCube::InMemoryCube no ids specified");
        QL_REQUIRE(dates.size() > 0, "InMemoryCube::InMemoryCube no dates specified");
        QL_REQUIRE(samples > 0, "InMemoryCube::InMemoryCube samples must be > 0");
    }
    //! construct from file
    InMemoryCubeBase(const std::string& fileName) {
        load(fileName);
        QL_REQUIRE(numIds() > 0 && numDates() > 0 && samples() > 0,
                   "InMemoryCube::InMemoryCube failed to load from file " << fileName);
    }

    //! default constructor
    InMemoryCubeBase() {}

    //! load cube from an archive
    void load(const std::string& fileName) override {
        std::ifstream ifs(fileName.c_str(), std::fstream::binary);
        QL_REQUIRE(ifs.is_open(), "error opening file " << fileName);
        boost::archive::binary_iarchive ia(ifs);
        ia >> *this;
    }

    //! write cube to an archive
    void save(const std::string& fileName) const override {
        std::ofstream ofs(fileName.c_str(), std::fstream::binary);
        QL_REQUIRE(ofs.is_open(), "error opening file " << fileName);
        boost::archive::binary_oarchive oa(ofs);
        oa << *this;
    }

    //! Return the length of each dimension
    Size numIds() const override { return ids_.size(); }
    Size numDates() const override { return dates_.size(); }
    Size samples() const override { return samples_; }

    //! Get the vector of ids for this cube
    const std::vector<std::string>& ids() const override { return ids_; }
    //! Get the vector of dates for this cube
    const std::vector<QuantLib::Date>& dates() const override { return dates_; }

    //! Return the asof date (T0 date)
    QuantLib::Date asof() const override { return asof_; }

protected:
    void check(Size i, Size j, Size k, Size d) const {
        QL_REQUIRE(i < numIds(), "Out of bounds on ids (i=" << i << ")");
        QL_REQUIRE(j < numDates(), "Out of bounds on dates (j=" << j << ")");
        QL_REQUIRE(k < samples(), "Out of bounds on samples (k=" << k << ")");
        QL_REQUIRE(d < depth(), "Out of bounds on depth(d=" << d << ")");
    }

private:
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int) {
        ar& asof_;
        ar& ids_;
        ar& dates_;
        ar& samples_;
        ar& t0Data_;
        ar& data_;
    }

private:
    QuantLib::Date asof_;
    vector<std::string> ids_;
    vector<QuantLib::Date> dates_;
    Size samples_;

protected:
    vector<T> t0Data_;
    vector<vector<vector<T>>> data_;
};

//! InMemoryCube of fixed depth 1
/*! This implementation stores the type directly in an InMemoryCubeBase to avoid us having
 *  nested vectors all of length 1
 */
template <typename T> class InMemoryCube1 : public InMemoryCubeBase<T> {
public:
    //! ctor
    InMemoryCube1(const Date& asof, const vector<std::string>& ids, const vector<Date>& dates, Size samples)
        : InMemoryCubeBase<T>(asof, ids, dates, samples) {}

    //! default
    InMemoryCube1() {}

    //! Fixed depth
    Size depth() const override { return 1; }

    //! Get a T0 value from the cube
    Real getT0(Size i, Size d) const override {
        this->check(i, 0, 0, d);
        return this->t0Data_[i];
    }

    //! Set a value in the cube
    void setT0(Real value, Size i, Size d) override {
        this->check(i, 0, 0, d);
        this->t0Data_[i] = static_cast<T>(value);
    }

    //! Get a value from the cube
    Real get(Size i, Size j, Size k, Size d) const override {
        this->check(i, j, k, d);
        return this->data_[i][j][k];
    }

    //! Set a value in the cube
    void set(Real value, Size i, Size j, Size k, Size d) override {
        this->check(i, j, k, d);
        this->data_[i][j][k] = static_cast<T>(value);
    }
};

//! InMemoryCube of variable depth
/*! This implementation stores a vector an InMemoryCubeBase
 */
template <typename T> class InMemoryCubeN : public InMemoryCubeBase<vector<T>> {
public:
    //! ctor
    InMemoryCubeN(const Date& asof, const vector<std::string>& ids, const vector<Date>& dates, Size samples, Size depth)
        : InMemoryCubeBase<vector<T>>(asof, ids, dates, samples, vector<T>(depth, T())) {}

    //! default
    InMemoryCubeN() {}

    //! Depth
    Size depth() const override { return this->data_[0][0][0].size(); } // we don't want any members in this class

    //! Get a T0 value from the cube
    Real getT0(Size i, Size d) const override {
        this->check(i, 0, 0, d);
        return this->t0Data_[i][d];
    }

    //! Set a value in the cube
    void setT0(Real value, Size i, Size d) override {
        this->check(i, 0, 0, d);
        this->t0Data_[i][d] = static_cast<T>(value);
    }

    //! Get a value from the cube
    Real get(Size i, Size j, Size k, Size d) const override {
        this->check(i, j, k, d);
        return this->data_[i][j][k][d];
    }

    //! Set a value in the cube
    void set(Real value, Size i, Size j, Size k, Size d) override {
        this->check(i, j, k, d);
        this->data_[i][j][k][d] = static_cast<T>(value);
    }
};

//! InMemoryCube of depth 1 with single precision floating point numbers.
using SinglePrecisionInMemoryCube = InMemoryCube1<float>;

//! InMemoryCube of depth 1 with double precision floating point numbers.
using DoublePrecisionInMemoryCube = InMemoryCube1<double>;

//! InMemoryCube of depth N with single precision floating point numbers.
using SinglePrecisionInMemoryCubeN = InMemoryCubeN<float>;

//! InMemoryCube of depth N with double precision floating point numbers.
using DoublePrecisionInMemoryCubeN = InMemoryCubeN<double>;
} // namespace analytics
} // namespace ore
