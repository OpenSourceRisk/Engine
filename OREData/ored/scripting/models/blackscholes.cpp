/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/scripting/models/blackscholes.hpp>

#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/math/randomvariablelsmbasissystem.hpp>
#include <qle/methods/fdmblackscholesmesher.hpp>
#include <qle/methods/fdmblackscholesop.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/methods/finitedifferences/meshers/fdmmeshercomposite.hpp>
#include <ql/quotes/simplequote.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

BlackScholes::BlackScholes(const Type type, const Size paths, const std::string& currency,
                           const Handle<YieldTermStructure>& curve, const std::string& index,
                           const std::string& indexCurrency, const Handle<BlackScholesModelWrapper>& model,
                           const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig,
                           const std::string& calibration, const std::vector<Real>& calibrationStrikes,
                           const Params& params)
    : BlackScholes(type, paths, {currency}, {curve}, {}, {}, {}, {index}, {indexCurrency}, {currency}, model, {},
                   simulationDates, iborFallbackConfig, calibration, {{index, calibrationStrikes}}, params) {}

BlackScholes::BlackScholes(
    const Type type, const Size paths, const std::vector<std::string>& currencies,
    const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
    const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
    const std::set<std::string>& payCcys, const Handle<BlackScholesModelWrapper>& model,
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
    const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig, const std::string& calibration,
    const std::map<std::string, std::vector<Real>>& calibrationStrikes, const Params& params)
    : ModelImpl(Type::MC, params, curves.at(0)->dayCounter(), paths, currencies, irIndices, infIndices, indices,
                indexCurrencies, simulationDates, iborFallbackConfig),
      curves_(curves), fxSpots_(fxSpots), payCcys_(payCcys), model_(model), correlations_(correlations),
      calibration_(calibration), calibrationStrikes_(calibrationStrikes) {

    // check inputs

    QL_REQUIRE(!model_.empty(), "model is empty");
    QL_REQUIRE(!curves_.empty(), "no curves given");
    QL_REQUIRE(currencies_.size() == curves_.size(), "number of currencies (" << currencies_.size()
                                                                              << ") does not match number of curves ("
                                                                              << curves_.size() << ")");
    QL_REQUIRE(currencies_.size() == fxSpots_.size() + 1,
               "number of currencies (" << currencies_.size() << ") does not match number of fx spots ("
                                        << fxSpots_.size() << ") + 1");

    QL_REQUIRE(indices_.size() == model_->processes().size(),
               "mismatch of processes size (" << model_->processes().size() << ") and number of indices ("
                                              << indices_.size() << ")");

    for (auto const& c : payCcys) {
        QL_REQUIRE(std::find(currencies_.begin(), currencies_.end(), c) != currencies_.end(),
                   "pay ccy '" << c << "' not found in currencies list.");
    }

    QL_REQUIRE(calibration_ == "ATM" || calibration_ == "Deal" || calibration == "LocalVol",
               "calibration '" << calibration_ << "' invalid, expected one of ATM, Deal, LocalVol");

    // register with observables

    for (auto const& o : fxSpots_)
        registerWith(o);
    for (auto const& o : correlations_)
        registerWith(o.second);

    registerWith(model_);

    // FD only: for one (or no) underlying, everything works as usual

    if (type_ == Type::MC || model_->processes().size() <= 1)
        return;

    // if we have one underlying + one FX index, we do a 1D PDE with a quanto adjustment under certain circumstances

    if (model_->processes().size() == 2) {
        // check whether we have exactly one pay ccy ...
        if (payCcys_.size() == 1) {
            std::string payCcy = *payCcys_.begin();
            // ... and the second index is an FX index suitable to do a quanto adjustment
            // from the first index's currency to the pay ccy ...
            std::string mainIndexCcy = indexCurrencies_[0];
            if (indices_[0].isFx()) {
                mainIndexCcy = indices_[0].fx()->targetCurrency().code();
            }
            if (indices_[1].isFx()) {
                std::string ccy1 = indices_[1].fx()->sourceCurrency().code();
                std::string ccy2 = indices_[1].fx()->targetCurrency().code();
                if ((ccy1 == mainIndexCcy && ccy2 == payCcy) || (ccy1 == payCcy && ccy2 == mainIndexCcy)) {
                    applyQuantoAdjustment_ = true;
                    quantoSourceCcyIndex_ = std::distance(
                        currencies.begin(), std::find(currencies.begin(), currencies.end(), mainIndexCcy));
                    quantoTargetCcyIndex_ =
                        std::distance(currencies.begin(), std::find(currencies.begin(), currencies.end(), payCcy));
                    quantoCorrelationMultiplier_ = ccy2 == payCcy ? 1.0 : -1.0;
                }
                DLOG("BlackScholes model will be run for index '"
                     << indices_[0].name() << "' with a quanto-adjustment " << currencies_[quantoSourceCcyIndex_]
                     << " => " << currencies_[quantoTargetCcyIndex_] << " derived from index '" << indices_[1].name()
                     << "'");
                return;
            }
        }
    }

    // otherwise we need more than one dimension, which we currently not support

    QL_FAIL("BlackScholes: model does not support multi-dim fd schemes currently, use mc instead.");

} // BlackScholes ctor

void BlackScholes::performCalculations() const {

    QL_REQUIRE(!inTrainingPhase_,
               "BlackScholes::performCalculations(): state inTrainingPhase should be false, this was "
               "not resetted appropriately.");

    referenceDate_ = curves_.front()->referenceDate();

    // set up time grid

    effectiveSimulationDates_ = model_->effectiveSimulationDates();

    std::vector<Real> times;
    for (auto const& d : effectiveSimulationDates_) {
        times.push_back(timeFromReference(d));
    }

    timeGrid_ = model_->discretisationTimeGrid();
    positionInTimeGrid_.resize(times.size());
    for (Size i = 0; i < positionInTimeGrid_.size(); ++i)
        positionInTimeGrid_[i] = timeGrid_.index(times[i]);

    underlyingPaths_.clear();
    underlyingPathsTraining_.clear();

    // nothing to do if we do not have any indices

    if (indices_.empty())
        return;

    if (type_ == Model::Type::MC) {
        if (calibration_ == "ATM" || calibration_ == "Deal") {
            performCalculationsMcBs();
        } else {
            performCalculationsMcLv();
        }
    } else if (type_ == Model::Type::FD) {
        if (calibration_ == "ATM" || calibration_ == "Deal") {
            performCalculationsFdBs();
        } else {
            performCalculationsFdLv();
        }
    }
}

