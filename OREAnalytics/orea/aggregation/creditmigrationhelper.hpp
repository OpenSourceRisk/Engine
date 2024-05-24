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

/*! Helper for credit migration risk calculation
   Dynamics of entity i's state X_i:
     \f$ dX_i = dY_i + dZ_i \f$
   with
     - systemic part \f$ dY_i = \sum_{j=1}^n \beta_{ij} dG_j \f$
     - n correlated global factors \f$ G_j \f$
     - entity specific factor loadings \f$ \beta_{ij} \f$
     - idiosyncratic part \f$ dZ_i = \sigma_i dW_i \f$
     - independent  Wiener processes W, i.e. \f$ dW_k dW_l = 0 \f$ and \f$ dW_k dG_j = 0 \f$ */
class CreditMigrationHelper {
public:
    enum class CreditMode { Migration, Default };
    enum class LoanExposureMode { Notional, Value };
    enum class Evaluation { Analytic, ForwardSimulationA, ForwardSimulationB, TerminalSimulation };

    CreditMigrationHelper(const QuantLib::ext::shared_ptr<CreditSimulationParameters> parameters,
                          const QuantLib::ext::shared_ptr<NPVCube> cube, const QuantLib::ext::shared_ptr<NPVCube> nettedCube,
                          const QuantLib::ext::shared_ptr<AggregationScenarioData> aggData, const Size cubeIndexCashflows,
                          const Size cubeIndexStateNpvs, const Real distributionLowerBound,
                          const Real distributionUpperBound, const Size buckets, const Matrix& globalFactorCorrelation,
                          const std::string& baseCurrency);

    //! builds the helper for a specific subset of trades stored in the cube
    void build(const std::map<std::string, QuantLib::ext::shared_ptr<Trade>>& trades);

    const std::vector<Real>& upperBucketBound() const { return bucketing_.upperBucketBound(); }
    //
    Array pnlDistribution(const Size date);

private:
    /*! Get the transition matrix from today to date by entity,
      sanitise the annual transition matrix input,
      rescale to the desired horizon/date using the generator,
      cache the result so that we do the sanitising/rescaling only once */
    std::map<string, Matrix> rescaledTransitionMatrices(const Size date);

    /*! Initialise
      - the variance of the global part Y_i of entity state X_i, for all entities
      - the global part Y_i of entity i's state X_i by date index, entity index and sample number
        using the simulated global state paths stored in the aggregation scenario data object */
    void init();

    //! Allocate 3d storage for the simulated idiosyncratic factors by entity, date and sample
    void initEntityStateSimulation();

    /*! Initialise the entity state simulationn for a given date for
        Evaluation = TerminalSimulation:
        Return transition matrix for each entity for the given date,
        conditional on the global terminal state on the given path */
    std::vector<Matrix> initEntityStateSimulation(const Size date, const Size path);

    /*! Generate one entity state sample path for all entities given the global state path
        and given the conditional transition matrices for all entities at the terminal date. */
    void simulateEntityStates(const std::vector<Matrix>& cond, const Size path, const MersenneTwisterUniformRng& mt);

    //! Look up the simulated entity credit state for the given entity, date and path
    Size simulatedEntityState(const Size i, const Size path) const;

    /*! Return a single PnL impact due to credit migration or default of Bond/CDS issuers and default of
      netting set counterparties on the given global path */
    Real generateMigrationPnl(const Size date, const Size path, const Size n) const;

    /*! Return a vector of PnL impacts and associated conditional probabilities for the specified global path,
      due to credit migration or default of Bond/CDS issuers and default of netting set counterparties */
    void generateConditionalMigrationPnl(const Size date, const Size path, const std::map<string, Matrix>& transMat,
                                         std::vector<Array>& condProbs, std::vector<Array>& pnl) const;

    QuantLib::ext::shared_ptr<CreditSimulationParameters> parameters_;
    QuantLib::ext::shared_ptr<NPVCube> cube_, nettedCube_;
    QuantLib::ext::shared_ptr<AggregationScenarioData> aggData_;
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

    // Transition matrix rows
    Size n_;
    std::vector<std::map<string, Matrix>> rescaledTransitionMatrices_;
    // Variance of the systemic part (Y_i) of entity state X_i
    std::vector<Real> globalVar_;
    // Storage for the simulated idiosyncratic factors Z by entity, sample number
    std::vector<std::vector<Size>> simulatedEntityState_;

    std::vector<std::vector<Matrix>> entityStateSimulationMatrices_;
    // Systemic part (Y_i) of entity state X_i by date index, entity index, sample number
    std::vector<std::vector<std::vector<Real>>> globalStates_;
};

CreditMigrationHelper::CreditMode parseCreditMode(const std::string& s);
CreditMigrationHelper::LoanExposureMode parseLoanExposureMode(const std::string& s);
CreditMigrationHelper::Evaluation parseEvaluation(const std::string& s);

} // namespace analytics
} // namespace ore
