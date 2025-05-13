/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/scripting/models/blackscholescg.hpp>

#include <ored/model/utilities.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/ad/computationgraph.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/math/randomvariable_ops.hpp>
#include <qle/math/randomvariablelsmbasissystem.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrixutilities/choleskydecomposition.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/quotes/simplequote.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

BlackScholesCG::BlackScholesCG(const ModelCG::Type type, const Size paths, const std::string& currency,
                               const Handle<YieldTermStructure>& curve, const std::string& index,
                               const std::string& indexCurrency, const Handle<BlackScholesModelWrapper>& model,
                               const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig,
                               const std::string& calibration, const std::vector<Real>& calibrationStrikes)
    : BlackScholesCG(type, paths, {currency}, {curve}, {}, {}, {}, {index}, {indexCurrency}, model, {}, simulationDates,
                     iborFallbackConfig) {}

BlackScholesCG::BlackScholesCG(
    const ModelCG::Type type, const Size paths, const std::vector<std::string>& currencies,
    const std::vector<Handle<YieldTermStructure>>& curves, const std::vector<Handle<Quote>>& fxSpots,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<InterestRateIndex>>>& irIndices,
    const std::vector<std::pair<std::string, QuantLib::ext::shared_ptr<ZeroInflationIndex>>>& infIndices,
    const std::vector<std::string>& indices, const std::vector<std::string>& indexCurrencies,
    const Handle<BlackScholesModelWrapper>& model,
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
    const std::set<Date>& simulationDates, const IborFallbackConfig& iborFallbackConfig, const std::string& calibration,
    const std::map<std::string, std::vector<Real>>& calibrationStrikes)
    : ModelCGImpl(type, curves.at(0)->dayCounter(), paths, currencies, irIndices, infIndices, indices, indexCurrencies,
                  simulationDates, iborFallbackConfig),
      curves_(curves), fxSpots_(fxSpots), model_(model), correlations_(correlations) {

    QL_REQUIRE(type == ModelCG::Type::MC, "BlackScholesCG: FD is not yet supported as a model type");

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

    // register with observables

    for (auto const& o : fxSpots_)
        registerWith(o);
    for (auto const& o : correlations_)
        registerWith(o.second);

    registerWith(model_);

} // BlackScholesBase ctor

namespace {

struct SqrtCovCalculator : public QuantLib::LazyObject {

    SqrtCovCalculator(
        const std::vector<IndexInfo>& indices, const std::vector<std::string>& indexCurrencies,
        const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations,
        const std::set<Date>& effectiveSimulationDates, const TimeGrid& timeGrid,
        const std::vector<Size>& positionInTimeGrid, const Handle<BlackScholesModelWrapper>& model,
        const std::vector<Real>& calibrationStrikes)
        : indices_(indices), indexCurrencies_(indexCurrencies), correlations_(correlations),
          effectiveSimulationDates_(effectiveSimulationDates), timeGrid_(timeGrid),
          positionInTimeGrid_(positionInTimeGrid), model_(model), calibrationStrikes_(calibrationStrikes) {

        // register with observables

        for (auto const& c : correlations)
            registerWith(c.second);

        registerWith(model_);

        // setup members

        sqrtCov_ = std::vector<Matrix>(effectiveSimulationDates_.size() - 1);
        covariance_ =
            std::vector<Matrix>(effectiveSimulationDates_.size() - 1, Matrix(indices_.size(), indices_.size()));
        lastCovariance_ = std::vector<Matrix>(effectiveSimulationDates_.size() - 1);
    }

    // helper function to build correlation matrix