void BlackScholes::performCalculationsMcBs() const {
    initUnderlyingPathsMc();
    setReferenceDateValuesMc();
    if (effectiveSimulationDates_.size() == 1)
        return;
    generatePathsBs();
}

void BlackScholes::performCalculationsMcLv() const {
    initUnderlyingPathsMc();
    setReferenceDateValuesMc();
    if (effectiveSimulationDates_.size() == 1)
        return;
    generatePathsLv();
}

void BlackScholes::performCalculationsFdBs() const {

    // 0c if we only have one effective sim date (today), we set the underlying values = spot

    if (effectiveSimulationDates_.size() == 1) {
        underlyingValues_ = RandomVariable(size(), model_->processes()[0]->x0());
        return;
    }

    // 1 set the calibration strikes

    std::vector<Real> calibrationStrikes = getCalibrationStrikes();

    // 1b set up the critical points for the mesher

    std::vector<std::vector<std::tuple<Real, Real, bool>>> cPoints;
    for (Size i = 0; i < indices_.size(); ++i) {
        cPoints.push_back(std::vector<std::tuple<Real, Real, bool>>());
        auto f = calibrationStrikes_.find(indices_[i].name());
        if (f != calibrationStrikes_.end()) {
            for (Size j = 0; j < std::min(f->second.size(), params_.mesherMaxConcentratingPoints); ++j) {
                cPoints.back().push_back(
                    QuantLib::ext::make_tuple(std::log(f->second[j]), params_.mesherConcentration, false));
                TLOG("added critical point at strike " << f->second[j] << " with concentration "
                                                       << params_.mesherConcentration);
            }
        }
    }

    // 2 set up mesher if we do not have one already or if we want to rebuild it every time

    if (mesher_ == nullptr || !params_.staticMesher) {
        mesher_ =
            QuantLib::ext::make_shared<FdmMesherComposite>(QuantLib::ext::make_shared<QuantExt::FdmBlackScholesMesher>(
                size(), model_->processes()[0], timeGrid_.back(),
                calibrationStrikes[0] == Null<Real>()
                    ? atmForward(model_->processes()[0]->x0(), model_->processes()[0]->riskFreeRate(),
                                 model_->processes()[0]->dividendYield(), timeGrid_.back())
                    : calibrationStrikes[0],
                Null<Real>(), Null<Real>(), params_.mesherEpsilon, params_.mesherScaling, cPoints[0]));
    }

    // 3 set up operator using atmf vol and without discounting, floor forward variances at zero

    QuantLib::ext::shared_ptr<QuantExt::FdmQuantoHelper> quantoHelper;

    if (applyQuantoAdjustment_) {
        Real quantoCorr = quantoCorrelationMultiplier_ * getCorrelation()[0][1];
        quantoHelper = QuantLib::ext::make_shared<QuantExt::FdmQuantoHelper>(
            *curves_[quantoTargetCcyIndex_], *curves_[quantoSourceCcyIndex_],
            *model_->processes()[1]->blackVolatility(), quantoCorr, Null<Real>(), model_->processes()[1]->x0(), false,
            true);
    }

    operator_ = QuantLib::ext::make_shared<QuantExt::FdmBlackScholesOp>(
        mesher_, model_->processes()[0], calibrationStrikes[0], false, -static_cast<Real>(Null<Real>()), 0,
        quantoHelper, false, true);

    // 4 set up bwd solver, hardcoded Douglas scheme (= CrankNicholson)

    solver_ = QuantLib::ext::make_shared<FdmBackwardSolver>(
        operator_, std::vector<QuantLib::ext::shared_ptr<BoundaryCondition<FdmLinearOp>>>(), nullptr,
        FdmSchemeDesc::Douglas());

    // 5 fill random variable with underlying values, these are valid for all times

    auto locations = mesher_->locations(0);
    underlyingValues_ = exp(RandomVariable(locations));

    // set additional results provided by this model

    for (Size i = 0; i < calibrationStrikes.size(); ++i) {
        additionalResults_["FdBlackScholes.CalibrationStrike_" + indices_[i].name()] =
            (calibrationStrikes[i] == Null<Real>() ? "ATMF" : std::to_string(calibrationStrikes[i]));
    }

    for (Size i = 0; i < indices_.size(); ++i) {
        Size timeStep = 0;
        for (auto const& d : effectiveSimulationDates_) {
            Real t = timeGrid_[positionInTimeGrid_[timeStep]];
            Real forward = atmForward(model_->processes()[i]->x0(), model_->processes()[i]->riskFreeRate(),
                                      model_->processes()[i]->dividendYield(), t);
            if (timeStep > 0) {
                Real volatility = model_->processes()[i]->blackVolatility()->blackVol(
                    t, calibrationStrikes[i] == Null<Real>() ? forward : calibrationStrikes[i]);
                additionalResults_["FdBlackScholes.Volatility_" + indices_[i].name() + "_" + ore::data::to_string(d)] =
                    volatility;
            }
            additionalResults_["FdBlackScholes.Forward_" + indices_[i].name() + "_" + ore::data::to_string(d)] =
                forward;
            ++timeStep;
        }
    }
}

void BlackScholes::performCalculationsFdLv() const {
    // ************** TODO ***************
}

