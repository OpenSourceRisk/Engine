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

#pragma once

#include <qle/math/randomvariable.hpp>

namespace QuantLib {
class SVD;
}

namespace QuantExt {

class RandomVariableRegressionCache {
public:
    class Key {
    public:
        Key() = default;
        Key(const std::vector<const RandomVariable*>& regressor,
            const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn,
            const Filter& filter, const RandomVariableRegressionMethod regressionMethod);
        std::size_t operator()() const;

    private:
        std::size_t value_;
    };

    RandomVariableRegressionCache() = default;

    bool hasMatrixDecomposition(const Key& key) const;
    void getMatrixDecomposition(const Key& key, QuantLib::ext::shared_ptr<QuantLib::Matrix>& q,
                                QuantLib::ext::shared_ptr<QuantLib::Matrix>& r,
                                QuantLib::ext::shared_ptr<std::vector<QuantLib::Size>>& lipvt,
                                QuantLib::ext::shared_ptr<QuantLib::SVD>& svd) const;
    void addMatrixDecomposition(const Key& key, QuantLib::ext::shared_ptr<QuantLib::Matrix> q,
                                QuantLib::ext::shared_ptr<QuantLib::Matrix> r,
                                QuantLib::ext::shared_ptr<std::vector<QuantLib::Size>> lipvt,
                                QuantLib::ext::shared_ptr<QuantLib::SVD> svd);

    std::size_t size() const { return data_.size(); }
    std::size_t dataSize() const { return dataSize_; }
    std::size_t hit() const { return cacheHit_; }
    std::size_t miss() const { return cacheMiss_; }

private:
    struct MatrixDecompData {
        QuantLib::ext::shared_ptr<QuantLib::Matrix> q = nullptr;
        QuantLib::ext::shared_ptr<QuantLib::Matrix> r = nullptr;
        QuantLib::ext::shared_ptr<std::vector<QuantLib::Size>> lipvt = nullptr;
        QuantLib::ext::shared_ptr<QuantLib::SVD> svd = nullptr;
    };
    std::map<std::size_t, MatrixDecompData> data_;
    mutable std::size_t dataSize_ = 0;
    mutable std::size_t cacheHit_ = 0;
    mutable std::size_t cacheMiss_ = 0;
};

} // namespace QuantExt
