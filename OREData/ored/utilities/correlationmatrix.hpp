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

/*! \file ored/utilities/correlationmatrix.hpp
    \brief configuration class for building correlation matrices
    \ingroup utilities
*/

#pragma once

#include <map>
#include <ql/math/matrix.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

//! Correlation matrix builder class
/*! Can be loaded with sets of individual correlations
    as pairs and will build a required correlation matrix.

        \ingroup utilities
*/
class CorrelationMatrixBuilder {
public:
    CorrelationMatrixBuilder() {}

    //! clear all data
    void reset() { data_.clear(); }

    void addCorrelation(const std::string& factor1, const std::string& factor2, Real correlation);

    //! Return a matrix for an IR/FX model, assumes ccys[0] is the base.
    /*! Returns an (2n-1)*(2n-1) matrix for an IR/FX model.
      For any correlations that are missing, it will set the value to 0 and not throw.
    */
    Disposable<Matrix> correlationMatrix(const std::vector<std::string>& ccys);

    //! Return a matrix for an IR/FX/INF model, assumes ccys[0] is the base.
    /*! Returns an (2n-1+m)*(2n-1+m) matrix for an IR/FX/INF model.
      For any correlations that are missing, it will set the value to 0 and not throw.
    */
    Disposable<Matrix> correlationMatrix(const std::vector<std::string>& ccys,
                                         const std::vector<std::string>& infIndices);

    //! Return a matrix for an IR/FX/INF/CR model, assumes ccys[0] is the base.
    /*! Returns an (2n-1+m+k)*(2n-1+m+k) matrix for an IR/FX/INF/CR model.
      For any correlations that are missing, it will set the value to 0 and not throw.
    */
    Disposable<Matrix> correlationMatrix(const std::vector<std::string>& ccys,
                                         const std::vector<std::string>& infIndices,
                                         const std::vector<std::string>& names);

    //! Return a matrix for an IR/FX/INF/CR/EQ model, assumes ccys[0] is the base.
    /*! Returns an (2n-1+m+p+q)*(2n-1+m+p+q) matrix for an IR/FX/INF/CR/EQ model.
      For any correlations that are missing, it will set the value to 0 and not throw.
    */
    Disposable<Matrix> correlationMatrix(const std::vector<std::string>& ccys,
                                         const std::vector<std::string>& infIndices,
                                         const std::vector<std::string>& names,
                                         const std::vector<std::string>& equities);

    // TODO: Add commodity

    //! Count the number of non-zero correlations in a given matrix
    static Size countNonZero(const Matrix& m);

    //! Check that the given matrix is a valid correlation matrix
    /*! Throws if:
      - matrix is not square and symmetric
      - diagonal is not 1.0
      - values are not in [-1.0,1.0]
      This method is called by correaltionMatrix() above before the newly built matrix is
      returned.
    */
    static void checkMatrix(const Matrix& m);

    //! Get the correlation between two factors
    Real lookup(const std::string& f1, const std::string& f2);

    //! Get the raw data map
    const std::map<std::pair<std::string, std::string>, Real>& data() { return data_; }

private:
    Disposable<Matrix> correlationMatrixImpl(const std::vector<std::string>& ccys, std::vector<std::string>& factors);

    Disposable<Matrix> correlationMatrixImpl(const std::vector<std::string>& ccys,
                                             const std::vector<std::string>& infIndices,
                                             std::vector<std::string>& factors);

    Disposable<Matrix> correlationMatrixImpl(const std::vector<std::string>& ccys,
                                             const std::vector<std::string>& infIndices,
                                             const std::vector<std::string>& names, std::vector<std::string>& factors);

    Disposable<Matrix> correlationMatrixImpl(const std::vector<std::string>& ccys,
                                             const std::vector<std::string>& infIndices,
                                             const std::vector<std::string>& names,
                                             const std::vector<std::string>& equities,
                                             std::vector<std::string>& factors);

    typedef std::pair<std::string, std::string> key;

    void checkFactor(const std::string& f);
    key buildkey(const std::string& f1, const std::string& f2);
    Disposable<Matrix> extendMatrix(const Matrix& mat, Size n);
    Disposable<Matrix> extendCorrelationMatrix(const Matrix& mat, const std::vector<std::string>& names,
                                               std::vector<std::string>& factors, const std::string& prefix);
    std::map<key, Real> data_;
};
} // namespace data
} // namespace ore
