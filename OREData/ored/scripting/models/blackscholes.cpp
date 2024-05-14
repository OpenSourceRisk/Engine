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

#include <ored/utilities/to_string.hpp>
#include <ored/model/utilities.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrixutilities/choleskydecomposition.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

BlackScholes::BlackScholes(const Size paths, const std::string& currency, const Handle<YieldTermStructure>& curve,
                           const std::string& index, const std::string& indexCurrency,
                           const Handle<BlackScholesModelWrapper>& model, const McParams& mcParams,
                           const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig,
                           const std::string& calibration, const std::vector<Real>& calibrationStrikes)
    : BlackScholes(paths, {currency}, {curve}, {}, {}, {}, {index}, {indexCurrency}, model, {}, mcParams,
                   simulationDates, iborFallbackConfig, calibration, {{index, calibrationStrikes}}) {}

BlackScholes::BlackScholes(
    const Size paths, const std::vector<std::string>& currencies, const std::vector<Handle<YieldTermStructure>>& curves,
    const std::vector<Handle<Quote>>& fxSpots,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
    const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
    const Handle<BlackScholesModelWrapper>& model,
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
    const McParams& mcParams, const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig,
    const std::string& calibration, const std::map<std::string, std::vector<Real>>& calibrationStrikes)
    : BlackScholesBase(paths, currencies, curves, fxSpots, irIndices, infIndices, indices, indexCurrencies, model,
                       correlations, mcParams, simulationDates, iborFallbackConfig),
      calibration_(calibration), calibrationStrikes_(calibrationStrikes) {}

void BlackScholes::performCalculations() const {

    BlackScholesBase::performCalculations();

    // nothing to do if we do not have any indices

    if (indices_.empty())
        return;

    // init underlying path where we map a date to a randomvariable representing the path values

    for (auto const& d : effectiveSimulationDates_) {
        underlyingPaths_[d] = std::vector<RandomVariable>(model_->processes().size(), RandomVariable(size(), 0.0));
        if (trainingSamples() != Null<Size>())
            underlyingPathsTraining_[d] =
                std::vector<RandomVariable>(model_->processes().size(), RandomVariable(trainingSamples(), 0.0));
    }

    // compile the correlation matrix

    Matrix correlation = getCorrelation();

    // determine calibration strikes

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
        QL_FAIL("BlackScholes: calibration '" << calibration_ << "' not supported, expected ATM, Deal");
    }

    // set reference date values, if there are no future simulation dates we are done

    for (Size l = 0; l < indices_.size(); ++l) {
        underlyingPaths_[*effectiveSimulationDates_.begin()][l].setAll(model_->processes()[l]->x0());
        if (trainingSamples() != Null<Size>()) {
            underlyingPathsTraining_[*effectiveSimulationDates_.begin()][l].setAll(model_->processes()[l]->x0());
        }
    }

    if (effectiveSimulationDates_.size() == 1)
        return;

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

    populatePathValues(size(), underlyingPaths_,
                       makeMultiPathVariateGenerator(mcParams_.sequenceType, indices_.size(),
                                                     effectiveSimulationDates_.size() - 1, mcParams_.seed,
                                                     mcParams_.sobolOrdering, mcParams_.sobolDirectionIntegers),
                       drift, sqrtCov);

    if (trainingSamples() != Null<Size>()) {
        populatePathValues(trainingSamples(), underlyingPathsTraining_,
                           makeMultiPathVariateGenerator(mcParams_.trainingSequenceType, indices_.size(),
                                                         effectiveSimulationDates_.size() - 1, mcParams_.trainingSeed,
                                                         mcParams_.sobolOrdering, mcParams_.sobolDirectionIntegers),
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
}

void BlackScholes::populatePathValues(const Size nSamples, std::map<Date, std::vector<RandomVariable>>& paths,
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
} // populatePathValues()

namespace {
struct comp {
    comp(const std::string& indexInput) : indexInput_(indexInput) {}
    template <typename T> bool operator()(const std::pair<IndexInfo, QuantLib::ext::shared_ptr<T>>& p) const {
        return p.first.name() == indexInput_;
    }
    const std::string indexInput_;
};
} // namespace

RandomVariable BlackScholes::getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                                  const RandomVariable& barrier, const bool above) const {

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

} // namespace data
} // namespace ore