void BlackScholes::initUnderlyingPathsMc() const {
    for (auto const& d : effectiveSimulationDates_) {
        underlyingPaths_[d] = std::vector<RandomVariable>(model_->processes().size(), RandomVariable(size(), 0.0));
        if (trainingSamples() != Null<Size>())
            underlyingPathsTraining_[d] =
                std::vector<RandomVariable>(model_->processes().size(), RandomVariable(trainingSamples(), 0.0));
    }
}

void BlackScholes::setReferenceDateValuesMc() const {
    for (Size l = 0; l < indices_.size(); ++l) {
        underlyingPaths_[*effectiveSimulationDates_.begin()][l].setAll(model_->processes()[l]->x0());
        if (trainingSamples() != Null<Size>()) {
            underlyingPathsTraining_[*effectiveSimulationDates_.begin()][l].setAll(model_->processes()[l]->x0());
        }
    }
}

void BlackScholes::generatePathsBs() const {

    // compile the correlation matrix

    Matrix correlation = getCorrelation();

    // determine calibration strikes

    std::vector<Real> calibrationStrikes = getCalibrationStrikes();

    // compute drift and covariances of log spots to evolve the process; the covariance computation is done
    // on the refined grid where we assume the volatilities to be constant

    // used for dirf adjustment eq / com that are not in base ccy below
    std::vector<Size> forCcyDaIndex(indices_.size(), Null<Size>());
    for (Size j = 0; j < indices_.size(); ++j) {
        if (!indices_[j].isFx()) {
            // do we have an fx index with the desired currency?
            for (Size jj = 0; jj < indices_.size(); ++jj) {
                if (indices_[jj].isFx()) {
                    if (indexCurrencies_[jj] == indexCurrencies_[j])
                        forCcyDaIndex[j] = jj;
                }
            }
        }
    }

    std::vector<Array> drift(effectiveSimulationDates_.size() - 1, Array(indices_.size(), 0.0));
    std::vector<Matrix> sqrtCov;
    covariance_ =
        std::vector<Matrix>(effectiveSimulationDates_.size() - 1, Matrix(indices_.size(), indices_.size(), 0.0));
    Array variance(indices_.size(), 0.0), discountRatio(indices_.size(), 1.0);
    Size tidx = 1; // index in the refined time grid

    for (Size i = 1; i < effectiveSimulationDates_.size(); ++i) {

        // covariance

        while (tidx <= positionInTimeGrid_[i]) {
            Array d_variance(indices_.size());
            for (Size j = 0; j < indices_.size(); ++j) {
                Real tmp = model_->processes()[j]->blackVolatility()->blackVariance(
                    timeGrid_[tidx],
                    calibrationStrikes[j] == Null<Real>()
                        ? atmForward(model_->processes()[j]->x0(), model_->processes()[j]->riskFreeRate(),
                                     model_->processes()[j]->dividendYield(), timeGrid_[tidx])
                        : calibrationStrikes[j]);
                d_variance[j] = std::max(tmp - variance[j], 1E-20);
                variance[j] = tmp;
            }
            for (Size j = 0; j < indices_.size(); ++j) {
                covariance_[i - 1][j][j] += d_variance[j];
                for (Size k = 0; k < j; ++k) {
                    Real tmp = correlation[k][j] * std::sqrt(d_variance[j] * d_variance[k]);
                    covariance_[i - 1][k][j] += tmp;
                    covariance_[i - 1][j][k] += tmp;
                }
            }
            ++tidx;
        }

        // salvage covariance matrix using spectral method if not positive semidefinite

        SymmetricSchurDecomposition jd(covariance_[i - 1]);

        bool needsSalvaging = false;
        for (Size k = 0; k < covariance_[i - 1].rows(); ++k) {
            if (jd.eigenvalues()[k] < -1E-16)
                needsSalvaging = true;
        }

        if (needsSalvaging) {
            Matrix diagonal(covariance_[i - 1].rows(), covariance_[i - 1].rows(), 0.0);
            for (Size k = 0; k < jd.eigenvalues().size(); ++k) {
                diagonal[k][k] = std::sqrt(std::max<Real>(jd.eigenvalues()[k], 0.0));
            }
            covariance_[i - 1] = jd.eigenvectors() * diagonal * diagonal * transpose(jd.eigenvectors());
        }

        // compute the _unique_ pos semidefinite square root

        sqrtCov.push_back(CholeskyDecomposition(covariance_[i - 1], true));

        // drift

        Date d = *std::next(effectiveSimulationDates_.begin(), i);
        for (Size j = 0; j < indices_.size(); ++j) {
            Real tmp = model_->processes()[j]->riskFreeRate()->discount(d) /
                       model_->processes()[j]->dividendYield()->discount(d);
            drift[i - 1][j] = -std::log(tmp / discountRatio[j]) - 0.5 * covariance_[i - 1][j][j];
            discountRatio[j] = tmp;

            // drift adjustment for eq / com indices that are not in base ccy
            if (forCcyDaIndex[j] != Null<Size>()) {
                drift[i - 1][j] -= covariance_[i - 1][forCcyDaIndex[j]][j];
            }
        }
    }

    // evolve the process using correlated normal variates and set the underlying path values

    populatePathValuesBs(size(), underlyingPaths_,
                         makeMultiPathVariateGenerator(params_.sequenceType, indices_.size(),
                                                       effectiveSimulationDates_.size() - 1, params_.seed,
                                                       params_.sobolOrdering, params_.sobolDirectionIntegers),
                         drift, sqrtCov);

    if (trainingSamples() != Null<Size>()) {
        populatePathValuesBs(trainingSamples(), underlyingPathsTraining_,
                             makeMultiPathVariateGenerator(params_.trainingSequenceType, indices_.size(),
                                                           effectiveSimulationDates_.size() - 1, params_.trainingSeed,
                                                           params_.sobolOrdering, params_.sobolDirectionIntegers),
                             drift, sqrtCov);
    }

    // set additional results provided by this model

    for (Size i = 0; i < indices_.size(); ++i) {
        for (Size j = 0; j < i; ++j) {
            additionalResults_["BlackScholes.Correlation_" + indices_[i].name() + "_" + indices_[j].name()] =
                correlation(i, j);
        }
    }

    for (Size i = 0; i < calibrationStrikes.size(); ++i) {
        additionalResults_["BlackScholes.CalibrationStrike_" + indices_[i].name()] =
            (calibrationStrikes[i] == Null<Real>() ? "ATMF" : std::to_string(calibrationStrikes[i]));
    }

    for (Size i = 0; i < indices_.size(); ++i) {
        Size timeStep = 0;
        for (auto const& d : effectiveSimulationDates_) {
            Real t = timeGrid_[positionInTimeGrid_[timeStep]];
            Real forward = atmForward(model_->processes()[i]->x0(), model_->processes()[i]->riskFreeRate(),
                                      model_->processes()[i]->dividendYield(), t);
            if (timeStep > 0) {
                Real volatility = model_->processes()[i]->blackVolatility()->blackVol(
                    t, calibrationStrikes[i] == Null<Real>() ? forward : calibrationStrikes[i]);
                additionalResults_["BlackScholes.Volatility_" + indices_[i].name() + "_" + ore::data::to_string(d)] =
                    volatility;
            }
            additionalResults_["BlackScholes.Forward_" + indices_[i].name() + "_" + ore::data::to_string(d)] = forward;
            ++timeStep;
        }
    }
} // performCalculationsBS()