    Matrix getCorrelation() const {
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
                correlation[i1][i2] = correlation[i2][i1] =
                    c.second->correlation(0.0); // we assume a constant correlation!
            }
        }
        TLOG("BlackScholesBase correlation matrix:");
        TLOGGERSTREAM(correlation);
        return correlation;
    }

    // the main calc function

    void performCalculations() const override {

        // if there are no future simulation dates we are done

        if (effectiveSimulationDates_.size() == 1)
            return;

        // compile the correlation matrix

        Matrix correlation = getCorrelation();

        // compute covariances of log spots to evolve the process; the covariance computation is done
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

        Array variance(indices_.size(), 0.0), discountRatio(indices_.size(), 1.0);
        Size tidx = 1; // index in the refined time grid
        for (Size i = 1; i < effectiveSimulationDates_.size(); ++i) {
            std::fill(covariance_[i - 1].begin(), covariance_[i - 1].end(), 0.0);
            // covariance
            while (tidx <= positionInTimeGrid_[i]) {
                Array d_variance(indices_.size());
                for (Size j = 0; j < indices_.size(); ++j) {
                    Real tmp = model_->processes()[j]->blackVolatility()->blackVariance(
                        timeGrid_[tidx],
                        calibrationStrikes_[j] == Null<Real>()
                            ? atmForward(model_->processes()[j]->x0(), model_->processes()[j]->riskFreeRate(),
                                         model_->processes()[j]->dividendYield(), timeGrid_[tidx])
                            : calibrationStrikes_[j]);
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
        }

        // update lastCovariance and sqrt if there is a change

        for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {

            bool covarianceHasChanged = lastCovariance_[i].rows() == 0;
            for (Size r = 0; r < lastCovariance_[i].rows() && !covarianceHasChanged; ++r) {
                for (Size c = 0; c < lastCovariance_[i].columns(); ++c) {
                    if (!close_enough(lastCovariance_[i](r, c), covariance_[i](r, c))) {
                        covarianceHasChanged = true;
                        break;
                    }
                }
            }

            if (covarianceHasChanged) {
                lastCovariance_[i] = covariance_[i];

                // salvage matrix using spectral method if not positive semidefinite

                SymmetricSchurDecomposition jd(covariance_[i]);

                bool needsSalvaging = false;
                for (Size k = 0; k < covariance_[i].rows(); ++k) {
                    if (jd.eigenvalues()[k] < -1E-16)
                        needsSalvaging = true;
                }

                if (needsSalvaging) {
                    Matrix diagonal(covariance_[i].rows(), covariance_[i].rows(), 0.0);
                    for (Size k = 0; k < jd.eigenvalues().size(); ++k) {
                        diagonal[k][k] = std::sqrt(std::max<Real>(jd.eigenvalues()[k], 0.0));
                    }
                    covariance_[i] = jd.eigenvectors() * diagonal * diagonal * transpose(jd.eigenvectors());
                }

                // compute the _unique_ pos semidefinite square root

                sqrtCov_[i] = CholeskyDecomposition(covariance_[i], true);
            }
        }
    }

    Real sqrtCov(Size i, Size j, Size k) {
        calculate();
        return sqrtCov_[i][j][k];
    }

    Real cov(Size i, Size j, Size k) {
        calculate();
        return covariance_[i][j][k];
    }

    mutable std::vector<Matrix> sqrtCov_;
    mutable std::vector<Matrix> covariance_;
    mutable std::vector<Matrix> lastCovariance_;

    // refs to members of BlackScholesCG and its base classes that are needed in the calculator
    const std::vector<IndexInfo>& indices_;
    const std::vector<std::string>& indexCurrencies_;
    const std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>>& correlations_;
    const std::set<Date>& effectiveSimulationDates_;
    const TimeGrid& timeGrid_;
    const std::vector<Size>& positionInTimeGrid_;
    const Handle<BlackScholesModelWrapper>& model_;

    // inputs that we need to copy
    const std::vector<Real> calibrationStrikes_;

}; // SqrtCovCalculator

} // namespace

const Date& BlackScholesCG::referenceDate() const {
    calculate();
    return referenceDate_;
}

