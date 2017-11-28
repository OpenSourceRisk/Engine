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

/*! \file engine/parametricvar.hpp
    \brief Perform parametric var calculation for a given portfolio
    \ingroup engine
*/

#pragma once

#include <orea/engine/sensitivitydata.hpp>

#include <ored/report/report.hpp>

#include <ql/math/array.hpp>
#include <ql/math/matrix.hpp>

using QuantLib::Matrix;
using QuantLib::Array;

namespace ore {
namespace analytics {

class ParametricVarCalculator {
public:
    virtual ~ParametricVarCalculator() {}
    ParametricVarCalculator(const std::map<std::string, std::string>& tradePortfolio,
                            const boost::shared_ptr<SensitivityData>& sensitivities,
                            const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covariance,
                            const std::vector<Real>& p, const std::string& method, const Size mcSamples,
                            const Size mcSeed, const bool breakdown);
    void calculate(ore::data::Report& report);

protected:
    virtual std::vector<Real> computeVar(const Matrix& omega, const Array& delta, const Matrix& gamma,
                                         const std::vector<Real>& p);
    const std::map<std::string, std::string> tradePortfolio_;
    const boost::shared_ptr<SensitivityData> sensitivities_;
    const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covariance_;
    const std::vector<Real> p_;
    const std::string method_;
    const Size mcSamples_, mcSeed_;
    const bool breakdown_;
};

void loadCovarianceDataFromCsv(std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real>& data,
                               const std::string& fileName, const char delim = '\n');

} // namespace analytics
} // namespace ore
