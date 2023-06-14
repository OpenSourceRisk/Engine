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

#include <orea/aggregation/creditmigrationhelper.hpp>

#include <ored/portfolio/creditdefaultswap.hpp>
#include <ored/utilities/indexparser.hpp>

#include <qle/math/matrixfunctions.hpp>
#include <qle/models/transitionmatrix.hpp>

#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace analytics {

CreditMigrationHelper::CreditMigrationHelper(const boost::shared_ptr<CreditSimulationParameters> parameters,
                                             const boost::shared_ptr<NPVCube> cube,
                                             const boost::shared_ptr<NPVCube> nettedCube,
                                             const boost::shared_ptr<AggregationScenarioData> aggData,
                                             const Size cubeIndexCashflows, const Size cubeIndexStateNpvs,
                                             const Real distributionLowerBound, const Real distributionUpperBound,
                                             const Size buckets, const Matrix& globalFactorCorrelation,
                                             const string& baseCurrency)
    : parameters_(parameters), cube_(cube), nettedCube_(nettedCube), aggData_(aggData),
      cubeIndexCashflows_(cubeIndexCashflows), cubeIndexStateNpvs_(cubeIndexStateNpvs),
      globalFactorCorrelation_(globalFactorCorrelation), baseCurrency_(baseCurrency),
      creditMode_(parseCreditMode(parameters_->creditMode())),
      loanExposureMode_(parseLoanExposureMode(parameters_->loanExposureMode())),
      evaluation_(parseEvaluation(parameters_->evaluation())),
      bucketing_(distributionLowerBound, distributionUpperBound, buckets) {

    rescaledTransitionMatrices_.resize(cube_->numDates());
    init();
    if (evaluation_ != Evaluation::Analytic)
        initEntityStateSimulation();
} // CreditMigrationHelper()

namespace {

Real conditionalProb(const Real p, const Real m, const Real v) {
    QuantLib::CumulativeNormalDistribution nd;
    QuantLib::InverseCumulativeNormal icn;
    if (close_enough(p, 0.0))
        return 0.0;
    if (close_enough(p, 1.0))
        return 1.0;
    Real icnP = icn(p);
    if (close_enough(v, 1.0))
        return icnP >= m ? 1.0 : 0.0;
    return nd((icnP - m) / std::sqrt(1.0 - v));
}

Real prob_tauA_lt_tauB_lt_T(const Real pa, const Real pb, const Real T) {
    Real l1 = -std::log(1.0 - pa) / T;
    Real l2 = -std::log(1.0 - pb) / T;
    Real res = (1.0 - std::exp(-l2 * T)) - l2 / (l1 + l2) * (1 - std::exp(-(l1 + l2) * T));
    return res;
}

} // anonymous namespace

std::map<string, Matrix> CreditMigrationHelper::rescaledTransitionMatrices(const Size date) {

    std::map<string, Matrix> transMat; // rescaled transition matrix per (matrix) name

    // have we computed the result for the date index already?
    if (!rescaledTransitionMatrices_[date].empty()) {
        transMat = rescaledTransitionMatrices_[date];
    } else {
        Time t = cubeTimes_[date];

        const std::vector<string>& entities = parameters_->entities();
        const std::vector<string>& matrixNames = parameters_->transitionMatrices();

        for (Size i = 0; i < entities.size(); ++i) {
            if (transMat.count(matrixNames[i]) > 0) {
                DLOG("Transition matrix for " << entities[i] << " (" << matrixNames[i] << ") cached, nothing to do.");
            } else {
                QL_REQUIRE(parameters_->transitionMatrix().count(parameters_->transitionMatrices()[i]) > 0,
                           "No transition matrix defined for " << parameters_->entities()[i] << " / "
                                                               << parameters_->transitionMatrices()[i]);
                Matrix m = parameters_->transitionMatrix().at(parameters_->transitionMatrices()[i]);
                DLOG("Transition matrix (1y) for " << parameters_->entities()[i] << " is "
                                                   << parameters_->transitionMatrices()[i] << ":");
                DLOGGERSTREAM(m);
                if (n_ == Null<Size>()) {
                    n_ = m.rows();
                } else {
                    QL_REQUIRE(m.rows() == n_, "Found transition matrix with different dimension "
                                                   << m.rows() << "x" << m.columns() << " expected " << n_ << "x" << n_
                                                   << " for " << parameters_->entities()[i] << " / "
                                                   << parameters_->transitionMatrices()[i]);
                }
                QL_REQUIRE(date < cube_->numDates(),
                           "date index " << date << " outside range, cube has " << cube_->numDates() << " dates.");
                sanitiseTransitionMatrix(m);
                DLOG("Sanitised transition matrix:");
                DLOGGERSTREAM(m);
                Matrix g = QuantExt::generator(m);
                DLOG("Generator matrix:")
                DLOGGERSTREAM(g);
                checkGeneratorMatrix(g);
                Matrix mt = QuantExt::Expm(t * g);
                DLOG("Scaled transition matrix (t=" << t << "):");
                DLOGGERSTREAM(mt);
                checkTransitionMatrix(mt);
                Matrix mcheck = m;
                for (Size c = 1; c < static_cast<Size>(std::round(t)); ++c) {
                    mcheck = mcheck * m;
                }
                DLOG("Elementary transition matrix (t=" << std::round(t) << ", just for plausibility):");
                DLOGGERSTREAM(mcheck);
                transMat[matrixNames[i]] = mt;
            }
        }
        rescaledTransitionMatrices_[date] = transMat;
    }

    return transMat;
} // rescaledTransitionMatrices