void BlackScholesCG::performCalculations() const {

    // needed for base class performCalculations()

    referenceDate_ = curves_.front()->referenceDate();

    // update cg version if necessary (eval date changed)

    ModelCGImpl::performCalculations();

    // if cg version has changed => update time grid related members and clear paths, so that they
    // are populated in derived classes

    if (cgVersion() != underlyingPathsCgVersion_) {

        // set up time grid

        effectiveSimulationDates_ = model_->effectiveSimulationDates();

        std::vector<Real> times;
        for (auto const& d : effectiveSimulationDates_) {
            times.push_back(curves_.front()->timeFromReference(d));
        }

        timeGrid_ = model_->discretisationTimeGrid();
        positionInTimeGrid_.resize(times.size());
        for (Size i = 0; i < positionInTimeGrid_.size(); ++i)
            positionInTimeGrid_[i] = timeGrid_.index(times[i]);

        underlyingPaths_.clear();
        underlyingPathsCgVersion_ = cgVersion();
    }

    // nothing to do if we do not have any indices or if underlying paths are populated already

    if (indices_.empty() || !underlyingPaths_.empty())
        return;

    // exit if there are no future simulation dates (i.e. only the reference date)

    if (effectiveSimulationDates_.size() == 1)
        return;

    // init underlying path where we map a date to a randomvariable representing the path values

    for (auto const& d : effectiveSimulationDates_) {
        underlyingPaths_[d] = std::vector<std::size_t>(model_->processes().size(), ComputationGraph::nan);
    }

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

    // generate computation graph of underlying paths dependend on the drift and sqrtCov between simulation
    // dates, which we treat as model parameters

    auto sqrtCovCalc = QuantLib::ext::make_shared<SqrtCovCalculator>(indices_, indexCurrencies_, correlations_,
                                                                     effectiveSimulationDates_, timeGrid_,
                                                                     positionInTimeGrid_, model_, calibrationStrikes);

    std::vector<std::vector<std::size_t>> drift(effectiveSimulationDates_.size() - 1,
                                                std::vector<std::size_t>(indices_.size(), ComputationGraph::nan));
    std::vector<std::vector<std::vector<std::size_t>>> sqrtCov(
        effectiveSimulationDates_.size() - 1,
        std::vector<std::vector<std::size_t>>(indices_.size(),
                                              std::vector<std::size_t>(indices_.size(), ComputationGraph::nan)));
    std::vector<std::vector<std::vector<std::size_t>>> cov(
        effectiveSimulationDates_.size() - 1,
        std::vector<std::vector<std::size_t>>(indices_.size(),
                                              std::vector<std::size_t>(indices_.size(), ComputationGraph::nan)));

    // precompute sqrtCov nodes and add model parameters for covariance (needed in futureBarrierProbability())

    for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
        Date date = *std::next(effectiveSimulationDates_.begin(), i);
        for (Size j = 0; j < indices_.size(); ++j) {
            for (Size k = 0; k < indices_.size(); ++k) {
                sqrtCov[i][j][k] = addModelParameter(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::sqrtCov, {}, {}, date, {}, {}, j, k),
                    [sqrtCovCalc, i, j, k] { return sqrtCovCalc->sqrtCov(i, j, k); });
                cov[i][j][k] = addModelParameter(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::cov, {}, {}, date, {}, {}, j, k),
                    [sqrtCovCalc, i, j, k] { return sqrtCovCalc->cov(i, j, k); });
            }
        }
    }

    // precompute drift nodes

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

    std::vector<std::size_t> discountRatio(indices_.size(), cg_const(*g_, 1.0));
    auto date = effectiveSimulationDates_.begin();
    for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
        Date d = *(++date);
        for (Size j = 0; j < indices_.size(); ++j) {
            auto p = model_->processes().at(j);
            std::string id = std::to_string(j) + "_" + ore::data::to_string(d);
            std::size_t div =
                addModelParameter(ModelCG::ModelParameter(ModelCG::ModelParameter::Type::div, {}, {}, d, {}, {}, j),
                                  [p, d]() { return p->dividendYield()->discount(d); });
            std::size_t rfr =
                addModelParameter(ModelCG::ModelParameter(ModelCG::ModelParameter::Type::rfr, {}, {}, d, {}, {}, j),
                                  [p, d]() { return p->riskFreeRate()->discount(d); });
            std::size_t tmp = cg_div(*g_, rfr, div);
            drift[i][j] = cg_subtract(*g_, cg_negative(*g_, cg_log(*g_, cg_div(*g_, tmp, discountRatio[j]))),
                                      cg_mult(*g_, cg_const(*g_, 0.5), cov[i][j][j]));
            discountRatio[j] = tmp;
            if (forCcyDaIndex[j] != Null<Size>()) {
                drift[i][j] = cg_subtract(*g_, drift[i][j], cov[i][forCcyDaIndex[j]][j]);
            }
        }
    }

    // set required random variates

    randomVariates_ = std::vector<std::vector<std::size_t>>(
        indices_.size(), std::vector<std::size_t>(effectiveSimulationDates_.size() - 1));

    for (Size j = 0; j < indices_.size(); ++j) {
        for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
            randomVariates_[j][i] = cg_var(*g_, "__rv_" + std::to_string(j) + "_" + std::to_string(i),
                                           ComputationGraph::VarDoesntExist::Create);
        }
    }

    // evolve the process using correlated normal variates and set the underlying path values

    std::vector<std::size_t> logState(indices_.size());
    for (Size j = 0; j < indices_.size(); ++j) {
        auto p = model_->processes().at(j);
        logState[j] =
            addModelParameter(ModelCG::ModelParameter(ModelCG::ModelParameter::Type::logX0, {}, {}, {}, {}, {}, j),
                              [p] { return std::log(p->x0()); });
        underlyingPaths_[*effectiveSimulationDates_.begin()][j] = cg_exp(*g_, logState[j]);
    }

    date = effectiveSimulationDates_.begin();
    for (Size i = 0; i < effectiveSimulationDates_.size() - 1; ++i) {
        ++date;
        for (Size j = 0; j < indices_.size(); ++j) {
            for (Size k = 0; k < indices_.size(); ++k) {
                logState[j] = cg_add(*g_, logState[j], cg_mult(*g_, sqrtCov[i][j][k], randomVariates_[k][i]));
            }
            logState[j] = cg_add(*g_, logState[j], drift[i][j]);
            underlyingPaths_[*date][j] = cg_exp(*g_, logState[j]);
        }
    }

    // set additional results provided by this model

    for (auto const& c : correlations_) {
        additionalResults_["BlackScholes.Correlation_" + c.first.first + "_" + c.first.second] =
            c.second->correlation(0.0);
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

namespace {
struct comp {
    comp(const std::string& indexInput) : indexInput_(indexInput) {}
    template <typename T> bool operator()(const std::pair<IndexInfo, QuantLib::ext::shared_ptr<T>>& p) const {
        return p.first.name() == indexInput_;
    }
    const std::string indexInput_;
};
} // namespace

std::size_t BlackScholesCG::getFutureBarrierProb(const std::string& index, const Date& obsdate1, const Date& obsdate2,
                                                 const std::size_t barrier, const bool above) const {

    // get the underlying values at the start and end points of the period

    std::size_t v1 = eval(index, obsdate1, Null<Date>());
    std::size_t v2 = eval(index, obsdate2, Null<Date>());

    // check the barrier at the two endpoints

    std::size_t barrierHit = cg_const(*g_, 0.0);
    if (above) {
        barrierHit = cg_min(*g_, cg_const(*g_, 1.0), cg_add(*g_, barrierHit, cg_indicatorGeq(*g_, v1, barrier)));
        barrierHit = cg_min(*g_, cg_const(*g_, 1.0), cg_add(*g_, barrierHit, cg_indicatorGeq(*g_, v2, barrier)));
    } else {
        barrierHit =
            cg_min(*g_, cg_const(*g_, 1.0),
                   cg_add(*g_, barrierHit, cg_subtract(*g_, cg_const(*g_, 1.0), cg_indicatorGt(*g_, v1, barrier))));
        barrierHit =
            cg_min(*g_, cg_const(*g_, 1.0),
                   cg_add(*g_, barrierHit, cg_subtract(*g_, cg_const(*g_, 1.0), cg_indicatorGt(*g_, v2, barrier))));
    }

    // for IR / INF indices we have to check each date, this is fast though, since the index values are deteministic
    // here

    auto ir = std::find_if(irIndices_.begin(), irIndices_.end(), comp(index));
    auto inf = std::find_if(infIndices_.begin(), infIndices_.end(), comp(index));
    if (ir != irIndices_.end() || inf != infIndices_.end()) {
        Date d = obsdate1 + 1;
        while (d < obsdate2) {
            std::size_t res;
            if (ir != irIndices_.end())
                res = getIrIndexValue(std::distance(irIndices_.begin(), ir), d);
            else
                res = getInfIndexValue(std::distance(infIndices_.begin(), inf), d);

            if (above) {
                barrierHit =
                    cg_min(*g_, cg_const(*g_, 1.0), cg_add(*g_, barrierHit, cg_indicatorGeq(*g_, res, barrier)));
            } else {
                barrierHit = cg_min(
                    *g_, cg_const(*g_, 1.0),
                    cg_add(*g_, barrierHit, cg_subtract(*g_, cg_const(*g_, 1.0), cg_indicatorGt(*g_, res, barrier))));
            }
            ++d;
        }
    }

    // return the result if we have an IR / INF index

    if (ir != irIndices_.end() || inf != infIndices_.end())
        return barrierHit;

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

    std::size_t variance = cg_const(*g_, 0.0);

    for (Size i = 1; i < effectiveSimulationDates_.size(); ++i) {

        Date d1 = *std::next(effectiveSimulationDates_.begin(), i - 1);
        Date d2 = *std::next(effectiveSimulationDates_.begin(), i);

        if (obsdate1 <= d1 && d2 <= obsdate2) {
            if (ind1 != Null<Size>()) {
                auto c = modelParameters_.find(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::cov, {}, {}, d1, {}, {}, ind1, ind1));
                QL_REQUIRE(c != modelParameters_.end(), "BlackScholesCG::getFutureBarrierProb(): internal error, "
                                                        "covariance 1/1 not found in model parameters.");
                variance = cg_add(*g_, variance, c->node());
            }
            if (ind2 != Null<Size>()) {
                auto c = modelParameters_.find(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::cov, {}, {}, d1, {}, {}, ind2, ind2));
                QL_REQUIRE(c != modelParameters_.end(), "BlackScholesCG::getFutureBarrierProb(): internal error, "
                                                        "covariance 2/2 not found in model parameters.");
                variance = cg_add(*g_, variance, c->node());
            }
            if (ind1 != Null<Size>() && ind2 != Null<Size>()) {
                auto c = modelParameters_.find(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::cov, {}, {}, d1, {}, {}, ind1, ind2));
                QL_REQUIRE(c != modelParameters_.end(), "BlackScholesCG::getFutureBarrierProb(): internal error, "
                                                        "covariance 1/2 not found in model parameters.");
                variance = cg_subtract(*g_, variance, cg_mult(*g_, cg_const(*g_, 2.0), c->node()));
            }
        }
    }

    // compute the hit probability
    // see e.g. formulas 2, 4 in Emmanuel Gobet, Advanced Monte Carlo methods for barrier and related exotic options

    std::size_t eps = cg_const(*g_, 1E-14);

    variance = cg_max(*g_, variance, eps);
    std::size_t adjBarrier = cg_max(*g_, barrier, eps);

    std::size_t hitProb = cg_min(*g_, cg_const(*g_, 1.0),
                                 cg_exp(*g_, cg_mult(*g_,
                                                     cg_mult(*g_, cg_div(*g_, cg_const(*g_, -2.0), variance),
                                                             cg_log(*g_, cg_div(*g_, v1, adjBarrier))),
                                                     cg_log(*g_, cg_div(*g_, v2, adjBarrier)))));

    barrierHit = cg_add(*g_, barrierHit, cg_mult(*g_, cg_subtract(*g_, cg_const(*g_, 1.0), barrierHit), hitProb));

    return barrierHit;
} // getFutureBarrierProb()