void BlackScholes::generatePathsLv() const {

    // compile the correlation matrix

    Matrix correlation = getCorrelation();

    // compute the sqrt correlation

    Matrix sqrtCorr = pseudoSqrt(correlation, params_.salvagingAlgorithm);

    // precompute the deterministic part of the drift on each time step

    std::vector<Array> deterministicDrift(timeGrid_.size() - 1, Array(indices_.size(), 0.0));

    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        for (Size j = 0; j < indices_.size(); ++j) {
            Real t0 = timeGrid_[i];
            Real t1 = timeGrid_[i + 1];
            deterministicDrift[i][j] = -std::log(model_->processes()[j]->riskFreeRate()->discount(t1) /
                                                 model_->processes()[j]->dividendYield()->discount(t1) /
                                                 (model_->processes()[j]->riskFreeRate()->discount(t0) /
                                                  model_->processes()[j]->dividendYield()->discount(t0)));
        }
    }

    // precompute index for drift adjustment for eq / com indices that are not in base ccy

    std::vector<Size> eqComIdx(indices_.size());
    for (Size j = 0; j < indices_.size(); ++j) {
        Size idx = Null<Size>();
        if (!indices_[j].isFx()) {
            // do we have an fx index with the desired currency?
            for (Size jj = 0; jj < indices_.size(); ++jj) {
                if (indices_[jj].isFx()) {
                    if (indexCurrencies_[jj] == indexCurrencies_[j])
                        idx = jj;
                }
            }
        }
        eqComIdx[j] = idx;
    }

    // precompute some time related quantities

    std::vector<Real> t(timeGrid_.size() - 1), dt(timeGrid_.size() - 1), sqrtdt(timeGrid_.size() - 1);
    for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
        t[i] = timeGrid_[i];
        dt[i] = timeGrid_[i + 1] - timeGrid_[i];
        sqrtdt[i] = std::sqrt(dt[i]);
    }

    // evolve the process using correlated normal variates and set the underlying path values

    populatePathValuesLv(size(), underlyingPaths_,
                         makeMultiPathVariateGenerator(params_.sequenceType, indices_.size(), timeGrid_.size() - 1,
                                                       params_.seed, params_.sobolOrdering,
                                                       params_.sobolDirectionIntegers),
                         correlation, sqrtCorr, deterministicDrift, eqComIdx, t, dt, sqrtdt);

    if (trainingSamples() != Null<Size>()) {
        populatePathValuesLv(trainingSamples(), underlyingPathsTraining_,
                             makeMultiPathVariateGenerator(params_.trainingSequenceType, indices_.size(),
                                                           timeGrid_.size() - 1, params_.trainingSeed,
                                                           params_.sobolOrdering, params_.sobolDirectionIntegers),
                             correlation, sqrtCorr, deterministicDrift, eqComIdx, t, dt, sqrtdt);
    }

} // performCalculationsLV()

void BlackScholes::populatePathValuesBs(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                                        const QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>& gen,
                                        const std::vector<Array>& drift, const std::vector<Matrix>& sqrtCov) const {

    std::vector<std::vector<RandomVariable*>> rvs(indices_.size(),
                                                  std::vector<RandomVariable*>(effectiveSimulationDates_.size() - 1));
    auto date = effectiveSimulationDates_.begin();
    for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
        ++date;
        for (Size j = 0; j < indices_.size(); ++j) {
            rvs[j][i] = &paths[*date][j];
            rvs[j][i]->expand();
        }
    }

    Array logState(indices_.size()), logState0(indices_.size());
    for (Size j = 0; j < indices_.size(); ++j) {
        logState0[j] = std::log(model_->processes()[j]->x0());
    }

    for (Size path = 0; path < nSamples; ++path) {
        auto seq = gen->next();
        logState = logState0;
        for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
            for (Size j = 0; j < indices_.size(); ++j) {
                for (Size k = 0; k < indices_.size(); ++k)
                    logState[j] += sqrtCov[i][j][k] * seq.value[i][k];
            }
            logState += drift[i];
            for (Size j = 0; j < indices_.size(); ++j)
                rvs[j][i]->data()[path] = std::exp(logState[j]);
        }
    }
} // populatePathValuesBS()