void CreditMigrationHelper::init() {

    LOG("CreditMigrationHelper Init");

    n_ = Null<Size>(); // number of states

    Date asof = cube_->asof();
    for (Size d = 0; d < cube_->numDates(); ++d) {
        Date tDate = cube_->dates()[d];
        cubeTimes_.push_back(
            ActualActual(ActualActual::ISDA).yearFraction(asof, tDate)); // FIME make dc consistent with sim
    }

    const std::vector<Array>& loadings = parameters_->factorLoadings();
    Size f = globalFactorCorrelation_.rows();
    globalVar_.resize(parameters_->entities().size(), 0.0);
    for (Size i = 0; i < parameters_->entities().size(); ++i) {
        // variance of global factors
        QL_REQUIRE(parameters_->factorLoadings()[i].size() == f, "wrong size for loadings for entity "
                                                                     << parameters_->entities()[i] << " ("
                                                                     << loadings.size() << "), expected " << f);
        // don't optimise, code is simpler, matrix is small
        for (Size j = 0; j < loadings[i].size(); ++j) {
            for (Size k = 0; k < loadings[i].size(); ++k) {
                globalVar_[i] += loadings[i][j] * loadings[i][k] * globalFactorCorrelation_[j][k];
            }
        }
    }

    globalStates_.resize(cube_->numDates(), std::vector<std::vector<Real>>(parameters_->entities().size(),
                                                                           std::vector<Real>(cube_->samples())));
    Array globalFactors(f);
    std::vector<string> numStr(f);
    for (Size i = 0; i < f; ++i) {
        std::ostringstream num;
        num << i;
        numStr[i] = num.str();
    }
    for (Size d = 0; d < cube_->numDates(); ++d) {
        for (Size i = 0; i < parameters_->entities().size(); ++i) {
            for (Size j = 0; j < cube_->samples(); ++j) {
                for (Size ii = 0; ii < f; ++ii) {
                    globalFactors[ii] = aggData_->get(d, j, AggregationScenarioDataType::CreditState, numStr[ii]);
                }
                globalStates_[d][i][j] = DotProduct(loadings[i], globalFactors / std::sqrt(cubeTimes_[d]));
            }
        }
    }

    LOG("CreditMigration Init done.");

} // init

void CreditMigrationHelper::initEntityStateSimulation() {

    LOG("Init entity state simulation");

    Size numDates = evaluation_ == Evaluation::TerminalSimulation ? 1 : cube_->numDates();
    simulatedEntityState_.resize(parameters_->entities().size(),
                                 std::vector<std::vector<Size>>(numDates, std::vector<Size>(cube_->samples())));

    LOG("Init entity state simulation done.");

} // initEntityStatesSimulation

