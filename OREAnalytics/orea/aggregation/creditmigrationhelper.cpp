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

CreditMigrationHelper::CreditMigrationHelper(const QuantLib::ext::shared_ptr<CreditSimulationParameters> parameters,
                                             const QuantLib::ext::shared_ptr<NPVCube> cube,
                                             const QuantLib::ext::shared_ptr<NPVCube> nettedCube,
                                             const QuantLib::ext::shared_ptr<AggregationScenarioData> aggData,
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
    if (evaluation_ == Evaluation::TerminalSimulation)
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

    simulatedEntityState_ =
        std::vector<std::vector<Size>>(parameters_->entities().size(), std::vector<Size>(cube_->samples()));

    LOG("Init entity state simulation done.");

} // initEntityStatesSimulation

std::vector<Matrix> CreditMigrationHelper::initEntityStateSimulation(const Size date, const Size path) {
    std::vector<Matrix> res = std::vector<Matrix>(parameters_->entities().size(), Matrix(n_, n_, 0.0));

    const std::vector<string>& matrixNames = parameters_->transitionMatrices();

    // build terminal matrices conditional on global states
    Size numWarnings = 0;
    auto transMat = rescaledTransitionMatrices(date);
    for (Size i = 0; i < parameters_->entities().size(); ++i) {
        const Matrix& m = transMat.at(matrixNames[i]);
        for (Size ii = 0; ii < m.rows(); ++ii) {
            Real p = 0.0, condProb0 = 0.0;
            for (Size jj = 0; jj < m.columns(); ++jj) {
                p += m[ii][jj];
                Real condProb = conditionalProb(p, globalStates_[date][i][path], globalVar_[i]);
                res[i][ii][jj] = condProb - condProb0;
                condProb0 = condProb;
            }
        }
        try {
            checkTransitionMatrix(res[i]);
        } catch (const std::exception& e) {
            if (++numWarnings <= 10) {
                WLOG("Invalid conditional transition matrix (path=" << path << ", date=" << date << ", entity =" << i
                                                                    << ": " << e.what());
            } else if (numWarnings == 11) {
                WLOG("Suppress further warnings on invalid conditional transition matrices");
            }
            sanitiseTransitionMatrix(res[i]);
        }
    }

    // ... and finally build partial sums over columns for the simulation
    for (Size i = 0; i < parameters_->entities().size(); ++i) {
        Matrix& m = res[i];
        for (Size ii = 0; ii < m.rows(); ++ii) {
            for (Size jj = 1; jj < m.columns(); ++jj) {
                m[ii][jj] += m[ii][jj - 1];
            }
        }
    }

    return res;
}

void CreditMigrationHelper::simulateEntityStates(const std::vector<Matrix>& cond, const Size path,
                                                 const MersenneTwisterUniformRng& mt) {

    QL_REQUIRE(evaluation_ != Evaluation::Analytic,
               "CreditMigrationHelper::simulateEntityStates() unexpected call, not in simulation mode");

    for (Size i = 0; i < parameters_->entities().size(); ++i) {
        Size initialState = parameters_->initialStates()[i];
            Real tmp = mt.next().value;
            Size entityState =
                std::lower_bound(cond[i].row_begin(initialState), cond[i].row_end(initialState), tmp) -
                cond[i].row_begin(initialState);
            entityState = std::min(entityState, cond[i].columns() - 1); // play safe
            simulatedEntityState_[i][path] = entityState;
            initialState = entityState;
    }

} // simulateEntityStates

Size CreditMigrationHelper::simulatedEntityState(const Size i, const Size path) const {
    QL_REQUIRE(evaluation_ != Evaluation::Analytic,
               "CreditMigrationHelper::simulatedEntityState() unexpected call, not in simulation mode");
    return simulatedEntityState_[i][path];
} // simulatedEntityState

Real CreditMigrationHelper::generateMigrationPnl(const Size date, const Size path, const Size n) const {

    QL_REQUIRE(!parameters_->doubleDefault(),
               "CreditMigrationHelper::generateMigrationPnl() does not support double default");

    const std::vector<string>& entities = parameters_->entities();
    Real pnl = 0.0;

    for (Size i = 0; i < entities.size(); ++i) {
        // compute credit state of entitiy
        // issuer migration risk
        Size simEntityState = simulatedEntityState(i, path);
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
            } catch (const std::exception& e) {
                ALOG("can not get state npv for trade " << tradeId << " (reason:" << e.what() << "), state "
                                                        << simEntityState << ", assume zero credit migration pnl");
            }
        }
        // default risk for derivative exposure
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
        // default risk for derivative exposure
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
            auto cond = initEntityStateSimulation(date, path);
            for (Size path2 = 0; path2 < parameters_->paths(); ++path2) {
                simulateEntityStates(cond, path, mt);
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

void CreditMigrationHelper::build(const std::map<std::string, QuantLib::ext::shared_ptr<Trade>>& trades) {
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
            QuantLib::ext::shared_ptr<Bond> bond = QuantLib::ext::dynamic_pointer_cast<Bond>(t);
            if (bond) {
                tradeCreditCurves_[t->id()] = bond->bondData().creditCurveId();
                // FIXME: We actually need the notional schedule here to determine future notionals
                tradeNotionals_[t->id()] = bond->notional();
                tradeCurrencies_[t->id()] = bond->bondData().currency();
            }
            QuantLib::ext::shared_ptr<CreditDefaultSwap> cds = QuantLib::ext::dynamic_pointer_cast<CreditDefaultSwap>(t);
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