void BlackScholes::populatePathValuesLv(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
                                        const QuantLib::ext::shared_ptr<MultiPathVariateGeneratorBase>& gen,
                                        const Matrix& correlation, const Matrix& sqrtCorr,
                                        const std::vector<Array>& deterministicDrift, const std::vector<Size>& eqComIdx,
                                        const std::vector<Real>& t, const std::vector<Real>& dt,
                                        const std::vector<Real>& sqrtdt) const {

    Array stateDiff(indices_.size()), logState(indices_.size()), logState0(indices_.size());
    for (Size j = 0; j < indices_.size(); ++j) {
        logState0[j] = std::log(model_->processes()[j]->x0());
    }

    std::vector<std::vector<RandomVariable*>> rvs(indices_.size(),
                                                  std::vector<RandomVariable*>(effectiveSimulationDates_.size() - 1));
    auto date = effectiveSimulationDates_.begin();
    for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
        ++date;
        for (Size j = 0; j < indices_.size(); ++j) {
            rvs[j][i] = &paths[*date][j];
            rvs[j][i]->expand();
        }
    }

    for (Size path = 0; path < size(); ++path) {
        auto p = gen->next();
        logState = logState0;
        std::size_t date = 0;
        auto pos = positionInTimeGrid_.begin();
        ++pos;
        // evolve the process on the refined time grid
        for (Size i = 0; i < timeGrid_.size() - 1; ++i) {
            for (Size j = 0; j < indices_.size(); ++j) {
                // localVol might throw / return nan, inf, handle these cases here
                // by setting the local vol to zero
                Real volj = 0.0;
                try {
                    volj = model_->processes()[j]->localVolatility()->localVol(t[i], std::exp(logState[j]));
                } catch (...) {
                }
                if (!std::isfinite(volj))
                    volj = 0.0;
                Real dw = 0;
                for (Size k = 0; k < indices_.size(); ++k) {
                    dw += sqrtCorr[j][k] * p.value[i][k];
                }
                stateDiff[j] = volj * dw * sqrtdt[i] - 0.5 * volj * volj * dt[i];
                // drift adjustment for eq / com indices that are not in base ccy
                if (eqComIdx[j] != Null<Size>()) {
                    Real volIdx = model_->processes()[eqComIdx[j]]->localVolatility()->localVol(
                        t[i], std::exp(logState[eqComIdx[j]]));
                    stateDiff[j] -= correlation[eqComIdx[j]][j] * volIdx * volj * dt[i];
                }
            }
            // update state with stateDiff from above and deterministic part of the drift
            logState += stateDiff + deterministicDrift[i];
            // on the effective simulation dates populate the underlying paths
            if (i + 1 == *pos) {
                for (Size j = 0; j < indices_.size(); ++j)
                    rvs[j][date]->data()[path] = std::exp(logState[j]);
                ++date;
                ++pos;
            }
        }
    }
} // populatePathValuesLV()

Matrix BlackScholes::getCorrelation() const {
    Matrix correlation(indices_.size(), indices_.size(), 0.0);
    for (Size i = 0; i < indices_.size(); ++i)
        correlation[i][i] = 1.0;
    for (auto const& c : correlations_) {
        IndexInfo inf1(c.first.first), inf2(c.first.second);
        auto ind1 = std::find(indices_.begin(), indices_.end(), inf1);
        auto ind2 = std::find(indices_.begin(), indices_.end(), inf2);
        if (ind1 != indices_.end() && ind2 != indices_.end()) {
            // EQ, FX, COMM index
            Size i1 = std::distance(indices_.begin(), ind1);
            Size i2 = std::distance(indices_.begin(), ind2);
            correlation[i1][i2] = correlation[i2][i1] = c.second->correlation(0.0); // we assume a constant correlation!
        }
    }
    DLOG("BlackScholes correlation matrix:");
    DLOGGERSTREAM(correlation);
    return correlation;
}

std::vector<Real> BlackScholes::getCalibrationStrikes() const {
    std::vector<Real> calibrationStrikes;
    if (calibration_ == "ATM") {
        calibrationStrikes.resize(indices_.size(), Null<Real>());
    } else if (calibration_ == "Deal") {
        for (Size i = 0; i < indices_.size(); ++i) {
            auto f = calibrationStrikes_.find(indices_[i].name());
            if (f != calibrationStrikes_.end() && !f->second.empty()) {
                calibrationStrikes.push_back(f->second[0]);
                TLOG("calibration strike for index '" << indices_[i] << "' is " << f->second[0]);
            } else {
                calibrationStrikes.push_back(Null<Real>());
                TLOG("calibration strike for index '" << indices_[i] << "' is ATMF");
            }
        }
    } else {
        QL_FAIL("BlackScholes::getCalibrationStrikes(): calibration '" << calibration_
                                                                       << "' not supported, expected ATM, Deal");
    }
    return calibrationStrikes;
}

const Date& BlackScholes::referenceDate() const {
    calculate();
    return referenceDate_;
}

RandomVariable BlackScholes::getIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {

    Date effFwd = fwd;
    if (indices_[indexNo].isComm()) {
        Date expiry = indices_[indexNo].comm(d)->expiryDate();
        // if a future is referenced we set the forward date effectively used below to the future's expiry date
        if (expiry != Date())
            effFwd = expiry;
        // if the future expiry is past the obsdate, we return the spot as of the obsdate, i.e. we
        // freeze the future value after its expiry, but keep it available for observation
        // TOOD should we throw an exception instead?
        effFwd = std::max(effFwd, d);
    }

    RandomVariable res;

    if (type_ == Type::FD) {

        res = RandomVariable(underlyingValues_);

        // set the observation time in the result random variable
        res.setTime(timeFromReference(d));

    } else {

        QL_REQUIRE(underlyingPaths_.find(d) != underlyingPaths_.end(), "did not find path for " << d);
        res = underlyingPaths_.at(d).at(indexNo);
    }

    // compute forwarding factor and multiply the result by this factor

    if (effFwd != Null<Date>()) {
        auto p = model_->processes().at(indexNo);
        res *= RandomVariable(size(), p->dividendYield()->discount(effFwd) / p->dividendYield()->discount(d) /
                                          (p->riskFreeRate()->discount(effFwd) / p->riskFreeRate()->discount(d)));
    }

    return res;
}

RandomVariable BlackScholes::getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFixingDate = d;
    if (fwd != Null<Date>())
        effFixingDate = fwd;
    // ensure a valid fixing date
    effFixingDate = irIndices_.at(indexNo).second->fixingCalendar().adjust(effFixingDate);
    return RandomVariable(size(), irIndices_.at(indexNo).second->fixing(effFixingDate));
}