std::size_t BlackScholesCG::getIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
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
    QL_REQUIRE(underlyingPaths_.find(d) != underlyingPaths_.end(), "did not find path for " << d);
    auto res = underlyingPaths_.at(d).at(indexNo);
    // compute forwarding factor
    if (effFwd != Null<Date>()) {
        auto p = model_->processes().at(indexNo);
        std::size_t div_d = addModelParameter(
            ModelCG::ModelParameter(ModelCG::ModelParameter::Type::div, {}, {}, d, {}, {}, {}, indexNo),
            [p, d]() { return p->dividendYield()->discount(d); });
        std::size_t div_f = addModelParameter(
            ModelCG::ModelParameter(ModelCG::ModelParameter::Type::div, {}, {}, effFwd, {}, {}, {}, indexNo),
            [p, effFwd]() { return p->dividendYield()->discount(effFwd); });
        std::size_t rfr_d = addModelParameter(
            ModelCG::ModelParameter(ModelCG::ModelParameter::Type::rfr, {}, {}, d, {}, {}, {}, indexNo),
            [p, d]() { return p->riskFreeRate()->discount(d); });
        std::size_t rfr_f = addModelParameter(
            ModelCG::ModelParameter(ModelCG::ModelParameter::Type::rfr, {}, {}, effFwd, {}, {}, {}, indexNo),
            [p, effFwd]() { return p->riskFreeRate()->discount(effFwd); });
        res = cg_mult(*g_, res, cg_mult(*g_, div_f, cg_div(*g_, rfr_d, cg_mult(*g_, div_d, rfr_f))));
    }
    return res;
}

