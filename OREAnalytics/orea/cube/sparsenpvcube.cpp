/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <orea/cube/sparsenpvcube.hpp>

#include <ored/utilities/serializationdate.hpp>

#include <ql/errors.hpp>
#include <ql/math/comparison.hpp>

#include <boost/make_shared.hpp>

#include <fstream>

namespace ore {
namespace analytics {

template <typename T> SparseNpvCube<T>::SparseNpvCube() {}

template <typename T>
SparseNpvCube<T>::SparseNpvCube(const Date& asof, const std::set<std::string>& ids, const std::vector<Date>& dates,
                                Size samples, Size depth, const T& t)
    : asof_(asof), dates_(dates), samples_(samples), depth_(depth) {
    QL_REQUIRE(ids.size() > 0, "SparseNpvCube::SparseNpvCube no ids specified");
    QL_REQUIRE(dates.size() > 0, "SparseNpvCube::SparseNpvCube no dates specified");
    QL_REQUIRE(samples > 0, "SparseNpvCube::SparseNpvCube samples must be > 0");
    QL_REQUIRE(depth > 0, "SparseNpvCube::SparseNpvCube depth must be > 0");
    Size check = std::numeric_limits<Size>::max();
    check /= ids.size();
    check /= dates.size();
    check /= depth;
    QL_REQUIRE(check >= 1, "SparseNpvCube::SparseNpvCube: total size exceeded: ids ("
                               << ids_.size() << ") * dates (" << dates.size() << ") * depth (" << depth << ") > "
                               << std::numeric_limits<Size>::max());
    Size pos = 0;
    for (const auto& id : ids) {
        ids_[id] = pos++;
    }
}

template <typename T> Size SparseNpvCube<T>::numIds() const { return ids_.size(); }
template <typename T> Size SparseNpvCube<T>::numDates() const { return dates_.size(); }
template <typename T> Size SparseNpvCube<T>::samples() const { return samples_; }
template <typename T> Size SparseNpvCube<T>::depth() const { return depth_; }
template <typename T> Date SparseNpvCube<T>::asof() const { return asof_; }
template <typename T> const std::map<std::string, Size>& SparseNpvCube<T>::idsAndIndexes() const { return ids_; }
template <typename T> const std::vector<QuantLib::Date>& SparseNpvCube<T>::dates() const { return dates_; }

template <typename T> Size SparseNpvCube<T>::pos(Size i, Size j, Size d) const {
    return (i * (numDates() + 1) + j) * depth() + d;
}

template <typename T> Real SparseNpvCube<T>::getT0(Size i, Size d) const {
    this->check(i, 0, 0, d);
    auto v = data_.find(pos(i, 0, d));
    if (v != data_.end())
        return static_cast<Real>(v->second[0]);
    else
        return static_cast<Real>(0.0);
}

template <typename T> void SparseNpvCube<T>::setT0(Real value, Size i, Size d) {
    this->check(i, 0, 0, d);
    if (QuantLib::close_enough(value, 0.0))
        return;
    data_[pos(i, 0, d)] = std::vector<T>(1, static_cast<T>(value));
}

template <typename T> Real SparseNpvCube<T>::get(Size i, Size j, Size k, Size d) const {
    this->check(i, j, k, d);
    auto v = data_.find(pos(i, j + 1, d));
    if (v != data_.end())
        return static_cast<Real>(v->second[k]);
    else
        return static_cast<Real>(0.0);
}

template <typename T> void SparseNpvCube<T>::set(Real value, Size i, Size j, Size k, Size d) {
    this->check(i, j, k, d);
    if (QuantLib::close_enough(value, 0.0))
        return;
    auto v = data_.find(pos(i, j + 1, d));
    if (v != data_.end()) {
        v->second[k] = static_cast<T>(value);
    } else {
        std::vector<T> tmp(samples(), 0.0);
        tmp[k] = static_cast<T>(value);
        data_[pos(i, j + 1, d)] = tmp;
    }
}

template <typename T> void SparseNpvCube<T>::check(Size i, Size j, Size k, Size d) const {
    QL_REQUIRE(i < numIds(), "Out of bounds on ids (i=" << i << ", numIds=" << numIds() << ")");
    QL_REQUIRE(j < numDates(), "Out of bounds on dates (j=" << j << ", numDates=" << numDates() << ")");
    QL_REQUIRE(k < samples(), "Out of bounds on samples (k=" << k << ", samples=" << samples() << ")");
    QL_REQUIRE(d < depth(), "Out of bounds on depth (d=" << d << ", depth=" << depth() << ")");
}

// template instantiations for Real and float

template SparseNpvCube<Real>::SparseNpvCube(const Date& asof, const std::set<std::string>& ids,
                                            const std::vector<Date>& dates, Size samples, Size depth, const Real& t);
template Size SparseNpvCube<Real>::numIds() const;
template Size SparseNpvCube<Real>::numDates() const;
template Size SparseNpvCube<Real>::samples() const;
template Size SparseNpvCube<Real>::depth() const;
template Date SparseNpvCube<Real>::asof() const;
template const std::map<std::string, Size>& SparseNpvCube<Real>::idsAndIndexes() const;
template const std::vector<QuantLib::Date>& SparseNpvCube<Real>::dates() const;
template Size SparseNpvCube<Real>::pos(Size i, Size j, Size d) const;
template Real SparseNpvCube<Real>::getT0(Size i, Size d) const;
template void SparseNpvCube<Real>::setT0(Real value, Size i, Size d);
template Real SparseNpvCube<Real>::get(Size i, Size j, Size k, Size d) const;
template void SparseNpvCube<Real>::set(Real value, Size i, Size j, Size k, Size d);
template void SparseNpvCube<Real>::check(Size i, Size j, Size k, Size d) const;

template SparseNpvCube<float>::SparseNpvCube();
template SparseNpvCube<float>::SparseNpvCube(const Date& asof, const std::set<std::string>& ids,
                                             const std::vector<Date>& dates, Size samples, Size depth, const float& t);
template Size SparseNpvCube<float>::numIds() const;
template Size SparseNpvCube<float>::numDates() const;
template Size SparseNpvCube<float>::samples() const;
template Size SparseNpvCube<float>::depth() const;
template Date SparseNpvCube<float>::asof() const;
template const std::map<std::string, Size>& SparseNpvCube<float>::idsAndIndexes() const;
template const std::vector<QuantLib::Date>& SparseNpvCube<float>::dates() const;
template Size SparseNpvCube<float>::pos(Size i, Size j, Size d) const;
template Real SparseNpvCube<float>::getT0(Size i, Size d) const;
template void SparseNpvCube<float>::setT0(Real value, Size i, Size d);
template Real SparseNpvCube<float>::get(Size i, Size j, Size k, Size d) const;
template void SparseNpvCube<float>::set(Real value, Size i, Size j, Size k, Size d);
template void SparseNpvCube<float>::check(Size i, Size j, Size k, Size d) const;

} // namespace analytics
} // namespace ore
