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


#include <orea/cube/npvsensicube.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/serializationdate.hpp>

#include <ql/errors.hpp>

#include <boost/make_shared.hpp>
#include <boost/math/special_functions/relative_difference.hpp>

#include <fstream>
#include <iostream>
#include <vector>


namespace ore {
namespace analytics {

//! SensiCube stores only npvs not equal to the base npvs
template <typename T> class SensiCube : public NPVSensiCube {
public:
    SensiCube(const std::set<std::string>& ids, const QuantLib::Date& asof, QuantLib::Size samples, const T& t = T())
        : asof_(asof), dates_(1, asof), samples_(samples), t0Data_(ids.size(), t),
          tradeNPVs_(ids.size(), map<Size, T>()) {
        Size pos = 0;
        for (const auto& id : ids) {
            idIdx_[id] = pos++; 
        }
    }

    //! Return the length of each dimension
    QuantLib::Size numIds() const override { return idIdx_.size(); }
    QuantLib::Size samples() const override { return samples_; }

    //! Get the vector of ids for this cube
    const std::map<std::string, Size>& idsAndIndexes() const override { return idIdx_; }

    //! Get the vector of dates for this cube
    const std::vector<QuantLib::Date>& dates() const override { return dates_; }

    //! Return the asof date (T0 date)
    QuantLib::Date asof() const override { return asof_; }

    //! Get a T0 value from the cube
    Real getT0(Size i, Size) const override {
        this->check(i, 0, 0);
        return this->t0Data_[i];
    }

    //! Set a value in the cube
    void setT0(Real value, Size i, Size) override {
        this->check(i, 0, 0);
        this->t0Data_[i] = static_cast<T>(value);
    }

    //! Get a value from the cube
    Real get(Size i, Size j, Size k, Size) const override {
        this->check(i, j, k);

        auto itr = this->tradeNPVs_[i].find(k);
        if (itr != tradeNPVs_[i].end()) {
            return itr->second;
        } else {
            return this->t0Data_[i];
        }
    }

    //! Set a value in the cube
    void set(Real value, Size i, Size j, Size k, Size) override {
        this->check(i, j, k);
        T castValue = static_cast<T>(value);
        if (boost::math::epsilon_difference<T>(castValue, t0Data_[i]) > 42) {
            this->tradeNPVs_[i][k] = castValue;
            relevantScenarios_.insert(k);
        }
    }

    void remove(Size i) override {
        this->check(i,0,0);
        this->t0Data_[i] = 0.0;
        this->tradeNPVs_[i].clear();
    }

    void remove(Size i, Size k) override {
        this->check(i,0,k);
        this->tradeNPVs_[i].erase(k);
    }

    std::map<QuantLib::Size, QuantLib::Real> getTradeNPVs(QuantLib::Size i) const override { return tradeNPVs_[i]; }

    std::set<QuantLib::Size> relevantScenarios() const override { return relevantScenarios_; }

private:
    std::map<std::string, Size> idIdx_;
    QuantLib::Date asof_;
    std::vector<QuantLib::Date> dates_;
    QuantLib::Size samples_;

protected:
    std::vector<T> t0Data_;
    std::vector<std::map<QuantLib::Size, T>> tradeNPVs_;
    std::set<QuantLib::Size> relevantScenarios_;

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

} // namespace analytics
} // namespace ore
