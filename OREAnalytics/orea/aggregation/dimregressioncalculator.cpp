/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/aggregation/dimregressioncalculator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/vectorutils.hpp>
#include <ql/errors.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/generallinearleastsquares.hpp>
#include <ql/math/kernelfunctions.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>
#include <ql/time/daycounters/actualactual.hpp>

#include <qle/math/nadarayawatson.hpp>
#include <qle/math/stabilisedglls.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

using namespace std;
using namespace QuantLib;

using namespace boost::accumulators;

namespace ore {
namespace analytics {

RegressionDynamicInitialMarginCalculator::RegressionDynamicInitialMarginCalculator(
    const QuantLib::ext::shared_ptr<InputParameters>& inputs,
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<NPVCube>& cube,
    const QuantLib::ext::shared_ptr<CubeInterpretation>& cubeInterpretation,
    const QuantLib::ext::shared_ptr<AggregationScenarioData>& scenarioData, Real quantile, Size horizonCalendarDays,
    Size regressionOrder, std::vector<std::string> regressors, Size localRegressionEvaluations,
    Real localRegressionBandWidth, const std::map<std::string, Real>& currentIM)
: DynamicInitialMarginCalculator(inputs, portfolio, cube, cubeInterpretation, scenarioData, quantile, horizonCalendarDays,
                                 currentIM),
      regressionOrder_(regressionOrder), regressors_(regressors),
      localRegressionEvaluations_(localRegressionEvaluations), localRegressionBandWidth_(localRegressionBandWidth) {
    Size dates = cube_->dates().size();
    Size samples = cube_->samples();
    for (const auto& nettingSetId : nettingSetIds_) {
        regressorArray_[nettingSetId] = vector<vector<Array>>(dates, vector<Array>(samples));
        nettingSetLocalDIM_[nettingSetId] = vector<vector<Real>>(dates, vector<Real>(samples, 0.0));
        nettingSetZeroOrderDIM_[nettingSetId] = vector<Real>(dates, 0.0);
        nettingSetSimpleDIMh_[nettingSetId] = vector<Real>(dates, 0.0);
        nettingSetSimpleDIMp_[nettingSetId] = vector<Real>(dates, 0.0);
    }
}

const vector<vector<Real>>&
RegressionDynamicInitialMarginCalculator::localRegressionResults(const std::string& nettingSet) {
    if (nettingSetLocalDIM_.find(nettingSet) != nettingSetLocalDIM_.end())
        return nettingSetLocalDIM_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in Local DIM results");
}

const vector<Real>& RegressionDynamicInitialMarginCalculator::zeroOrderResults(const std::string& nettingSet) {
    if (nettingSetZeroOrderDIM_.find(nettingSet) != nettingSetZeroOrderDIM_.end())
        return nettingSetZeroOrderDIM_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in Zero Order DIM results");
}

const vector<Real>& RegressionDynamicInitialMarginCalculator::simpleResultsUpper(const std::string& nettingSet) {
    if (nettingSetSimpleDIMp_.find(nettingSet) != nettingSetSimpleDIMp_.end())
        return nettingSetSimpleDIMp_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in Simple DIM (p) results");
}

const vector<Real>& RegressionDynamicInitialMarginCalculator::simpleResultsLower(const std::string& nettingSet) {
    if (nettingSetSimpleDIMh_.find(nettingSet) != nettingSetSimpleDIMh_.end())
        return nettingSetSimpleDIMh_[nettingSet];
    else
        QL_FAIL("netting set " << nettingSet << " not found in Simple DIM (c) results");
}

void RegressionDynamicInitialMarginCalculator::build() {
    LOG("DIM Analysis by polynomial regression");

    map<string, Real> currentDim = unscaledCurrentDIM();

    Size stopDatesLoop = datesLoopSize_;
    Size samples = cube_->samples();

    Size polynomOrder = regressionOrder_;
    LOG("DIM regression polynom order = " << regressionOrder_);
    LsmBasisSystem::PolynomialType polynomType = LsmBasisSystem::Monomial;
    Size regressionDimension = regressors_.empty() ? 1 : regressors_.size();
    LOG("DIM regression dimension = " << regressionDimension);
    std::vector<ext::function<Real(Array)>> v(
        LsmBasisSystem::multiPathBasisSystem(regressionDimension, polynomOrder, polynomType));
    Real confidenceLevel = QuantLib::InverseCumulativeNormal()(quantile_);
    LOG("DIM confidence level " << confidenceLevel);

    Size simple_dim_index_h = Size(floor(quantile_ * (samples - 1) + 0.5));
    Size simple_dim_index_p = Size(floor((1.0 - quantile_) * (samples - 1) + 0.5));

    Size nettingSetCount = 0;
    for (auto n : nettingSetIds_) {
        LOG("Process netting set " << n);

        if (inputs_) {
            // Check whether a deterministic IM evolution was provided for this netting set
            // If found then we use this external data to overwrite the following
            // - nettingSetExpectedDIM_  by nettingSet and date
            // - nettingSetZeroOrderDIM_ by nettingSet and date
            // - nettingSetSimpleDIMh_   by nettingSet and date
            // - nettingSetSimpleDIMp_   by nettingSet and date
            // - nettingSetDIM_          by nettingSet, date and sample
            // - dimCube_                by nettingSet, date and sample
            TimeSeries<Real> im = inputs_->deterministicInitialMargin(n);
            LOG("External IM evolution for netting set " << n << " has size " << im.size());
            if (im.size() > 0) {
                WLOG("Try overriding DIM with externally provided IM evolution for netting set " << n);
                for (Size j = 0; j < stopDatesLoop; ++j) {
                    Date d = cube_->dates()[j];
                    try {
                        Real value = im[d];
                        nettingSetExpectedDIM_[n][j] = value;
                        nettingSetZeroOrderDIM_[n][j] = value;
                        nettingSetSimpleDIMh_[n][j] = value;
                        nettingSetSimpleDIMp_[n][j] = value;
                        for (Size k = 0; k < samples; ++k) {
                            dimCube_->set(value, nettingSetCount, j, k);
                            nettingSetDIM_[n][j][k] = value;
                        }
                    } catch(std::exception& ) {
                        QL_FAIL("Failed to lookup external IM for netting set " << n
                                << " at date " << io::iso_date(d));
                    }
                }
                WLOG("Overriding DIM for netting set " << n << " succeeded");
                // continue to the next netting set
                continue;
            }
        }
        
        if (currentIM_.find(n) != currentIM_.end()) {
            Real t0im = currentIM_[n];
            QL_REQUIRE(currentDim.find(n) != currentDim.end(), "current DIM not found for netting set " << n);
            Real t0dim = currentDim[n];
            Real t0scaling = t0im / t0dim;
            LOG("t0 scaling for netting set " << n << ": t0im" << t0im << " t0dim=" << t0dim
                                              << " t0scaling=" << t0scaling);
            nettingSetScaling_[n] = t0scaling;
        }

        Real nettingSetDimScaling =
            nettingSetScaling_.find(n) == nettingSetScaling_.end() ? 1.0 : nettingSetScaling_[n];
        LOG("Netting set DIM scaling factor: " << nettingSetDimScaling);

        for (Size j = 0; j < stopDatesLoop; ++j) {
            accumulator_set<double, stats<boost::accumulators::tag::mean, boost::accumulators::tag::variance>> accDiff;
            accumulator_set<double, stats<boost::accumulators::tag::mean>> accOneOverNumeraire;
            for (Size k = 0; k < samples; ++k) {
                Real numDefault =
                    cubeInterpretation_->getDefaultAggregationScenarioData(AggregationScenarioDataType::Numeraire, j, k);
                Real numCloseOut =
                    cubeInterpretation_->getCloseOutAggregationScenarioData(AggregationScenarioDataType::Numeraire, j, k);
                Real npvDefault = nettingSetNPV_[n][j][k];
                Real flow = nettingSetFLOW_[n][j][k];
                Real npvCloseOut = nettingSetCloseOutNPV_[n][j][k];
                accDiff((npvCloseOut * numCloseOut) + (flow * numDefault) - (npvDefault * numDefault));
                accOneOverNumeraire(1.0 / numDefault);
            }

            Size mporCalendarDays = cubeInterpretation_->getMporCalendarDays(cube_, j);
            Real horizonScaling = sqrt(1.0 * horizonCalendarDays_ / mporCalendarDays);

            Real stdevDiff = sqrt(boost::accumulators::variance(accDiff));
            Real E_OneOverNumeraire =
                mean(accOneOverNumeraire); // "re-discount" (the stdev is calculated on non-discounted deltaNPVs)

            nettingSetZeroOrderDIM_[n][j] = stdevDiff * horizonScaling * confidenceLevel;
            nettingSetZeroOrderDIM_[n][j] *= E_OneOverNumeraire;

            vector<Real> rx0(samples, 0.0);
            vector<Array> rx(samples, Array());
            vector<Real> ry1(samples, 0.0);
            vector<Real> ry2(samples, 0.0);
            for (Size k = 0; k < samples; ++k) {
                Real numDefault =
                    cubeInterpretation_->getDefaultAggregationScenarioData(AggregationScenarioDataType::Numeraire, j, k);
                Real numCloseOut =
                    cubeInterpretation_->getCloseOutAggregationScenarioData(AggregationScenarioDataType::Numeraire, j, k);
                Real x = nettingSetNPV_[n][j][k] * numDefault;
                Real f = nettingSetFLOW_[n][j][k] * numDefault;
                Real y = nettingSetCloseOutNPV_[n][j][k] * numCloseOut;
                Real z = (y + f - x);
                rx[k] = regressors_.empty() ? Array(1, nettingSetNPV_[n][j][k]) : regressorArray(n, j, k);
                rx0[k] = rx[k][0];
                ry1[k] = z;     // for local regression
                ry2[k] = z * z; // for least squares regression
                nettingSetDeltaNPV_[n][j][k] = z;
                regressorArray_[n][j][k] = rx[k];
            }
            vector<Real> delNpvVec_copy = nettingSetDeltaNPV_[n][j];
            sort(delNpvVec_copy.begin(), delNpvVec_copy.end());
            Real simpleDim_h = delNpvVec_copy[simple_dim_index_h];
            Real simpleDim_p = delNpvVec_copy[simple_dim_index_p];
            simpleDim_h *= horizonScaling;                                  // the usual scaling factors
            simpleDim_p *= horizonScaling;                                  // the usual scaling factors
            nettingSetSimpleDIMh_[n][j] = simpleDim_h * E_OneOverNumeraire; // discounted DIM
            nettingSetSimpleDIMp_[n][j] = simpleDim_p * E_OneOverNumeraire; // discounted DIM

            QL_REQUIRE(rx.size() > v.size(), "not enough points for regression with polynom order " << polynomOrder);
            if (close_enough(stdevDiff, 0.0)) {
                LOG("DIM: Zero std dev estimation at step " << j);
                // Skip IM calculation if all samples have zero NPV (e.g. after latest maturity)
                for (Size k = 0; k < samples; ++k) {
                    nettingSetDIM_[n][j][k] = 0.0;
                    nettingSetLocalDIM_[n][j][k] = 0.0;
                }
            } else {
                // Least squares polynomial regression with specified polynom order
                QuantExt::StabilisedGLLS ls(rx, ry2, v, QuantExt::StabilisedGLLS::MeanStdDev);
                LOG("DIM data normalisation at time step "
                    << j << ": " << scientific << setprecision(6) << " x-shift = " << ls.xShift() << " x-multiplier = "
                    << ls.xMultiplier() << " y-shift = " << ls.yShift() << " y-multiplier = " << ls.yMultiplier());
                LOG("DIM regression coefficients at time step " << j << ": " << fixed << setprecision(6)
                                                                << ls.transformedCoefficients());

                // Local regression versus first regression variable (i.e. we do not perform a
                // multidimensional local regression):
                // We evaluate this at a limited number of samples only for validation purposes.
                // Note that computational effort scales quadratically with number of samples.
                // NadarayaWatson needs a large number of samples for good results.
                QuantExt::NadarayaWatson lr(rx0.begin(), rx0.end(), ry1.begin(),
                                            GaussianKernel(0.0, localRegressionBandWidth_));
                Size localRegressionSamples = samples;
                if (localRegressionEvaluations_ > 0)
                    localRegressionSamples = Size(floor(1.0 * samples / localRegressionEvaluations_ + .5));

                // Evaluate regression function to compute DIM for each scenario
                for (Size k = 0; k < samples; ++k) {
                    // Real num1 = scenarioData_->get(j, k, AggregationScenarioDataType::Numeraire);
                    Real numDefault = cubeInterpretation_->getDefaultAggregationScenarioData(
                        AggregationScenarioDataType::Numeraire, j, k);
                    Array regressor = regressors_.empty() ? Array(1, nettingSetNPV_[n][j][k]) : regressorArray(n, j, k);
                    Real e = ls.eval(regressor, v);
                    if (e < 0.0)
                        LOG("Negative variance regression for date " << j << ", sample " << k
                                                                     << ", regressor = " << regressor);

                    // Note:
                    // 1) We assume vanishing mean of "z", because the drift over a MPOR is usually small,
                    //    and to avoid a second regression for the conditional mean
                    // 2) In particular the linear regression function can yield negative variance values in
                    //    extreme scenarios where an exact analytical or delta VaR calculation would yield a
                    //    variance approaching zero. We correct this here by taking the positive part.
                    Real std = sqrt(std::max(e, 0.0));
                    Real scalingFactor = horizonScaling * confidenceLevel * nettingSetDimScaling;
                    // Real dim = std * scalingFactor / num1;
                    Real dim = std * scalingFactor / numDefault;
                    dimCube_->set(dim, nettingSetCount, j, k);
                    nettingSetDIM_[n][j][k] = dim;
                    nettingSetExpectedDIM_[n][j] += dim / samples;

                    // Evaluate the Kernel regression for a subset of the samples only (performance)
                    if (localRegressionEvaluations_ > 0 && (k % localRegressionSamples == 0))
                        // nettingSetLocalDIM_[n][j][k] = lr.standardDeviation(regressor[0]) * scalingFactor / num1;
                        nettingSetLocalDIM_[n][j][k] = lr.standardDeviation(regressor[0]) * scalingFactor / numDefault;
                    else
                        nettingSetLocalDIM_[n][j][k] = 0.0;
                }
            }
        }

        nettingSetCount++;
    }
    LOG("DIM by polynomial regression done");
}

Array RegressionDynamicInitialMarginCalculator::regressorArray(string nettingSet, Size dateIndex,
                                                                           Size sampleIndex) {
    Array a(regressors_.size());
    for (Size i = 0; i < regressors_.size(); ++i) {
        string variable = regressors_[i];
        if (boost::to_upper_copy(variable) ==
            "NPV") // this allows possibility to include NPV as a regressor alongside more fundamental risk factors
            a[i] = nettingSetNPV_[nettingSet][dateIndex][sampleIndex];
        else if (scenarioData_->has(AggregationScenarioDataType::IndexFixing, variable))
            a[i] = cubeInterpretation_->getDefaultAggregationScenarioData(AggregationScenarioDataType::IndexFixing,
                                                                      dateIndex, sampleIndex, variable);
        else if (scenarioData_->has(AggregationScenarioDataType::FXSpot, variable))
            a[i] = cubeInterpretation_->getDefaultAggregationScenarioData(AggregationScenarioDataType::FXSpot, dateIndex,
                                                                      sampleIndex, variable);
        else if (scenarioData_->has(AggregationScenarioDataType::Generic, variable))
            a[i] = cubeInterpretation_->getDefaultAggregationScenarioData(AggregationScenarioDataType::Generic, dateIndex,
                                                                      sampleIndex, variable);
        else
            QL_FAIL("scenario data does not provide data for " << variable);
    }
    return a;
}

map<string, Real> RegressionDynamicInitialMarginCalculator::unscaledCurrentDIM() {
    // In this function we proxy the model-implied T0 IM by looking at the
    // cube grid horizon lying closest to t0+mpor. We measure diffs relative
    // to the mean of the distribution at this same time horizon, thus avoiding
    // any cashflow-specific jumps

    Date today = cube_->asof();
    Size relevantDateIdx = 0;
    Real sqrtTimeScaling = 1.0;
    for (Size i = 0; i < cube_->dates().size(); ++i) {
        Size daysFromT0 = (cube_->dates()[i] - today);
        if (daysFromT0 < horizonCalendarDays_) {
            // iterate until we straddle t0+mpor
            continue;
        } else if (daysFromT0 == horizonCalendarDays_) {
            // this date corresponds to t0+mpor, so use it
            relevantDateIdx = i;
            sqrtTimeScaling = 1.0;
            break;
        } else if (daysFromT0 > horizonCalendarDays_) {
            // the first date greater than t0+MPOR, check if it is closest
            Size lastIdx = (i == 0) ? 0 : (i - 1);
            Size lastDaysFromT0 = (cube_->dates()[lastIdx] - today);
            int daysFromT0CloseOut = static_cast<int>(daysFromT0 - horizonCalendarDays_);
            int prevDaysFromT0CloseOut = static_cast<int>(lastDaysFromT0 - horizonCalendarDays_);
            if (std::abs(daysFromT0CloseOut) <= std::abs(prevDaysFromT0CloseOut)) {
                relevantDateIdx = i;
                sqrtTimeScaling = std::sqrt(Real(horizonCalendarDays_) / Real(daysFromT0));
            } else {
                relevantDateIdx = lastIdx;
                sqrtTimeScaling = std::sqrt(Real(horizonCalendarDays_) / Real(lastDaysFromT0));
            }
            break;
        }
    }
    // set some reasonable bounds on the sqrt time scaling, so that we are not looking at a ridiculous time horizon
    if (sqrtTimeScaling < std::sqrt(0.5) || sqrtTimeScaling > std::sqrt(2.0)) {
        WLOG("T0 IM Estimation - The estimation time horizon from grid is not sufficiently close to t0+MPOR - "
             << QuantLib::io::iso_date(cube_->dates()[relevantDateIdx])
             << ", the T0 IM estimate might be inaccurate. Consider inserting a first grid tenor closer to the dim "
                "horizon");
    }

    // TODO: Ensure that the simulation containers read-from below are indeed populated

    Real confidenceLevel = QuantLib::InverseCumulativeNormal()(quantile_);
    Size simple_dim_index_h = Size(floor(quantile_ * (cube_->samples() - 1) + 0.5));
    map<string, Real> t0dimReg, t0dimSimple;
    for (auto it_map = nettingSetNPV_.begin(); it_map != nettingSetNPV_.end(); ++it_map) {
        string key = it_map->first;
        vector<Real> t0_dist = it_map->second[relevantDateIdx];
        Size dist_size = t0_dist.size();
        QL_REQUIRE(dist_size == cube_->samples(),
                   "T0 IM - cube samples size mismatch - " << dist_size << ", " << cube_->samples());
        Real mean_t0_dist = std::accumulate(t0_dist.begin(), t0_dist.end(), 0.0);
        mean_t0_dist /= dist_size;
        vector<Real> t0_delMtM_dist(dist_size, 0.0);
        accumulator_set<double, stats<boost::accumulators::tag::mean, boost::accumulators::tag::variance>> acc_delMtm;
        accumulator_set<double, stats<boost::accumulators::tag::mean>> acc_OneOverNum;
        for (Size i = 0; i < dist_size; ++i) {
            Real numeraire = scenarioData_->get(relevantDateIdx, i, AggregationScenarioDataType::Numeraire);
            Real deltaMtmFromMean = numeraire * (t0_dist[i] - mean_t0_dist) * sqrtTimeScaling;
            t0_delMtM_dist[i] = deltaMtmFromMean;
            acc_delMtm(deltaMtmFromMean);
            acc_OneOverNum(1.0 / numeraire);
        }
        Real E_OneOverNumeraire = mean(acc_OneOverNum);
        Real variance_t0 = boost::accumulators::variance(acc_delMtm);
        Real sqrt_t0 = sqrt(variance_t0);
        t0dimReg[key] = (sqrt_t0 * confidenceLevel * E_OneOverNumeraire);
        std::sort(t0_delMtM_dist.begin(), t0_delMtM_dist.end());
        t0dimSimple[key] = (t0_delMtM_dist[simple_dim_index_h] * E_OneOverNumeraire);

        LOG("T0 IM (Reg) - {" << key << "} = " << t0dimReg[key]);
        LOG("T0 IM (Simple) - {" << key << "} = " << t0dimSimple[key]);
    }
    LOG("T0 IM Calculations Completed");

    return t0dimReg;
}

void RegressionDynamicInitialMarginCalculator::exportDimEvolution(ore::data::Report& dimEvolutionReport) const {

    Size samples = dimCube_->samples();
    Size stopDatesLoop = datesLoopSize_;
    Date asof = cube_->asof();

    dimEvolutionReport.addColumn("TimeStep", Size())
        .addColumn("Date", Date())
        .addColumn("DaysInPeriod", Size())
        .addColumn("ZeroOrderDIM", Real(), 6)
        .addColumn("AverageDIM", Real(), 6)
        .addColumn("AverageFLOW", Real(), 6)
        .addColumn("SimpleDIM", Real(), 6)
        .addColumn("NettingSet", string())
        .addColumn("Time", Real(), 6);

    for (const auto& [nettingSet, _] : dimCube_->idsAndIndexes()) {

        LOG("Export DIM evolution for netting set " << nettingSet);
        for (Size i = 0; i < stopDatesLoop; ++i) {
            Real expectedFlow = 0.0;
            for (Size j = 0; j < samples; ++j) {
                expectedFlow += nettingSetFLOW_.find(nettingSet)->second[i][j] / samples;
            }

            Date defaultDate = dimCube_->dates()[i];
            Time t = ActualActual(ActualActual::ISDA).yearFraction(asof, defaultDate);
            Size days = cubeInterpretation_->getMporCalendarDays(dimCube_, i);
            dimEvolutionReport.next()
                .add(i)
                .add(defaultDate)
                .add(days)
                .add(nettingSetZeroOrderDIM_.find(nettingSet)->second[i])
                .add(nettingSetExpectedDIM_.find(nettingSet)->second[i])
                .add(expectedFlow)
                .add(nettingSetSimpleDIMh_.find(nettingSet)->second[i])
                .add(nettingSet)
                .add(t);
        }
    }
    dimEvolutionReport.end();
    LOG("Exporting expected DIM through time done");
}

void RegressionDynamicInitialMarginCalculator::exportDimRegression(
    const std::string& nettingSet, const std::vector<Size>& timeSteps,
    const std::vector<QuantLib::ext::shared_ptr<ore::data::Report>>& dimRegReports) {

    QL_REQUIRE(dimRegReports.size() == timeSteps.size(),
               "number of file names (" << dimRegReports.size() << ") does not match number of time steps ("
                                        << timeSteps.size() << ")");
    for (Size ii = 0; ii < timeSteps.size(); ++ii) {
        Size timeStep = timeSteps[ii];
        LOG("Export DIM by sample for netting set " << nettingSet << " and time step " << timeStep);

        Size dates = dimCube_->dates().size();
        const std::map<std::string, Size>& ids = dimCube_->idsAndIndexes();

        auto it = ids.find(nettingSet);
                
        QL_REQUIRE(it != ids.end(), "netting set " << nettingSet << " not found in DIM cube");

        QL_REQUIRE(timeStep < dates - 1, "selected time step " << timeStep << " out of range [0, " << dates - 1 << "]");

        Size samples = cube_->samples();
        vector<Real> numeraires(samples, 0.0);
        for (Size k = 0; k < samples; ++k)
            // numeraires[k] = scenarioData_->get(timeStep, k, AggregationScenarioDataType::Numeraire);
            numeraires[k] =
                cubeInterpretation_->getDefaultAggregationScenarioData(AggregationScenarioDataType::Numeraire, timeStep, k);

        auto p = sort_permutation(regressorArray_[nettingSet][timeStep], lessThan);
        vector<Array> reg = apply_permutation(regressorArray_[nettingSet][timeStep], p);
        vector<Real> dim = apply_permutation(nettingSetDIM_[nettingSet][timeStep], p);
        vector<Real> ldim = apply_permutation(nettingSetLocalDIM_[nettingSet][timeStep], p);
        vector<Real> delta = apply_permutation(nettingSetDeltaNPV_[nettingSet][timeStep], p);
        vector<Real> num = apply_permutation(numeraires, p);

        QuantLib::ext::shared_ptr<ore::data::Report> regReport = dimRegReports[ii];
        regReport->addColumn("Sample", Size());
        for (Size k = 0; k < reg[0].size(); ++k) {
            ostringstream o;
            o << "Regressor_" << k << "_";
            o << (regressors_.empty() ? "NPV" : regressors_[k]);
            regReport->addColumn(o.str(), Real(), 6);
        }
        regReport->addColumn("RegressionDIM", Real(), 6)
            .addColumn("LocalDIM", Real(), 6)
            .addColumn("ExpectedDIM", Real(), 6)
            .addColumn("ZeroOrderDIM", Real(), 6)
            .addColumn("DeltaNPV", Real(), 6)
            .addColumn("SimpleDIM", Real(), 6);

        // Note that RegressionDIM, LocalDIM, DeltaNPV are _not_ reduced by the numeraire in this output,
        // but ExpectedDIM, ZeroOrderDIM and SimpleDIM _are_ reduced by the numeraire.
        // This is so that the regression formula can be manually validated

        for (Size j = 0; j < reg.size(); ++j) {
            regReport->next().add(j);
            for (Size k = 0; k < reg[j].size(); ++k)
                regReport->add(reg[j][k]);
            regReport->add(dim[j] * num[j])
                .add(ldim[j] * num[j])
                .add(nettingSetExpectedDIM_[nettingSet][timeStep])
                .add(nettingSetZeroOrderDIM_[nettingSet][timeStep])
                .add(delta[j])
                .add(nettingSetSimpleDIMh_[nettingSet][timeStep]);
        }
        regReport->end();
        LOG("Exporting DIM by Sample done for");
    }
}

} // namespace analytics
} // namespace ore