std::size_t BlackScholesCG::getIrIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFixingDate = d;
    if (fwd != Null<Date>())
        effFixingDate = fwd;
    // ensure a valid fixing date
    effFixingDate = irIndices_.at(indexNo).second->fixingCalendar().adjust(effFixingDate);
    auto index = irIndices_.at(indexNo).second;
    return addModelParameter(
        ModelCG::ModelParameter(ModelCG::ModelParameter::Type::fix, index->name(), {}, effFixingDate),
        [index, effFixingDate]() { return index->fixing(effFixingDate); });
}

std::size_t BlackScholesCG::getInfIndexValue(const Size indexNo, const Date& d, const Date& fwd) const {
    Date effFixingDate = d;
    if (fwd != Null<Date>())
        effFixingDate = fwd;
    auto index = infIndices_.at(indexNo).second;
    return addModelParameter(
        ModelCG::ModelParameter(ModelCG::ModelParameter::Type::fix, index->name(), {}, effFixingDate),
        [index, effFixingDate]() { return index->fixing(effFixingDate); });
}

std::size_t BlackScholesCG::fwdCompAvg(const bool isAvg, const std::string& indexInput, const Date& obsdate,
                                       const Date& start, const Date& end, const Real spread, const Real gearing,
                                       const Integer lookback, const Natural rateCutoff, const Natural fixingDays,
                                       const bool includeSpread, const Real cap, const Real floor,
                                       const bool nakedOption, const bool localCapFloor) const {
    calculate();
    auto index = std::find_if(irIndices_.begin(), irIndices_.end(), comp(indexInput));
    QL_REQUIRE(index != irIndices_.end(),
               "BlackScholesCG::fwdCompAvg(): did not find ir index " << indexInput << " - this is unexpected.");
    auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index->second);
    QL_REQUIRE(on, "BlackScholesCG::fwdCompAvg(): expected on index for " << indexInput);
    // if we want to support cap / floors we need the OIS CF surface
    QL_REQUIRE(cap > 999998.0 && floor < -999998.0,
               "BlackScholesCG:fwdCompAvg(): cap (" << cap << ") / floor (" << floor << ") not supported");
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
    return addModelParameter(
        ModelCG::ModelParameter(ModelCG::ModelParameter::Type::fwdCompAvg, {}, {}, {}, {}, {}, {}, g_->size()),
        [coupon]() { return coupon->rate(); });
}