std::vector<std::vector<Matrix>> CreditMigrationHelper::initEntityStateSimulation(const Size date, const Size path) {
    Size numDates = evaluation_ == Evaluation::TerminalSimulation ? 1 : cube_->numDates();
    std::vector<std::vector<Matrix>> res(numDates,
                                         std::vector<Matrix>(parameters_->entities().size(), Matrix(n_, n_, 0.0)));

    const std::vector<string>& matrixNames = parameters_->transitionMatrices();

    // Variant 1

    // build terminal matrices conditional on global states first ...
    Size numWarnings = 0;
    for (Size d = 0; d < numDates; ++d) {
        Size condDateIdx = evaluation_ == Evaluation::ForwardSimulationA ? d : date;
        Size dateIdx = evaluation_ == Evaluation::TerminalSimulation ? date : d;
        auto transMat = rescaledTransitionMatrices(dateIdx);
        for (Size i = 0; i < parameters_->entities().size(); ++i) {
            const Matrix& m = transMat.at(matrixNames[i]);
            for (Size ii = 0; ii < m.rows(); ++ii) {
                Real p = 0.0, condProb0 = 0.0;
                for (Size jj = 0; jj < m.columns(); ++jj) {
                    p += m[ii][jj];
                    Real condProb = conditionalProb(p, globalStates_[condDateIdx][i][path], globalVar_[i]);
                    res[d][i][ii][jj] = condProb - condProb0;
                    condProb0 = condProb;
                }
            }
            try {
                checkTransitionMatrix(res[d][i]);
            } catch (const std::exception& e) {
                if (++numWarnings <= 10) {
                    WLOG("Invalid conditional transition matrix (path=" << path << ", date=" << d << ", entity =" << i
                                                                        << ": " << e.what());
                } else if (numWarnings == 11) {
                    WLOG("Suppress further warnings on invalid conditional transition matrices");
                }
                sanitiseTransitionMatrix(res[d][i]);
            }
        }
    }

    // ... and from them the fwd matrices, in case we want that
    if (evaluation_ == Evaluation::ForwardSimulationA || evaluation_ == Evaluation::ForwardSimulationB) {
        numWarnings = 0;
        for (Size d = numDates - 1; d > 0; --d) {
            for (Size i = 0; i < parameters_->entities().size(); ++i) {
                res[d][i] = inverse(res[d - 1][i]) * res[d][i];
                try {
                    checkTransitionMatrix(res[d][i]);
                } catch (const std::exception& e) {
                    if (++numWarnings <= 10) {
                        WLOG("Invalid conditional forward transition matrix (path="
                             << path << ", date=" << d << ", entity =" << i << ": " << e.what());
                    } else if (numWarnings == 11) {
                        WLOG("Suppress further warnings on invalid conditional forward transition matrices");
                    }
                    sanitiseTransitionMatrix(res[d][i]);
                }
            }
        }
    }

    // ... and finally build partial sums over columns for the simulation
    for (Size d = 0; d < numDates; ++d) {
        for (Size i = 0; i < parameters_->entities().size(); ++i) {
            Matrix& m = res[d][i];
            for (Size ii = 0; ii < m.rows(); ++ii) {
                for (Size jj = 1; jj < m.columns(); ++jj) {
                    m[ii][jj] += m[ii][jj - 1];
                }
            }
        }
    }

    return res;
}

void CreditMigrationHelper::simulateEntityStates(const std::vector<std::vector<Matrix>>& cond, const Size path,
                                                 const MersenneTwisterUniformRng& mt, const Size date) {

    QL_REQUIRE(evaluation_ != Evaluation::Analytic,
               "CreditMigrationHelper::simulateEntityStates() unexpected call, not in simulation mode");

    Size numDates = evaluation_ == Evaluation::TerminalSimulation ? 1 : cube_->numDates();
    for (Size i = 0; i < parameters_->entities().size(); ++i) {
        Size initialState = parameters_->initialStates()[i];
        for (Size d = 0; d < numDates; ++d) {
            Real tmp = mt.next().value;
            Size entityState =
                std::lower_bound(cond[d][i].row_begin(initialState), cond[d][i].row_end(initialState), tmp) -
                cond[d][i].row_begin(initialState);
            entityState = std::min(entityState, cond[d][i].columns() - 1); // play safe
            simulatedEntityState_[i][d][path] = entityState;
            initialState = entityState;
        }
    }

} // simulateEntityStates

Size CreditMigrationHelper::simulatedEntityState(const Size i, const Size date, const Size path) const {
    QL_REQUIRE(evaluation_ != Evaluation::Analytic,
               "CreditMigrationHelper::simulatedEntityState() unexpected call, not in simulation mode");
    return simulatedEntityState_[i][evaluation_ == Evaluation::TerminalSimulation ? 0 : date][path];
} // simulatedEntityState

bool CreditMigrationHelper::simulatedDefaultOrder(const Size entityA, const Size entityB, const Size date,
                                                  const Size path, const Size n) const {
    QL_REQUIRE(evaluation_ == Evaluation::ForwardSimulationA || evaluation_ == Evaluation::ForwardSimulationB,
               "CreditMigrationHelper::simulatedDefaultOrder() only available in ForwardSimulation(A/B) mode");
    bool defA = false, defB = false;
    for (Size i = 0; i <= date && !defB; ++i) {
        if (simulatedEntityState(entityA, i, path) == n - 1 && !defB) {
            defA = true;
        }
        if (simulatedEntityState(entityB, i, path) == n - 1) {
            defB = true;
        }
    }
    return defA && defB;
} // simulatedDefaultOrder