RandomVariable BlackScholes::getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFixingDate = d;
    if (fwd != Null<Date>())
        effFixingDate = fwd;
    return RandomVariable(size(), infIndices_.at(indexNo).second->fixing(effFixingDate));
}

namespace {
struct comp {
    comp(const std::string& indexInput) : indexInput_(indexInput) {}
    template <typename T> bool operator()(const std::pair<IndexInfo, QuantLib::ext::shared_ptr<T>>& p) const {
        return p.first.name() == indexInput_;
    }
    const std::string indexInput_;
};
} // namespace

RandomVariable BlackScholes::fwdCompAvg(const bool isAvg, const std::string& indexInput, const Date& obsdate,
                                        const Date& start, const Date& end, const Real spread, const Real gearing,
                                        const Integer lookback, const Natural rateCutoff, const Natural fixingDays,
                                        const bool includeSpread, const Real cap, const Real floor,
                                        const bool nakedOption, const bool localCapFloor) const {
    calculate();
    auto index = std::find_if(irIndices_.begin(), irIndices_.end(), comp(indexInput));
    QL_REQUIRE(index != irIndices_.end(),
               "BlackScholes::fwdCompAvg(): did not find ir index " << indexInput << " - this is unexpected.");
    auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index->second);
    QL_REQUIRE(on, "BlackScholes::fwdCompAvg(): expected on index for " << indexInput);
    // if we want to support cap / floors we need the OIS CF surface
    QL_REQUIRE(cap > 999998.0 && floor < -999998.0,
               "BlackScholes:fwdCompAvg(): cap (" << cap << ") / floor (" << floor << ") not supported");
    QuantLib::ext::shared_ptr<QuantLib::FloatingRateCoupon> coupon;
    QuantLib::ext::shared_ptr<QuantLib::FloatingRateCouponPricer> pricer;
    if (isAvg) {
        coupon = QuantLib::ext::make_shared<QuantExt::AverageONIndexedCoupon>(
            end, 1.0, start, end, on, gearing, spread, rateCutoff, on->dayCounter(), lookback * Days, fixingDays);
        pricer = QuantLib::ext::make_shared<AverageONIndexedCouponPricer>();
    } else {
        coupon = QuantLib::ext::make_shared<QuantExt::OvernightIndexedCoupon>(
            end, 1.0, start, end, on, gearing, spread, Date(), Date(), on->dayCounter(), false, includeSpread,
            lookback * Days, rateCutoff, fixingDays);
        pricer = QuantLib::ext::make_shared<OvernightIndexedCouponPricer>();
    }
    coupon->setPricer(pricer);
    return RandomVariable(size(), coupon->rate());
}

RandomVariable BlackScholes::getDiscount(const Size idx, const Date& s, const Date& t) const {
    return RandomVariable(size(), curves_.at(idx)->discount(t) / curves_.at(idx)->discount(s));
}

RandomVariable BlackScholes::getNumeraire(const Date& s) const {
    if (!applyQuantoAdjustment_)
        return RandomVariable(size(), 1.0 / curves_.at(0)->discount(s));
    else
        return RandomVariable(size(), 1.0 / curves_.at(quantoTargetCcyIndex_)->discount(s));
}

Real BlackScholes::getFxSpot(const Size idx) const { return fxSpots_.at(idx)->value(); }