std::size_t BlackScholesCG::getDiscount(const Size idx, const Date& s, const Date& t) const {
    auto c = curves_.at(idx);
    std::size_t ns =
        addModelParameter(ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, currencies_[idx], {}, s),
                          [c, s] { return c->discount(s); });
    std::size_t nt =
        addModelParameter(ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, currencies_[idx], {}, t),
                          [c, t] { return c->discount(t); });
    return cg_div(*g_, nt, ns);
}

std::size_t BlackScholesCG::numeraire(const Date& s) const {
    auto c = curves_.at(0);
    std::size_t ds =
        addModelParameter(ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, currencies_[0], {}, s),
                          [c, s] { return c->discount(s); });
    return cg_div(*g_, cg_const(*g_, 1.0), ds);
}

std::size_t BlackScholesCG::getFxSpot(const Size idx) const {
    auto c = fxSpots_.at(idx);
    return cg_exp(
        *g_, addModelParameter(ModelCG::ModelParameter(ModelCG::ModelParameter::Type::logFxSpot, currencies_[idx + 1]),
                               [c] { return c->value(); }));
}

Real BlackScholesCG::getDirectFxSpotT0(const std::string& forCcy, const std::string& domCcy) const {
    auto c1 = std::find(currencies_.begin(), currencies_.end(), forCcy);
    auto c2 = std::find(currencies_.begin(), currencies_.end(), domCcy);
    QL_REQUIRE(c1 != currencies_.end(), "currency " << forCcy << " not handled");
    QL_REQUIRE(c2 != currencies_.end(), "currency " << domCcy << " not handled");
    Size cidx1 = std::distance(currencies_.begin(), c1);
    Size cidx2 = std::distance(currencies_.begin(), c2);
    Real fx = 1.0;
    if (cidx1 > 0)
        fx *= fxSpots_.at(cidx1 - 1)->value();
    if (cidx2 > 0)
        fx /= fxSpots_.at(cidx2 - 1)->value();
    return fx;
}

