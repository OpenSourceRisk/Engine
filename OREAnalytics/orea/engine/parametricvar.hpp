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
#include <orea/engine/varcalculator.hpp>
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

typedef std::pair<RiskFactorKey, RiskFactorKey> CrossPair;

class ParametricVarCalculator : public VarCalculator {
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

    ParametricVarCalculator(const ParametricVarParams& parametricVarParams, const QuantLib::Matrix& omega,
                            const std::map<RiskFactorKey, QuantLib::Real>& deltas,
                            const std::map<CrossPair, Real>& gammas, const QuantLib::ext::shared_ptr<QuantExt::CovarianceSalvage>& covarianceSalvage,
                            const bool& includeGammaMargin, const bool& includeDeltaMargin) : 
        parametricVarParams_(parametricVarParams), omega_(omega), deltas_(deltas), gammas_(gammas), covarianceSalvage_(covarianceSalvage),
        includeGammaMargin_(includeGammaMargin), includeDeltaMargin_(includeDeltaMargin) {}

    
    QuantLib::Real var(QuantLib::Real confidence, const bool isCall = true, 
        const std::set<std::pair<std::string, QuantLib::Size>>& tradeIds = {}) override;

private:
    const ParametricVarParams& parametricVarParams_;
    const QuantLib::Matrix& omega_;
    const std::map<RiskFactorKey, QuantLib::Real>& deltas_;
    const std::map<CrossPair, QuantLib::Real>& gammas_;
    const QuantLib::ext::shared_ptr<QuantExt::CovarianceSalvage>& covarianceSalvage_;
    const bool& includeGammaMargin_;
    const bool& includeDeltaMargin_;
};

//! Parametric VaR Calculator
/*! This class takes sensitivity data and a covariance matrix as an input and computes a parametric value at risk. The
 * output can be broken down by portfolios, risk classes (IR, FX, EQ, ...) and risk types (delta-gamma, vega, ...). */
class ParametricVarReport : public VarReport {
public:
    virtual ~ParametricVarReport() {}
    ParametricVarReport(
        const std::string& baseCurrency,
        const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
        const std::string& portfolioFilter,
        const std::vector<QuantLib::Real>& p,
        const ParametricVarCalculator::ParametricVarParams& parametricVarParams,
        const bool salvageCovarianceMatrix, boost::optional<ore::data::TimePeriod> period,
        std::unique_ptr<SensiRunArgs> sensiArgs = nullptr, const bool breakdown = false);
    
    ParametricVarReport(
        const std::string& baseCurrency,
        const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
        const std::string& portfolioFilter,
        const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen,
        const std::vector<QuantLib::Real>& p,
        const ParametricVarCalculator::ParametricVarParams& parametricVarParams,
        const bool salvageCovarianceMatrix, boost::optional<ore::data::TimePeriod> period,
        std::unique_ptr<SensiRunArgs> sensiArgs = nullptr, const bool breakdown = false);

    void createVarCalculator() override;
    
    typedef std::pair<RiskFactorKey, RiskFactorKey> CrossPair;

protected:    
    const QuantLib::ext::shared_ptr<SensitivityScenarioData> sensitivityConfig_;
    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig_;

    //! The parameters to use for calculating the parametric VAR benchmark
    ParametricVarCalculator::ParametricVarParams parametricVarParams_;
    bool salvageCovarianceMatrix_ = true;
};

ParametricVarCalculator::ParametricVarParams::Method parseParametricVarMethod(const std::string& method);
std::ostream& operator<<(std::ostream& out, const ParametricVarCalculator::ParametricVarParams::Method& method);

} // namespace analytics
} // namespace ore
