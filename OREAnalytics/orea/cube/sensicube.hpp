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

/*! \file orea/cube/sensicube.hpp
    \brief A cube implementation that stores the cube in memory
    \ingroup cube
*/
#pragma once

#include <fstream>
#include <iostream>
#include <ql/errors.hpp>
#include <vector>

#include <boost/make_shared.hpp>
#include <boost/serialization/vector.hpp>
#include <orea/cube/npvsensicube.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/serializationdate.hpp>

namespace ore {
namespace analytics {

//! Naieve concrete implementation of NPVSensiCube
template <typename T>
class SensiCube : public ore::analytics::NPVSensiCube {
public:
    SensiCube(const std::vector<std::string>& ids, const QuantLib::Date& asof, QuantLib::Size samples, const T& t = T()) :
        ids_(ids), asof_(asof), dates_(1, asof_), samples_(samples), t0Data_(ids.size(), t), tradeNPVs_(ids.size(), std::map<Size,T>()) {
    }

    //! load cube from an archive
    void load(const std::string& fileName) override {
        std::ifstream ifs(fileName.c_str(), std::fstream::binary);
        QL_REQUIRE(ifs.is_open(), "error opening file " << fileName);
        boost::archive::binary_iarchive ia(ifs);
        ia >> *this;
    }

    
    //! write cube to an archive
    void save(const std::string& fileName) const override{
        std::ofstream ofs(fileName.c_str(), std::fstream::binary);
        QL_REQUIRE(ofs.is_open(), "error opening file " << fileName);
        boost::archive::binary_oarchive oa(ofs);
        oa << *this;
    }
    
    //! Return the length of each dimension
    QuantLib::Size numIds() const override { return ids_.size(); }
    QuantLib::Size samples() const override { return samples_; }

    //! Get the vector of ids for this cube
    const std::vector<std::string>& ids() const override { return ids_; }
    
    //! Get the vector of dates for this cube
    const std::vector<QuantLib::Date>& dates() const override { return dates_; }

    //! Return the asof date (T0 date)
    QuantLib::Date asof() const override { return asof_; }

    //! Get a T0 value from the cube
    Real getT0(QuantLib::Size i, QuantLib::Size) const override {
        this->check(i, 0, 0);
        return this->t0Data_[i];
    }
    
    //! Set a value in the cube
    void setT0(QuantLib::Real value, QuantLib::Size i, QuantLib::Size) override  {
        this->check(i, 0, 0);
        this->t0Data_[i] = static_cast<T>(value);
    }
    
    //! Get a value from the cube
    Real get(QuantLib::Size i, QuantLib::Size j, QuantLib::Size k, QuantLib::Size) const override {
        this->check(i, j, k);

        auto itr = this->tradeNPVs_[i].find(k);
        if (itr != tradeNPVs_[i].end()) {
            return itr->second; 
        } else {
            return this->t0Data_[i];
        }
    }

    //! Set a value in the cube
    void set(QuantLib::Real value, QuantLib::Size i, QuantLib::Size j, QuantLib::Size k, QuantLib::Size ) override {
        this->check(i, j, k);
        this->tradeNPVs_[i][k] = static_cast<T>(value);
    }

    const std::map<QuantLib::Size, QuantLib::Real>& getTradeNPVs(QuantLib::Size i) const override {
        return tradeNPVs_[i];
    }
    
private:
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int) {
        ar& ids_;
        ar& asof_;
        ar& samples_;
        ar& t0Data_;
        ar& tradeNPVs_;
    }

    std::vector<std::string> ids_;
    QuantLib::Date asof_;
    std::vector<QuantLib::Date> dates_;
    QuantLib::Size samples_;

protected:
    std::vector<T> t0Data_;
    std::vector<std::map<QuantLib::Size, T>> tradeNPVs_;

    void check(QuantLib::Size i, QuantLib::Size j, QuantLib::Size k) const {
        QL_REQUIRE(i < numIds(), "Out of bounds on ids (i=" << i << ")");
        QL_REQUIRE(j < depth(), "Out of bounds on depth (j=" << j << ")");
        QL_REQUIRE(k < samples(), "Out of bounds on samples (k=" << k << ")");
    }
};

//! Sensi cube with single precision floating point numbers.
using SinglePrecisionSensiCube = SensiCube<float>;

//! Sensi cube with double precision floating point numbers.
using DoublePrecisionSensiCube = SensiCube<double>;

}
}
