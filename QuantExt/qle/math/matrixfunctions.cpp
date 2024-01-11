/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/math/matrixfunctions.hpp>

#ifdef ORE_USE_EIGEN

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include <unsupported/Eigen/MatrixFunctions>

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#endif

namespace QuantExt {

namespace {

inline Eigen::MatrixXd ql2eigen(const QuantLib::Matrix& m) {
    Eigen::MatrixXd res(m.rows(), m.columns());
    for (size_t i = 0; i < m.rows(); ++i)
        for (size_t j = 0; j < m.columns(); ++j)
            res(i, j) = m(i, j);
    return res;
}

inline QuantLib::Matrix eigen2ql(const Eigen::MatrixXd& m) {
    QuantLib::Matrix res(m.rows(), m.cols());
    for (long i = 0; i < m.rows(); ++i)
        for (long j = 0; j < m.cols(); ++j)
            res(i, j) = m(i, j);
    return res;
}

} // anonymous namespace

bool supports_Logm() { return true; }
bool supports_Expm() { return true; }

QuantLib::Matrix Logm(const QuantLib::Matrix& m) {
    QuantLib::Matrix res = eigen2ql(ql2eigen(m).log());
    return res;
}

QuantLib::Matrix Expm(const QuantLib::Matrix& m) {
    QuantLib::Matrix res = eigen2ql(ql2eigen(m).exp());
    return res;
}
} // namespace QuantExt

#else

#include <ql/math/matrixutilities/expm.hpp>

namespace QuantExt {

bool supports_Logm() { return false; }
bool supports_Expm() { return true; }

QuantLib::Matrix Logm(const QuantLib::Matrix& m) {
    QL_FAIL("Logm(): no implementation provided, you can e.g. install Eigen and rebuild ORE to enable this function.");
}

QuantLib::Matrix Expm(const QuantLib::Matrix& m) { return QuantLib::Expm(m); }

} // namespace QuantExt

#endif