RandomVariable BlackScholes::npv(const RandomVariable& amount, const Date& obsdate, const Filter& filter,
                                 const boost::optional<long>& memSlot, const RandomVariable& addRegressor1,
                                 const RandomVariable& addRegressor2) const {

    calculate();

    if (type_ == Type::FD) {

        // handle FD calculation

        QL_REQUIRE(!memSlot, "BlackScholes::npv(): mem slot not allowed");
        QL_REQUIRE(!filter.initialised(), "BlackScholes::npv(). filter not allowed");
        QL_REQUIRE(!addRegressor1.initialised(), "BlackScholes::npv(). addRegressor1 not allowed");
        QL_REQUIRE(!addRegressor2.initialised(), "BlackScholes::npv(). addRegressor2 not allowed");

        Real t1 = amount.time();
        Real t0 = timeFromReference(obsdate);

        // handle case when amount is deterministic

        if (amount.deterministic()) {
            RandomVariable result(amount);
            result.setTime(t0);
            return result;
        }

        // handle stochastic amount

        QL_REQUIRE(t1 != Null<Real>(),
                   "BlackScholes::npv(): can not roll back amount wiithout time attached (to t0=" << t0 << ")");

        // might throw if t0, t1 are not found in timeGrid_

        Size ind1 = timeGrid_.index(t1);
        Size ind0 = timeGrid_.index(t0);

        // check t0 <= t1, i.e. ind0 <= ind1

        QL_REQUIRE(ind0 <= ind1, "BlackScholes::npv(): can not roll back from t1= "
                                     << t1 << " (index " << ind1 << ") to t0= " << t0 << " (" << ind0 << ")");

        // if t0 = t1, no rollback is necessary and we can return the input random variable

        if (ind0 == ind1)
            return amount;

        // if t0 < t1, we roll back on the time grid

        Array workingArray(amount.size());
        amount.copyToArray(workingArray);

        for (int j = static_cast<int>(ind1) - 1; j >= static_cast<int>(ind0); --j) {
            solver_->rollback(workingArray, timeGrid_[j + 1], timeGrid_[j], 1, 0);
        }

        // return the rolled back value

        return RandomVariable(workingArray, t0);

    } else if (type_ == Type::MC) {

        // handle MC calculation

        // short cut, if amount is deterministic and no memslot is given

        if (amount.deterministic() && !memSlot)
            return amount;

        // if obsdate is today, take a plain expectation

        if (obsdate == referenceDate())
            return expectation(amount);

        // build the state

        std::vector<const RandomVariable*> state;

        if (!underlyingPaths_.empty()) {
            for (auto const& r : underlyingPaths_.at(obsdate))
                state.push_back(&r);
        }

        Size nModelStates = state.size();

        if (addRegressor1.initialised() && (memSlot || !addRegressor1.deterministic()))
            state.push_back(&addRegressor1);
        if (addRegressor2.initialised() && (memSlot || !addRegressor2.deterministic()))
            state.push_back(&addRegressor2);

        Size nAddReg = state.size() - nModelStates;

        // if the state is empty, return the plain expectation (no conditioning)

        if (state.empty()) {
            return expectation(amount);
        }

        // the regression model is given by coefficients and an optional coordinate transform

        Array coeff;
        Matrix coordinateTransform;

        // if a memSlot is given and coefficients / coordinate transform are stored, we use them

        bool haveStoredModel = false;

        if (memSlot) {
            if (auto it = storedRegressionModel_.find(*memSlot); it != storedRegressionModel_.end()) {
                coeff = std::get<0>(it->second);
                coordinateTransform = std::get<2>(it->second);
                QL_REQUIRE(std::get<1>(it->second) == state.size(),
                           "BlackScholes::npv(): stored regression coefficients at mem slot "
                               << *memSlot << " are for state size " << std::get<1>(it->second)
                               << ", actual state size is " << state.size()
                               << " (before possible coordinate transform).");
                haveStoredModel = true;
            }
        }

        // if we do not have retrieved a model in the previous step, we create it now

        std::vector<RandomVariable> transformedState;

        if (!haveStoredModel) {

            // factor reduction to reduce dimensionalitty and handle collinearity

            if (params_.regressionVarianceCutoff != Null<Real>()) {
                coordinateTransform = pcaCoordinateTransform(state, params_.regressionVarianceCutoff);
                transformedState = applyCoordinateTransform(state, coordinateTransform);
                state = vec2vecptr(transformedState);
            }

            // train coefficients

            coeff =
                regressionCoefficients(amount, state,
                                       multiPathBasisSystem(state.size(), params_.regressionOrder, params_.polynomType,
                                                            {}, std::min(size(), trainingSamples())),
                                       filter, RandomVariableRegressionMethod::QR);
            DLOG("BlackScholes::npv(" << ore::data::to_string(obsdate) << "): regression coefficients are " << coeff
                                      << " (got model state size " << nModelStates << " and " << nAddReg
                                      << " additional regressors, coordinate transform "
                                      << coordinateTransform.columns() << " -> " << coordinateTransform.rows() << ")");

            // store model if requried

            if (memSlot) {
                storedRegressionModel_[*memSlot] = std::make_tuple(coeff, nModelStates, coordinateTransform);
            }

        } else {

            // apply the stored coordinate transform to the state

            if (!coordinateTransform.empty()) {
                transformedState = applyCoordinateTransform(state, coordinateTransform);
                state = vec2vecptr(transformedState);
            }
        }

        // compute conditional expectation and return the result

        return conditionalExpectation(state,
                                      multiPathBasisSystem(state.size(), params_.regressionOrder, params_.polynomType,
                                                           {}, std::min(size(), trainingSamples())),
                                      coeff);
    } else {
        QL_FAIL("BlackScholes::npv(): unhandled type, internal error.");
    }
}