Real CreditMigrationHelper::generateMigrationPnl(const Size date, const Size path, const Size n) const {

    const std::vector<string>& entities = parameters_->entities();
    Real pnl = 0.0;

    for (Size i = 0; i < entities.size(); ++i) {
        // compute credit state of entitiy
        // issuer migration risk
        Size simEntityState = simulatedEntityState(i, date, path);
        for (auto const& tradeId : issuerTradeIds_[i]) {
            try {
                Size tid = cube_->idsAndIndexes().at(tradeId);
                Real baseValue = cube_->get(tid, date, path, 0);
                Real stateValue = cube_->get(tid, date, path, cubeIndexStateNpvs_ + simEntityState);
                if (loanExposureMode_ == LoanExposureMode::Notional) {
                    if (tradeNotionals_.find(tradeId) != tradeNotionals_.end()) {
                        // this is a bond
                        string tradeCcy = tradeCurrencies_.at(tradeId);
                        string ccypair = tradeCcy + baseCurrency_;
                        Real fx = 1.0;
                        if (tradeCcy != baseCurrency_) {
                            QL_REQUIRE(aggData_->has(AggregationScenarioDataType::FXSpot, ccypair),
                                       "FX spot data not found in aggregation data for currency pair " << ccypair);
                            fx = aggData_->get(date, path, AggregationScenarioDataType::FXSpot, ccypair);
                        }
                        // FIXME: We actually need the correct current notional as of the future horizon date,
                        // but we have the current notional as of today
                        baseValue = tradeNotionals_.at(tradeId) * fx;
                        // FIXME: get the bond's recovery rate
                        Real rr = 0.0;
                        stateValue = simEntityState == n - 1 ? rr * baseValue : baseValue;
                    }
                    if (tradeCdsCptyIdx_.find(tradeId) != tradeCdsCptyIdx_.end()) {
                        // this is a cds
                        baseValue = 0.0;
                        if (simEntityState < n - 1)
                            stateValue = 0.0;
                        else
                            stateValue *= aggData_->get(date, path, AggregationScenarioDataType::Numeraire);
                    }
                }
                if (creditMode_ == CreditMode::Default && simEntityState < n - 1) {
                    stateValue = baseValue;
                }
                pnl += stateValue - baseValue;
                // for a CDS we have to consider double default
                if (parameters_->doubleDefault() && simEntityState == n - 1 &&
                    tradeCdsCptyIdx_.find(tradeId) != tradeCdsCptyIdx_.end()) {
                    if (simulatedDefaultOrder(tradeCdsCptyIdx_.at(tradeId), i, date, path, n)) {
                        pnl -= stateValue - baseValue;
                    }
                }
            } catch (const std::exception& e) {
                ALOG("can not get state npv for trade " << tradeId << " (reason:" << e.what() << "), state "
                                                        << simEntityState << ", assume zero credit migration pnl");
            }
        }
        // default risk for uncollaterised derivative exposure
        // TODO, assuming a zero recovery here...
        for (auto const& nettingSetId : cptyNettingSetIds_[i]) {
            Size nid = nettedCube_->idsAndIndexes().at(nettingSetId);
            QL_REQUIRE(nettedCube_, "empty netted cube");
            if (simEntityState == n - 1)
                pnl -= std::max(nettedCube_->get(nid, date, path), 0.0);
        }
    }
    return pnl;
} // generateMigrationPnl

