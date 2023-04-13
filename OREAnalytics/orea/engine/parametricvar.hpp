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
#include <orea/engine/sensitivityaggregator.hpp>
#include <orea/scenario/historicalscenariogenerator.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>

#include <ored/report/report.hpp>
#include <ored/utilities/timeperiod.hpp>

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

    //! A container for holding the parametric VAR parameters
    struct ParametricVarParams {
        enum class Method {
            Delta,
            DeltaGammaNormal,
            MonteCarlo,
            CornishFisher,
            Saddlepoint,
        };

        ParametricVarParams() {};
        ParametricVarParams(const std::string& m, QuantLib::Size samples, QuantLib::Size seed);

        Method method = Method::Delta;
        QuantLib::Size samples = QuantLib::Null<QuantLib::Size>();
        QuantLib::Size seed = QuantLib::Null<QuantLib::Size>();
    };

    virtual ~ParametricVarCalculator() {}
    ParametricVarCalculator(
        const std::map<std::string, std::set<std::pair<std::string, QuantLib::Size>>>& tradePortfolio, 
        const std::string& portfolioFilter, const QuantLib::ext::shared_ptr<SensitivityStream>& sensitivities,
        const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covariance, const std::vector<Real>& p,
        const ParametricVarParams& parametricVarParams, const bool breakdown, const bool salvageCovarianceMatrix);
    
    ParametricVarCalculator(
        const std::map<std::string, std::set<std::pair<std::string, QuantLib::Size>>>& tradePortfolios,
        const std::string& portfolioFilter, const QuantLib::ext::shared_ptr<SensitivityStream>& sensitivities,
        const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen,
        const ore::data::TimePeriod& benchmarkPeriod,
        const QuantLib::ext::shared_ptr<SensitivityScenarioData>& sensitivityConfig,
        const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketConfig, 
        const std::vector<QuantLib::Real>& p,
        const ParametricVarParams& parametricVarParams, const bool breakdown,
        const bool salvageCovarianceMatrix);

    ParametricVarCalculator(const std::vector<QuantLib::Real>& p, const ParametricVarParams& parametricVarParams);

    void calculate(ore::data::Report& report);
    
    typedef std::pair<RiskFactorKey, RiskFactorKey> CrossPair;
    virtual std::vector<Real> computeVar(const Matrix& omega, const map<RiskFactorKey, Real>& deltas,
        const map<CrossPair, Real>& gammas,
        const QuantExt::CovarianceSalvage& covarianceSalvage, Real factor = 1.0,
        const bool includeGammaMargin = true, const bool includeDeltaMargin = true);

protected:
    std::map<std::string, std::set<std::pair<std::string, QuantLib::Size>>> tradePortfolios_;
    const std::string portfolioFilter_;
    const QuantLib::ext::shared_ptr<SensitivityStream> sensitivities_;
    const std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covariance_;

    
    //! Historical scenario generator
    QuantLib::ext::shared_ptr<HistoricalScenarioGenerator> hisScenGen_;
    boost::optional<ore::data::TimePeriod> benchmarkPeriod_;
    const QuantLib::ext::shared_ptr<SensitivityScenarioData> sensitivityConfig_;
    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig_;

    Matrix cov_;

    const std::vector<Real> p_;

    //! The parameters to use for calculating the parametric VAR benchmark
    ParametricVarParams parametricVarParams_;
    bool breakdown_ = false;
    bool salvageCovarianceMatrix_ = true;
};

ParametricVarCalculator::ParametricVarParams::Method parseParametricVarMethod(const string& method);
std::ostream& operator<<(std::ostream& out, const ParametricVarCalculator::ParametricVarParams::Method& method);

} // namespace analytics
} // namespace ore