RandomVariable BlackScholes::getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                                  const RandomVariable& barrier, const bool above) const {

    QL_REQUIRE(calibration_ != "LocalVol",
               "BlackScholes::getFutureBarrierProb(): not implemented for calibration == LocalVol");

    // get the underlying values at the start and end points of the period

    RandomVariable v1 = eval(index, obsdate1, Null<Date>());
    RandomVariable v2 = eval(index, obsdate2, Null<Date>());

    // check the barrier at the two endpoints

    Filter barrierHit(barrier.size(), false);
    if (above) {
        barrierHit = barrierHit || v1 >= barrier;
        barrierHit = barrierHit || v2 >= barrier;
    } else {
        barrierHit = barrierHit || v1 <= barrier;
        barrierHit = barrierHit || v2 <= barrier;
    }

    // for IR / INF indices we have to check each date, this is fast though, since the index values are deteministic
    // here

    auto ir = std::find_if(irIndices_.begin(), irIndices_.end(), comp(index));
    auto inf = std::find_if(infIndices_.begin(), infIndices_.end(), comp(index));
    if (ir != irIndices_.end() || inf != infIndices_.end()) {
        Date d = obsdate1 + 1;
        while (d < obsdate2) {
            RandomVariable res;
            if (ir != irIndices_.end())
                res = getIrIndexValue(std::distance(irIndices_.begin(), ir), d);
            else
                res = getInfIndexValue(std::distance(infIndices_.begin(), inf), d);
            if (res.initialised()) {
                if (above) {
                    barrierHit = barrierHit || res >= barrier;
                } else {
                    barrierHit = barrierHit || res <= barrier;
                }
            }
            ++d;
        }
    }

    // Convert the bool result we have obtained so far to 1 / 0

    RandomVariable result(barrierHit, 1.0, 0.0);

    // return the result if we have an IR / INF index

    if (ir != irIndices_.end() || inf != infIndices_.end())
        return result;

    // Now handle the dynamic indices in this model

    // first make sure that v1 is not a historical fixing to avoid differences to a daily MC simulation where
    // the process starts to evolve at the market data spot (only matters if the two values differ, which they
    // should not too much anyway)

    if (obsdate1 == referenceDate()) {
        v1 = eval(index, obsdate1, Null<Date>(), false, true);
    }

    IndexInfo indexInfo(index);

    // if we have an FX index, we "normalise" the tag by GENERIC (since it does not matter for projections), this
    // is the same as in ModelImpl::eval()

    if (indexInfo.isFx())
        indexInfo = IndexInfo("FX-GENERIC-" + indexInfo.fx()->sourceCurrency().code() + "-" +
                              indexInfo.fx()->targetCurrency().code());

    // get the average volatility over the period for the underlying, this is an approximation that could be refined
    // by sampling more points in the interval and doing the below calculation for each subinterval - this would
    // be slower though, so probably in the end we want this to be configurable in the model settings

    // We might have one or two indices contributing to the desired volatility, since FX indices might require a
    // triangulation. We look for the indices ind1 and ind2 so that the index is the quotient of the two.

    Size ind1 = Null<Size>(), ind2 = Null<Size>();

    auto i = std::find(indices_.begin(), indices_.end(), indexInfo);
    if (i != indices_.end()) {
        // we find the index directly in the index list
        ind1 = std::distance(indices_.begin(), i);
    } else {
        // we can maybe triangulate an FX index (again, this is similar to ModelImpl::eval())
        // if not, we can only try something else for FX indices
        QL_REQUIRE(indexInfo.isFx(), "BlackScholes::getFutureBarrierProb(): index " << index << " not handled");
        // is it a trivial fx index (CCY-CCY) or can we triangulate it?
        if (indexInfo.fx()->sourceCurrency() == indexInfo.fx()->targetCurrency()) {
            // pseudo FX index FX-GENERIC-CCY-CCY, leave ind1 and ind2 both set to zero
        } else {
            // if not, we can only try something else for FX indices
            QL_REQUIRE(indexInfo.isFx(), "BlackScholes::getFutureBarrierProb(): index " << index << " not handled");
            if (indexInfo.fx()->sourceCurrency() == indexInfo.fx()->targetCurrency()) {
                // nothing to do, leave ind1 and ind2 = null
            } else {
                for (Size i = 0; i < indexCurrencies_.size(); ++i) {
                    if (indices_[i].isFx()) {
                        if (indexInfo.fx()->sourceCurrency().code() == indexCurrencies_[i])
                            ind1 = i;
                        if (indexInfo.fx()->targetCurrency().code() == indexCurrencies_[i])
                            ind2 = i;
                    }
                }
            }
        }
    }

    // accumulate the variance contributions over [obsdate1, obsdate2]

    Real variance = 0.0;

    for (Size i = 1; i < effectiveSimulationDates_.size(); ++i) {

        Date d1 = *std::next(effectiveSimulationDates_.begin(), i - 1);
        Date d2 = *std::next(effectiveSimulationDates_.begin(), i);

        if (obsdate1 <= d1 && d2 <= obsdate2) {
            if (ind1 != Null<Size>()) {
                variance += covariance_[i - 1][ind1][ind1];
            }
            if (ind2 != Null<Size>()) {
                variance += covariance_[i - 1][ind2][ind2];
            }
            if (ind1 != Null<Size>() && ind2 != Null<Size>()) {
                variance -= 2.0 * covariance_[i - 1][ind1][ind2];
            }
        }
    }

    // compute the hit probability
    // see e.g. formulas 2, 4 in Emmanuel Gobet, Advanced Monte Carlo methods for barrier and related exotic options

    if (!QuantLib::close_enough(variance, 0.0)) {
        RandomVariable eps = RandomVariable(barrier.size(), 1E-14);
        RandomVariable hitProb = exp(RandomVariable(barrier.size(), -2.0 / variance) * log(v1 / max(barrier, eps)) *
                                     log(v2 / max(barrier, eps)));
        result = result + applyInverseFilter(hitProb, barrierHit);
    }

    return result;
}

void BlackScholes::releaseMemory() {
    underlyingPaths_.clear();
    underlyingPathsTraining_.clear();
}

void BlackScholes::resetNPVMem() { storedRegressionModel_.clear(); }

void BlackScholes::toggleTrainingPaths() const {
    std::swap(underlyingPaths_, underlyingPathsTraining_);
    inTrainingPhase_ = !inTrainingPhase_;
}

Size BlackScholes::trainingSamples() const { return params_.trainingSamples; }

Size BlackScholes::size() const {
    if (inTrainingPhase_)
        return params_.trainingSamples;
    else
        return Model::size();
}

const std::string& BlackScholes::baseCcy() const {
    if (!applyQuantoAdjustment_)
        return ModelImpl::baseCcy();
    return currencies_[quantoTargetCcyIndex_];
}

Real BlackScholes::extractT0Result(const RandomVariable& value) const {

    if (type_ == Type::MC)
        return ModelImpl::extractT0Result(value);

    // specific code for FD:

    calculate();

    // roll back to today (if necessary)

    RandomVariable r = npv(value, referenceDate(), Filter(), boost::none, RandomVariable(), RandomVariable());

    // if result is deterministic, return the value

    if (r.deterministic())
        return r.at(0);

    // otherwise interpolate the result at the spot of the underlying process

    Array x(underlyingValues_.size());
    Array y(underlyingValues_.size());
    underlyingValues_.copyToArray(x);
    r.copyToArray(y);
    MonotonicCubicNaturalSpline interpolation(x.begin(), x.end(), y.begin());
    interpolation.enableExtrapolation();
    return interpolation(model_->processes()[0]->x0());
}

RandomVariable BlackScholes::pay(const RandomVariable& amount, const Date& obsdate, const Date& paydate,
                                 const std::string& currency) const {

    if (type_ == Type::MC)
        return ModelImpl::pay(amount, obsdate, paydate, currency);

    // specific code for FD:

    calculate();

    if (!applyQuantoAdjustment_) {
        auto res = ModelImpl::pay(amount, obsdate, paydate, currency);
        res.setTime(timeFromReference(obsdate));
        return res;
    }

    QL_REQUIRE(currency == currencies_[quantoTargetCcyIndex_],
               "pay ccy is '" << currency << "', expected '" << currencies_[quantoTargetCcyIndex_]
                              << "' in quanto-adjusted FDBlackScholesBase model");

    Date effectiveDate = std::max(obsdate, referenceDate());

    auto res = amount * getDiscount(quantoTargetCcyIndex_, effectiveDate, paydate) / getNumeraire(effectiveDate);
    res.setTime(timeFromReference(obsdate));
    return res;
}

} // namespace data
} // namespace ore
