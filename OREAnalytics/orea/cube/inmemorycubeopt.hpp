/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file orea/cube/inmemorycubeopt.hpp
    \brief cube storing data in memory with some space optimization
    \ingroup cube
*/

#pragma once

#include <orea/cube/npvcube.hpp>

#include <ql/errors.hpp>

#include <boost/make_shared.hpp>

#include <map>
#include <vector>

namespace ore {
namespace analytics {

using QuantLib::Date;
using QuantLib::Size;

template <typename T> class InMemoryCubeOpt : public NPVCube {
private:
    // number of dates in a block
    static constexpr Size N = 5;

public:
    InMemoryCubeOpt(const Date& asof, const std::set<std::string>& ids, const std::vector<Date>& dates, Size samples,
                    const T& t = T())
        : InMemoryCubeOpt(asof, ids, dates, samples, 1, t) {}

    InMemoryCubeOpt(const Date& asof, const std::set<std::string>& ids, const std::vector<Date>& dates, Size samples,
                    Size depth, const T& t = T())
        : asof_(asof), dates_(dates), samples_(samples), depth_(depth), t0data_(new T[depth_ * samples_]),
          data_(dates_.size() / N + (dates_.size() % N == 0 ? 0 : 1), std::vector<T*>(ids.size())) {

        Size pos = 0;
        for (const auto& id : ids) {
            idIdx_[id] = pos++;
        }

        t0data_ = new T[depth_ * idIdx_.size()];
        std::fill(t0data_, t0data_ + depth_ * idIdx_.size(), 0.0);
    }

    ~InMemoryCubeOpt() {

        if (t0data_)
            delete[] t0data_;

        for (auto& v : data_)
            for (auto p : v)
                if (p)
                    delete[] p;
    }

    Size numIds() const override { return idIdx_.size(); }
    Size numDates() const override { return dates_.size(); }
    Size samples() const override { return samples_; }
    Size depth() const override { return depth_; }
    const std::map<std::string, Size>& idsAndIndexes() const override { return idIdx_; }
    const std::vector<QuantLib::Date>& dates() const override { return dates_; }
    QuantLib::Date asof() const override { return asof_; }

    virtual Real getT0(Size i, Size d) const override {
        this->check(i, 0, 0, d);
        return static_cast<Real>(this->t0data_[d * idIdx_.size() + i]);
    }

    virtual void setT0(Real value, Size i, Size d) override {
        this->check(i, 0, 0, d);
        this->t0data_[d * idIdx_.size() + i] = static_cast<T>(value);
    }

    Real get(Size i, Size j, Size k, Size d) const override {
        this->check(i, j, k, d);
        Size n = j / N;
        Size m = j % N;
        if (data_[n][i] == nullptr)
            return 0.0;
        else
            return static_cast<Real>(data_[n][i][d * N * samples_ + m * samples_ + k]);
    }

    void set(Real value, Size i, Size j, Size k, Size d) override {
        this->check(i, j, k, d);
        if (value == 0.0)
            return;
        Size n = j / N;
        Size m = j % N;
        if (data_[n][i] == nullptr) {
            data_[n][i] = new T[N * depth_ * samples_];
            std::fill(data_[n][i], data_[n][i] + N * depth_ * samples_, 0.0);
        }
        data_[n][i][d * N * samples_ + m * samples_ + k] = static_cast<T>(value);
    }

private:
    void check(Size i, Size j, Size k, Size d) const {
        QL_REQUIRE(i < numIds(), "Out of bounds on ids (i=" << i << ", numIds=" << numIds() << ")");
        QL_REQUIRE(j < numDates(), "Out of bounds on dates (j=" << j << ", numDates=" << numDates() << ")");
        QL_REQUIRE(k < samples(), "Out of bounds on samples (k=" << k << ", samples=" << samples() << ")");
        QL_REQUIRE(d < depth(), "Out of bounds on depth (d=" << d << ", depth=" << depth() << ")");
    }

    QuantLib::Date asof_;
    std::vector<QuantLib::Date> dates_;
    Size samples_;
    Size depth_;

    T* t0data_;
    mutable std::vector<std::vector<T*>> data_;

    std::map<std::string, Size> idIdx_;
};

// using SinglePrecisionInMemoryCubeOpt = InMemoryCubeOpt<float>;
// using DoublePrecisionInMemoryCubeOpt = InMemoryCubeOpt<double>;

using SinglePrecisionInMemoryCube = InMemoryCubeOpt<float>;
using SinglePrecisionInMemoryCubeN = InMemoryCubeOpt<float>;
using DoublePrecisionInMemoryCube = InMemoryCubeOpt<double>;
using DoublePrecisionInMemoryCubeN = InMemoryCubeOpt<double>;

} // namespace analytics
} // namespace ore