Real BlackScholesCG::getDirectDiscountT0(const Date& paydate, const std::string& currency) const {
    auto c = std::find(currencies_.begin(), currencies_.end(), currency);
    QL_REQUIRE(c != currencies_.end(), "currency " << currency << " not handled");
    Size cidx = std::distance(currencies_.begin(), c);
    return curves_.at(cidx)->discount(paydate);
}

std::set<std::size_t>
BlackScholesCG::npvRegressors(const Date& obsdate,
                              const std::optional<std::set<std::string>>& relevantCurrencies) const {

    std::set<std::size_t> state;

    if (obsdate == referenceDate()) {
        return state;
    }

    if (!underlyingPaths_.empty()) {
        for (Size i = 0; i < indices_.size(); ++i) {
            if (relevantCurrencies && indices_[i].isFx()) {
                if (relevantCurrencies->find(indices_[i].fx()->sourceCurrency().code()) == relevantCurrencies->end())
                    continue;
            }
            state.insert(underlyingPaths_.at(obsdate).at(i));
        }
    }

    return state;
}

std::size_t BlackScholesCG::npv(const std::size_t amount, const Date& obsdate, const std::size_t filter,
                                const std::optional<long>& memSlot, const std::set<std::size_t> addRegressors,
                                const std::optional<std::set<std::size_t>>& overwriteRegressors) const {

    calculate();

    QL_REQUIRE(!memSlot, "BlackScholesCG::npv() with memSlot not yet supported!");

    // if obsdate is today, take a plain expectation

    if (obsdate == referenceDate()) {
        return cg_conditionalExpectation(*g_, amount, {}, cg_const(*g_, 1.0));
    }

    // build the state

    std::vector<std::size_t> state;

    if (overwriteRegressors) {
        state.insert(state.end(), overwriteRegressors->begin(), overwriteRegressors->end());
    } else {
        std::set<std::size_t> r = npvRegressors(obsdate, std::nullopt);
        state.insert(state.end(), r.begin(), r.end());
    }

    for (auto const& r : addRegressors)
        if (r != ComputationGraph::nan)
            state.push_back(r);

    // if the state is empty, return the plain expectation (no conditioning)

    if (state.empty()) {
        return cg_conditionalExpectation(*g_, amount, {}, cg_const(*g_, 1.0));
    }

    // if a memSlot is given and coefficients are stored, we use them
    // TODO ...

    // otherwise compute coefficients and store them if a memSlot is given
    // TODO ...

    // compute conditional expectation and return the result

    return cg_conditionalExpectation(*g_, amount, state, filter);
}

} // namespace data
} // namespace ore