void CreditMigrationHelper::generateConditionalMigrationPnl(const Size date, const Size path,
                                                            const std::map<string, Matrix>& transMat,
                                                            std::vector<Array>& condProbs,
                                                            std::vector<Array>& pnl) const {

    Real t = cubeTimes_[date];

    const std::vector<string>& entities = parameters_->entities();
    const std::vector<string>& matrixNames = parameters_->transitionMatrices();

    for (Size i = 0; i < entities.size(); ++i) {
        // compute conditional migration prob
        Size initialState = parameters_->initialStates()[i];
        Real p = 0.0, condProb0 = 0.0;
        const Matrix& m = transMat.at(matrixNames[i]);
        for (Size j = 0; j < n_; ++j) {
            p += m[initialState][j];
            Real condProb = conditionalProb(p, globalStates_[date][i][path], globalVar_[i]);
            condProbs[i][j] = condProb - condProb0;
            condProb0 = condProb;
        }
        // issuer migration risk
        Size cdsCptyIdx = Null<Size>();
        for (auto const& tradeId : issuerTradeIds_[i]) {
            for (Size j = 0; j < n_; ++j) {
                try {
                    Size tid = cube_->idsAndIndexes().at(tradeId);
                    Real baseValue = cube_->get(tid, date, path, 0);
                    Real stateValue = cube_->get(tid, date, path, cubeIndexStateNpvs_ + j);
                    if (loanExposureMode_ == LoanExposureMode::Notional) {
                        if (tradeNotionals_.find(tradeId) != tradeNotionals_.end()) {
                            // this is a bond
                            string tradeCcy = tradeCurrencies_.at(tradeId);
                            string ccypair = tradeCcy + baseCurrency_;
                            Real fx = 1.0;
                            if (tradeCcy != baseCurrency_) {
                                QL_REQUIRE(aggData_->has(AggregationScenarioDataType::FXSpot, ccypair),
                                           "FX spot data not found in aggregation data for currency pair " << ccypair);
                                fx = aggData_->get(date, path, AggregationScenarioDataType::FXSpot, ccypair);
                            }
                            // FIXME: We actually need the correct current notional as of the future horizon date,
                            // but we have the current notional as of today
                            baseValue = tradeNotionals_.at(tradeId) * fx;
                            // FIXME: get the bond's recovery rate
                            Real rr = 0.0;
                            stateValue = j == n_ - 1 ? rr * baseValue : baseValue;
                        }
                        if (tradeCdsCptyIdx_.find(tradeId) != tradeCdsCptyIdx_.end()) {
                            // this is a cds
                            baseValue = 0.0;
                            if (j < n_ - 1)
                                stateValue = 0.0;
                            else
                                stateValue *= aggData_->get(date, path, AggregationScenarioDataType::Numeraire);
                        }
                    }
                    if (creditMode_ == CreditMode::Default && j < n_ - 1) {
                        stateValue = baseValue;
                    }
                    pnl[i][j] += stateValue - baseValue;
                    // pnl for additional double default state
                    if (j == n_ - 1)
                        pnl[i][n_] += stateValue - baseValue;
                    // for a CDS we have to subdivide the default migration event into two events (see above)
                    if (parameters_->doubleDefault() && j == n_ - 1 &&
                        tradeCdsCptyIdx_.find(tradeId) != tradeCdsCptyIdx_.end()) {
                        // FIXME currently we can not handle two CDS cptys for same underlying issuer
                        QL_REQUIRE(cdsCptyIdx == Null<Size>() || cdsCptyIdx == tradeCdsCptyIdx_.at(tradeId),
                                   "CreditMigrationHelper: Two different CDS cptys found for same issuer "
                                       << entities[i]);
                        // only adjust probability once
                        if (cdsCptyIdx == Null<Size>()) {
                            Real cptyDefaultPd =
                                transMat.at(matrixNames[tradeCdsCptyIdx_.at(tradeId)])[initialState][n_ - 1];
                            Real pd = prob_tauA_lt_tauB_lt_T(cptyDefaultPd, condProbs[i][n_ - 1], t);
                            QL_REQUIRE(pd <= condProbs[i][n_ - 1],
                                       "CreditMigrationHelper: unexpected probability for double default event "
                                           << pd << " > " << condProbs[i][n_ - 1]);
                            condProbs[i][n_ - 1] -= pd;
                            condProbs[i][n_] = pd;
                            cdsCptyIdx = tradeCdsCptyIdx_.at(tradeId);
                            // pnl for new state is zero
                            pnl[i][n_] -= stateValue - baseValue;
                        }
                    }
                } catch (const std::exception& e) {
                    ALOG("can not get state npv for trade " << tradeId << " (reason:" << e.what() << "), state " << j
                                                            << ", assume zero credit migration pnl");
                }
            }
        }
        // default risk for uncollaterised derivative exposure
        // TODO, assuming a zero recovery here...
        for (auto const& nettingSetId : cptyNettingSetIds_[i]) {
            auto nid = nettedCube_->idsAndIndexes().at(nettingSetId);
            QL_REQUIRE(nettedCube_, "empty netted cube");
            pnl[i][n_ - 1] -= std::max(nettedCube_->get(nid, date, path), 0.0);
        }
    }
} // generateConditionalMigrationPnl

