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

/*! \file aggregation/creditmigrationhelper.hpp
    \brief Credit migration helper class
    \ingroup engine
*/

#pragma once

#include <orea/aggregation/creditsimulationparameters.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/scenario/aggregationscenariodata.hpp>

#include <ored/portfolio/bond.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/log.hpp>

#include <qle/models/hullwhitebucketing.hpp>

#include <ql/math/matrix.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>

namespace ore {
namespace analytics {

//! Helper for credit migration risk calculation

class CreditMigrationHelper {
public:
    enum class CreditMode { Migration, Default };
    enum class LoanExposureMode { Notional, Value };
    enum class Evaluation { Analytic, ForwardSimulationA, ForwardSimulationB, TerminalSimulation };

    CreditMigrationHelper(const boost::shared_ptr<CreditSimulationParameters> parameters,
                          const boost::shared_ptr<NPVCube> cube, const boost::shared_ptr<NPVCube> nettedCube,
                          const boost::shared_ptr<AggregationScenarioData> aggData, const Size cubeIndexCashflows,
                          const Size cubeIndexStateNpvs, const Real distributionLowerBound,
                          const Real distributionUpperBound, const Size buckets, const Matrix& globalFactorCorrelation,
                          const std::string& baseCurrency);

    //! builds the helper for a specific subset of trades stored in the cube
    void build(const std::map<std::string, boost::shared_ptr<Trade>>& trades);

    const std::vector<Real>& upperBucketBound() const { return bucketing_.upperBucketBound(); }
    Array pnlDistribution(const Size date);

    //! Correlation of factor1/factor2 moves from today to date
    void stateCorrelations(const Size date, Size entity1, Size entity2, string index1, Real initialIndexValue1,
                           string index2, Real initialIndexValue2);

private:
    std::map<string, Matrix> rescaledTransitionMatrices(const Size date);
    void init();
    void initEntityStateSimulation();
    std::vector<std::vector<Matrix>> initEntityStateSimulation(const Size date, const Size path);
    void simulateEntityStates(const std::vector<std::vector<Matrix>>& cond, const Size path,
                              const MersenneTwisterUniformRng& mt, const Size date);
    Size simulatedEntityState(const Size i, const Size date, const Size path) const;
    bool simulatedDefaultOrder(const Size entityA, const Size entityB, const Size date, const Size path,
                               const Size n) const;
    Real generateMigrationPnl(const Size date, const Size path, const Size n) const;
    void generateConditionalMigrationPnl(const Size date, const Size path, const std::map<string, Matrix>& transMat,
                                         std::vector<Array>& condProbs, std::vector<Array>& pnl) const;

    boost::shared_ptr<CreditSimulationParameters> parameters_;
    boost::shared_ptr<NPVCube> cube_, nettedCube_;
    boost::shared_ptr<AggregationScenarioData> aggData_;
    Size cubeIndexCashflows_, cubeIndexStateNpvs_;
    Matrix globalFactorCorrelation_;
    std::string baseCurrency_;

    CreditMode creditMode_;
    LoanExposureMode loanExposureMode_;
    Evaluation evaluation_;
    std::vector<Real> cubeTimes_;

    QuantExt::Bucketing bucketing_;

    std::vector<std::set<std::string>> issuerTradeIds_;
    std::vector<std::set<std::string>> cptyNettingSetIds_;

    std::map<std::string, std::string> tradeCreditCurves_;
    std::map<std::string, Real> tradeNotionals_;
    std::map<std::string, std::string> tradeCurrencies_;
    std::map<std::string, Size> tradeCdsCptyIdx_;

    Size n_;
    std::vector<std::map<string, Matrix>> rescaledTransitionMatrices_;

    std::vector<Real> globalVar_;
    std::vector<std::vector<std::vector<Size>>> simulatedEntityState_;

    std::vector<std::vector<Matrix>> entityStateSimulationMatrices_;
    std::vector<std::vector<std::vector<Real>>> globalStates_;
};

CreditMigrationHelper::CreditMode parseCreditMode(const std::string& s);
CreditMigrationHelper::LoanExposureMode parseLoanExposureMode(const std::string& s);
CreditMigrationHelper::Evaluation parseEvaluation(const std::string& s);

} // namespace analytics
} // namespace ore
