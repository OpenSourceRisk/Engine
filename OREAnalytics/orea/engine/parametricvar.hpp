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

#include <orea/engine/sensitivitystream.hpp>

#include <ored/report/report.hpp>

#include <qle/math/covariancesalvage.hpp>

#include <ql/math/array.hpp>
#include <ql/math/matrix.hpp>

#include <map>
#include <set>

namespace ore {
namespace analytics {
using QuantLib::Array;
using QuantLib::Matrix;

//! Parametric VaR Calculator
/*! This class takes sensitivity data and a covariance matrix as an input and computes a parametric value at risk. The
 * output can be broken down by portfolios, risk classes (IR, FX, EQ, ...) and risk types (delta-gamma, vega, ...). */
class ParametricVarCalculator {
public:
    virtual ~ParametricVarCalculator() {}
    ParametricVarCalculator(const std::map<std::string, std::set<std::string>>& tradePortfolio,
                            const std::string& portfolioFilter,
                            const boost::shared_ptr<SensitivityStream>& sensitivities,
                            const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covariance,
                            const std::vector<Real>& p, const std::string& method, const Size mcSamples,
                            const Size mcSeed, const bool breakdown, const bool salvageCovarianceMatrix);
    void calculate(ore::data::Report& report);

protected:
    virtual std::vector<Real> computeVar(const Matrix& omega, const Array& delta, const Matrix& gamma,
                                         const std::vector<Real>& p,
                                         const QuantExt::CovarianceSalvage& covarianceSalvage);
    const std::map<std::string, std::set<std::string>> tradePortfolios_;
    const std::string portfolioFilter_;
    const boost::shared_ptr<SensitivityStream> sensitivities_;
    const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covariance_;
    const std::vector<Real> p_;
    const std::string method_;
    const Size mcSamples_, mcSeed_;
    const bool breakdown_, salvageCovarianceMatrix_;
};

void loadCovarianceDataFromCsv(std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real>& data,
                               const std::string& fileName, const char delim = '\n');

} // namespace analytics
} // namespace ore