Array CreditMigrationHelper::pnlDistribution(const Size date) {

    // FIXME if we ask this method for more than one time step, it might be more efficient to
    // pass a vector of those time steps and compute the distirbutions in one sweep here
    // in particular step 2b-1 (in forward simulation mode)

    LOG("Compute PnL distribution for date " << date);
    QL_REQUIRE(date < cube_->numDates(), "date index " << date << " out of range 0..." << cube_->numDates() - 1);
    const std::vector<string>& entities = parameters_->entities();

    // 1 get transition matrices for entities and rescale them to horizon

    std::map<string, Matrix> transMat; // rescaled transition matrix per (matrix) name

    // 1 get transition matrices for time step and compute variance of global factors for each entiity

    if (parameters_->creditRisk()) {
        transMat = rescaledTransitionMatrices(date);
    }

    // 2 compute conditional pnl distributions and average over paths

    const std::set<std::string>& tradeIds = cube_->ids();
    Array res(bucketing_.buckets(), 0.0);

    Size numPaths = cube_->samples();
    Real avgCash = 0.0;

    HullWhiteBucketing hwBucketing(bucketing_.upperBucketBound().begin(), bucketing_.upperBucketBound().end());

    MersenneTwisterUniformRng mt(parameters_->seed());

    for (Size path = 0; path < numPaths; ++path) {

        // 2a market pnl (t0 to horizon date, over whole cube)

        Real cash = 0.0;

        if (parameters_->marketRisk()) {
            for (Size j = 0; j <= date + 1; ++j) {
                for (auto const& tradeId : tradeIds) {
                    Size i = cube_->idsAndIndexes().at(tradeId);
                    // get cumulative survival probability on the path
                    Real sp = 1.0;
                    //Real rr = 0.0;
                    // FIXME 1
                    // Methodology question: Do we need/want to multiply with the stochastic discount factor
                    // here if we do an explicit credit default simulation at horizon?
                    // FIXME 2
                    // make CDS PnL neutral bei weighting flows with surv prob and generating protection flow
                    // with default prob
                    if (parameters_->zeroMarketPnl() && j > 0 &&
                        tradeCreditCurves_.find(tradeId) != tradeCreditCurves_.end()) {
                        string creditCurve = tradeCreditCurves_.at(tradeId);
                        sp = aggData_->get(j - 1, path, AggregationScenarioDataType::SurvivalWeight, creditCurve);
                        //rr = aggData_->get(j - 1, path, AggregationScenarioDataType::RecoveryRate, creditCurve);
                    }
                    if (j == 0) {
                        // at t0 we flip the sign of the npvs to get the initial cash balance
                        cash -= cube_->getT0(i, 0);
                        // collect intermediate cashflows
                        if (cubeIndexCashflows_ != Null<Size>())
                            cash += cube_->getT0(i, cubeIndexCashflows_);
                    } else if (j <= date) {
                        // collect intermediate cashflows
                        if (cubeIndexCashflows_ != Null<Size>())
                            cash += sp * cube_->get(i, j - 1, path, cubeIndexCashflows_);
                    } else {
                        // at the horizon date we realise the npv
                        cash += sp * cube_->get(i, j - 1, path, 0);
                    }
                }
            } // for data
        }     // if market risk

        if (!parameters_->creditRisk()) {
            // if we just add scalar market pnl realisations, we don't really need
            // the bucketing algorithm to do that, we just update the result
            // distribution directly
            res[hwBucketing.index(cash)] += 1.0 / static_cast<Real>(numPaths);
            continue;
        }

        // 2b credit migration pnl (at horizon date, over entities specified in credit simulation parameters)

        std::vector<Array> condProbs, pnl;

        if (evaluation_ != Evaluation::Analytic) {
            // 2b-1 generate pnl on the path using simulated idiosyncratic factors
            condProbs.resize(1, Array(parameters_->paths(), 1.0 / static_cast<Real>(parameters_->paths())));
            // we could build the distribution more efficiently here, but later in 2c we add the market pnl
            // maybe extend the hw bucketing so that we can feed precomputed distributions and just update
            // these with additional data?
            pnl.resize(1, Array(parameters_->paths(), 0.0));
            std::vector<std::vector<Matrix>> cond = initEntityStateSimulation(date, path);
            for (Size path2 = 0; path2 < parameters_->paths(); ++path2) {
                simulateEntityStates(cond, path, mt, date);
                pnl[0][path2] = generateMigrationPnl(date, path, n_);
            }
        } else {
            // 2b-2 generate pnl distribution without simulation of idiosyncratic factors using the conditional
            // independence of migration on the path / systemic factors

            // n+1 states, since for CDS we have to subdivide the issuer default into
            // i) default of issuer and non-default of CDS cpty
            // ii) default of issuer, default of CDS cpty (but after the issuer default)
            // iii) default of issuer, default of CDS cpty (before the issuer default)
            // for non-CDS trades for all sub-states the pnl will be set to the same value
            // for CDS trades i)+ii) will have the same pnl, but iii) will have a zero pnl
            // in total, we only have to distinguish i)+ii) and iii), i.e. we need one
            // additional state

            condProbs.resize(entities.size(), Array(n_ + 1, 0.0));
            pnl.resize(entities.size(), Array(n_ + 1, 0.0));
            generateConditionalMigrationPnl(date, path, transMat, condProbs, pnl);
        }

        // 2c aggregate market pnl and credit migration pnl

        if (parameters_->marketRisk()) {
            condProbs.push_back(Array(1, 1.0));
            pnl.push_back(Array(1, cash));
        }

        hwBucketing.computeMultiState(condProbs.begin(), condProbs.end(), pnl.begin());

        // 2d add pnl contribution of path to result distribution
        res += hwBucketing.probability() / static_cast<Real>(numPaths);
        // average market risk pnl
        avgCash += cash / static_cast<Real>(numPaths);

    } // for path

    DLOG("Expected Market Risk PnL at date " << date << ": " << avgCash);
    return res;
} // pnlDistribution

