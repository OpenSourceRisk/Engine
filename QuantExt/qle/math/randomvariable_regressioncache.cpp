/*
 Copyright (C) 2026 AcadiaSoft, Inc.
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

#include <ql/math/matrixutilities/svd.hpp>

#include <qle/math/randomvariable_regressioncache.hpp>

#include <boost/container_hash/hash.hpp>

namespace QuantExt {

RandomVariableRegressionCache::Key::Key(
    const std::vector<const RandomVariable*>& regressor,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
    const Filter& filter, const RandomVariableRegressionMethod regressionMethod) {
    value_ = 0;
    for (auto const& r : regressor)
        boost::hash_combine(value_, *r);
    boost::hash_combine(value_, filter);
    boost::hash_combine(value_, static_cast<int>(regressionMethod));
    boost::hash_combine(value_, &basisFn);
}

std::size_t RandomVariableRegressionCache::Key::operator()() const { return value_; }

bool RandomVariableRegressionCache::hasMatrixDecomposition(const Key& key) const {
    if (auto it = data_.find(key()); it != data_.end()) {
        cacheHit_++;
        return true;
    }
    cacheMiss_++;
    return false;
}

void RandomVariableRegressionCache::getMatrixDecomposition(
    const Key& key, QuantLib::ext::shared_ptr<QuantLib::Matrix>& q, QuantLib::ext::shared_ptr<QuantLib::Matrix>& r,
    QuantLib::ext::shared_ptr<std::vector<QuantLib::Size>>& lipvt,
    QuantLib::ext::shared_ptr<QuantLib::SVD>& svd) const {
    if (auto it = data_.find(key()); it != data_.end()) {
        q = it->second.q;
        r = it->second.r;
        lipvt = it->second.lipvt;
        svd = it->second.svd;
    }
}

void RandomVariableRegressionCache::addMatrixDecomposition(const RandomVariableRegressionCache::Key& key,
                                                           QuantLib::ext::shared_ptr<QuantLib::Matrix> q,
                                                           QuantLib::ext::shared_ptr<QuantLib::Matrix> r,
                                                           QuantLib::ext::shared_ptr<std::vector<QuantLib::Size>> lipvt,
                                                           QuantLib::ext::shared_ptr<QuantLib::SVD> svd) {
    dataSize_ += q ? q->rows() * q->columns() : 0;
    dataSize_ += r ? r->rows() * r->columns() : 0;
    dataSize_ += svd ? svd->U().rows() * svd->U().columns() : 0;
    dataSize_ += svd ? svd->V().rows() * svd->V().columns() : 0;

    MatrixDecompData d;
    d.q = std::move(q);
    d.r = std::move(r);
    d.lipvt = std::move(lipvt);
    d.svd = std::move(svd);

    QL_REQUIRE(data_.insert(std::make_pair(key(), std::move(d))).second,
               "RandomVariableRegressionCache::addMatrixDecomposition(): called with key that is already present. This "
               "is unexpected.");
}

} // namespace QuantExt