void CreditMigrationHelper::stateCorrelations(const Size date, Size entity1, Size entity2, string indexName1,
                                              Real initialIndexValue1, string indexName2, Real initialIndexValue2) {
    if (evaluation_ != Evaluation::Analytic) {
        LOG("Evaluate correlations of moves between entity " << entity1 << ", entity " << entity2 << ", index "
                                                             << indexName1 << ", index " << indexName2);
    } else {
        ALOG("StateCorrelations called but not in simulation mode, skip.");
        return;
    }

    QL_REQUIRE(entity1 < parameters_->entities().size(), "entity 1 " << entity1 << " out of range");
    QL_REQUIRE(entity2 < parameters_->entities().size(), "entity 2 " << entity2 << " out of range");

    // const std::vector<string>& entities = parameters_->entities();
    MersenneTwisterUniformRng mt(parameters_->seed());
    // initEntityStateSimulation();

    Size dates = cube_->numDates(); // date + 1;
    Size numOuter = cube_->samples();
    Size numInner = parameters_->paths();
    Size numPaths = numOuter * numInner;
    vector<vector<Real>> mean(4, vector<Real>(dates, 0.0));
    vector<vector<Real>> var(4, vector<Real>(dates, 0.0));
    vector<vector<Real>> cov(6, vector<Real>(dates, 0.0));
    vector<Real> delta(4);

    Size initialState1 = parameters_->initialStates()[entity1];
    Size initialState2 = parameters_->initialStates()[entity2];

    LOG("initial states: " << initialState1 << " " << initialState2);

    Size endDate = (evaluation_ == Evaluation::TerminalSimulation ? date : cube_->numDates() - 1);

    // outer systemic paths
    for (Size path = 0; path < numOuter; ++path) {

        std::vector<std::vector<Matrix>> cond = initEntityStateSimulation(date, path);

        // inner idiosyncratic paths
        for (Size path2 = 0; path2 < numInner; ++path2) {
            simulateEntityStates(cond, path, mt, date);

            Size d = (evaluation_ == Evaluation::TerminalSimulation ? date : 0);
            for (; d <= endDate; ++d) {

                Real index1 = aggData_->get(d, path, AggregationScenarioDataType::IndexFixing, indexName1);
                Real index2 = aggData_->get(d, path, AggregationScenarioDataType::IndexFixing, indexName2);

                Size currentState1 = simulatedEntityState(entity1, d, path);
                Size currentState2 = simulatedEntityState(entity2, d, path);
                delta[0] = Real(currentState1) - Real(initialState1);
                delta[1] = Real(currentState2) - Real(initialState2);
                delta[2] = index1 - initialIndexValue1;
                delta[3] = index2 - initialIndexValue2;
                Size k = 0;
                for (Size i = 0; i < delta.size(); ++i) {
                    mean[i][d] += delta[i] / numPaths;
                    var[i][d] += delta[i] * delta[i] / numPaths;
                    for (Size j = 0; j < i; ++j)
                        cov[k++][d] += delta[i] * delta[j] / numPaths;
                }
            }
        } // inner idiosyncratic paths
    }     // outer systemic paths

    Size d = (evaluation_ == Evaluation::TerminalSimulation ? date : 0);
    for (; d <= endDate; ++d) {
        Size k = 0;
        for (Size i = 0; i < delta.size(); ++i) {
            var[i][d] -= mean[i][d] * mean[i][d];
            for (Size j = 0; j < i; ++j) {
                cov[k++][d] -= mean[i][d] * mean[j][d];
                DLOG("State Correlation " << i + 1 << "-" << j + 1 << " date " << d << " "
                                          << cov[k - 1][d] / sqrt(var[i][d] * var[j][d]));
            }
        }
    }
}

void CreditMigrationHelper::build(const std::map<std::string, boost::shared_ptr<Trade>>& trades) {
    LOG("CreditMigrationHelper: Build trade ID map");
    issuerTradeIds_.resize(parameters_->entities().size());
    cptyNettingSetIds_.resize(parameters_->entities().size());
    for (Size i = 0; i < parameters_->entities().size(); ++i) {
        string entity = parameters_->entities()[i];
        std::set<string> tmp, tmp2;
        for (const auto& [_, t] : trades) {
            if (t->issuer() == entity) {
                tmp.insert(t->id());
            }
            if (t->envelope().counterparty() == entity &&
                std::find(parameters_->nettingSetIds().begin(), parameters_->nettingSetIds().end(),
                          t->envelope().nettingSetId()) != parameters_->nettingSetIds().end()) {
                tmp2.insert(t->envelope().nettingSetId());
            }
            boost::shared_ptr<Bond> bond = boost::dynamic_pointer_cast<Bond>(t);
            // FIXME the credit curve is also needed for CDS
            if (bond) {
                tradeCreditCurves_[t->id()] = bond->bondData().creditCurveId();
                // FIXME: We actually need the notional schedule here to determine future notionals
                tradeNotionals_[t->id()] = bond->notional();
                tradeCurrencies_[t->id()] = bond->bondData().currency();
            }
            boost::shared_ptr<CreditDefaultSwap> cds = boost::dynamic_pointer_cast<CreditDefaultSwap>(t);
            if (cds) {
                string cpty = cds->envelope().counterparty();
                QL_REQUIRE(cpty != t->issuer(), "CDS has same CPTY and issuer " << cpty);
                Size idx = std::find(parameters_->entities().begin(), parameters_->entities().end(), cpty) -
                           parameters_->entities().begin();
                if (idx < parameters_->entities().size())
                    tradeCdsCptyIdx_[t->id()] = idx;
                else
                    WLOG("CreditMigrationHelepr: CDS trade "
                         << t->id() << " has cpty " << cpty
                         << " which is not in the list of simulated entities, "
                            "ignore joint default event of issuer and cpty for this CDS");
            }
        }
        issuerTradeIds_[i] = tmp;
        cptyNettingSetIds_[i] = tmp2;
    }
    LOG("CreditMigrationHelper: Built issuer and cpty trade ID sets for " << parameters_->entities().size()
                                                                          << " entities.");
    for (Size i = 0; i < parameters_->entities().size(); ++i) {
        DLOG("Entity " << parameters_->entities()[i] << ": " << issuerTradeIds_[i].size()
                       << " trades with issuer risk, " << cptyNettingSetIds_[i].size()
                       << " nettings sets with derivative exposure risk.");
    }
}

CreditMigrationHelper::CreditMode parseCreditMode(const std::string& s) {
    static map<string, CreditMigrationHelper::CreditMode> m = {
        {"Migration", CreditMigrationHelper::CreditMode::Migration},
        {"Default", CreditMigrationHelper::CreditMode::Default}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Credit Mode \"" << s << "\" not recognized");
    }
}

CreditMigrationHelper::LoanExposureMode parseLoanExposureMode(const std::string& s) {
    static map<string, CreditMigrationHelper::LoanExposureMode> m = {
        {"Notional", CreditMigrationHelper::LoanExposureMode::Notional},
        {"Value", CreditMigrationHelper::LoanExposureMode::Value}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Loan EAD \"" << s << "\" not recognized");
    }
}

CreditMigrationHelper::Evaluation parseEvaluation(const std::string& s) {
    static map<string, CreditMigrationHelper::Evaluation> m = {
        {"Analytic", CreditMigrationHelper::Evaluation::Analytic},
        {"ForwardSimulationA", CreditMigrationHelper::Evaluation::ForwardSimulationA},
        {"ForwardSimulationB", CreditMigrationHelper::Evaluation::ForwardSimulationB},
        {"TerminalSimulation", CreditMigrationHelper::Evaluation::TerminalSimulation}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Evaluation \"" << s << "\" not recognized");
    }
}

} // namespace analytics
} // namespace ore
