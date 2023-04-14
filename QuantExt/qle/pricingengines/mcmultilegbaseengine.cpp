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

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/pricingengines/mcmultilegbaseengine.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/math/generallinearleastsquares.hpp>
#include <ql/math/matrixutilities/qrdecomposition.hpp>

namespace QuantExt {

namespace {

template <class I>
#if QL_HEX_VERSION > 0x01150000
Array regression(const std::vector<Array>& X, const I& Y, const std::vector<ext::function<Real(Array)>>& basisFn) {
#else
Array regression(const std::vector<Array>& X, const I& Y, const std::vector<boost::function1<Real, Array>>& basisFn) {
#endif
    QL_REQUIRE(X.size() == Y.size(), "McMultiLegBaseEngine: vector lenghts do not match");
    return GeneralLinearLeastSquares(X, Y, basisFn).coefficients();
}

#if QL_HEX_VERSION > 0x01150000
Real evalRegression(const Array& c, const Array& x, const std::vector<ext::function<Real(Array)>> basisFns) {
#else
Real evalRegression(const Array& c, const Array& x, const std::vector<boost::function1<Real, Array>> basisFns) {
#endif
    QL_REQUIRE(basisFns.size() == c.size(), "McMultiLegBaseEngine: coefficients size ("
                                                << c.size() << ") and number of basis functions (" << basisFns.size()
                                                << ") do not match");
    Real result = 0.0;
    for (Size j = 0; j < basisFns.size(); ++j) {
        result += c[j] * basisFns[j](x);
    }
    return result;
}

} // namespace

McMultiLegBaseEngine::McMultiLegBaseEngine(
    const Handle<CrossAssetModel>& model, const SequenceType calibrationPathGenerator,
    const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
    const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
    const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
    SobolRsg::DirectionIntegers directionIntegers, const std::vector<Handle<YieldTermStructure>>& discountCurves,
    const std::vector<Date>& simulationDates, const std::vector<Size>& externalModelIndices, const bool minimalObsDate,
    const bool regressionOnExerciseOnly)
    : model_(model), calibrationPathGenerator_(calibrationPathGenerator), pricingPathGenerator_(pricingPathGenerator),
      calibrationSamples_(calibrationSamples), pricingSamples_(pricingSamples), calibrationSeed_(calibrationSeed),
      pricingSeed_(pricingSeed), discountCurves_(discountCurves), simulationDates_(simulationDates),
      externalModelIndices_(externalModelIndices),
      basisFns_(LsmBasisSystem::multiPathBasisSystem(model->dimension(), polynomOrder, polynomType)),
      ordering_(ordering), directionIntegers_(directionIntegers), minimalObsDate_(minimalObsDate),
      regressionOnExerciseOnly_(regressionOnExerciseOnly) {

    QL_REQUIRE(calibrationSamples >= basisFns_.size(), "McMultiLegBaseEngine: too few calibrationSamples ("
                                                           << calibrationSamples
                                                           << ") given, must at least be the number of basis fns ("
                                                           << basisFns_.size() << ")");
    if (discountCurves_.size() == 0)
        discountCurves_.resize(model_->components(CrossAssetModel::AssetType::IR));
    else {
        QL_REQUIRE(discountCurves_.size() == model_->components(CrossAssetModel::AssetType::IR),
                   "McMultiLegBaseEngine: " << discountCurves_.size() << " discount curves given, but model has "
                                            << model_->components(CrossAssetModel::AssetType::IR) << " IR components.");
    }
}

void McMultiLegBaseEngine::computePath(const MultiPath& p) const {

    // reset result vectors
    std::fill(pathUndEx_.begin(), pathUndEx_.end(), 0.0);
    std::fill(pathUndDirty_.begin(), pathUndDirty_.end(), 0.0);
    std::fill(pathUndTrapped_.begin(), pathUndTrapped_.end(), 0.0);
    // loop over the path
    for (Size i = 0; i < p.pathSize() - 1; ++i) {
        // obsevation path index (default value = current path index)
        Size pathIndex = i + 1;
        // loop over events associated to the path time
        for (Size j = 0; j < modelIndex_[i].size(); ++j) {
            // determine the observation time
            if (observationTimeIndex_[i][j] != Null<Size>())
                pathIndex = observationTimeIndex_[i][j];
            Real t = p[0].timeGrid()[pathIndex];
            if (indexFwdCurve_[i][j] != nullptr)
                indexFwdCurve_[i][j]->move(pathDates_[pathIndex], p[indexCcyIndex_[i][j]][pathIndex]);
            if (indexDscCurve_[i][j] != nullptr)
                indexDscCurve_[i][j]->move(pathDates_[pathIndex], p[indexCcyIndex_[i][j]][pathIndex]);
            // compute payoff and add to underlying pvs
            Real fixing = modelIndex_[i][j] != nullptr ? modelIndex_[i][j]->fixing(fixingDate_[i][j]) : 0.0;
            Real underlying_rate = gearing_[i][j] * fixing + spread_[i][j];
            Real rate = std::max(underlying_rate, flooredRate_[i][j]);
            if (isNakedOption_[i][j])
                rate -= std::min(underlying_rate, cappedRate_[i][j]);
            else
                rate = std::min(rate, cappedRate_[i][j]);
            Real amount = rate * accrualTime_[i][j] * nominal_[i][j];

            if (couponIndex_[i][j])
                amount *= couponIndexQuantity_[i][j] * couponIndex_[i][j]->fixing(couponIndexFixingDate_[i][j]);

            Size payCcyNum = payCcyNum_[i][j];
            Real dsc = model_->discountBond(payCcyNum, t, payTime_[i][j], p[payCcyIndex_[i][j]][pathIndex],
                                            discountCurves_[payCcyNum]);
            Real fx = payCcyNum > 0 ? std::exp(p[payFxIndex_[i][j]][pathIndex]) : 1.0;
            Real num = model_->numeraire(0, t, p[0][pathIndex], discountCurves_[0]);
            Real npv = fx * dsc * amount / num;
            for (Size k = 0; k <= maxExerciseIndex_[i][j]; ++k) {
                pathUndEx_[k] += npv;
            }
            for (Size k = 0; k <= maxDirtyNpvIndex_[i][j]; ++k) {
                pathUndDirty_[k] += npv;
            }
            for (Size l = 0; l < trappedCoupons_[i][j].size(); ++l) {
                pathUndTrapped_[l] += npv;
            }
        }
    }
}

void McMultiLegBaseEngine::rollback(const bool calibration) const {

    Size N = calibration ? calibrationSamples_ : pricingSamples_; // number of paths
    Size numIdx = indexes_.size();                                // number of exercise / sim times

    std::vector<Real> option(N, 0.0); // option value
    std::vector<Array> x;             // regressor for option
    std::vector<Real> y;              // dependend variable for option
    std::vector<Real> exerciseValue;  // exercise value
    std::vector<Size> itmPaths;       // itm path indexes

    x.reserve(N);
    y.reserve(N);
    itmPaths.reserve(N);
    exerciseValue.reserve(N);

    // init result vectors
    if (calibration) {
        pathUndEx_.resize(numEx_ + 1);
        pathUndDirty_.resize(numSim_ + 1);
        pathUndTrapped_.resize(numSim_); // no index 0 for whole underlying
        coeffsItm_.resize(numEx_);
        coeffsUndEx_.resize(numEx_);
        coeffsFull_.resize(numSim_);
        coeffsUndTrapped_.resize(numSim_);
        coeffsUndDirty_.resize(numSim_);
    }

    // path generation and underlying npv computation
    std::vector<std::vector<Array>> paths(numIdx, std::vector<Array>(N));
    std::vector<Array> underlyingValueEx(numEx_ + 1, Array(N)), underlyingValueDirty(numSim_ + 1, Array(N)),
        underlyingValueTrapped(numSim_, Array());
    for (Size k = 0; k < numSim_; ++k) {
        if (isTrappedDate_[k])
            underlyingValueTrapped[k] = Array(N);
    }
    for (Size i = 0; i < N; ++i) {
        Sample<MultiPath> sample = calibration ? pathGeneratorCalibration_->next() : pathGeneratorPricing_->next();
        computePath(sample.value);
        for (Size k = 0; k <= numEx_; ++k)
            underlyingValueEx[k][i] = pathUndEx_[k];
        for (Size k = 0; k <= numSim_; ++k) {
            underlyingValueDirty[k][i] = pathUndDirty_[k];
            if (k > 0 && isTrappedDate_[k - 1])
                underlyingValueTrapped[k - 1][i] = pathUndDirty_[k] - pathUndTrapped_[k - 1];
        }
        for (Size j = 0; j < numIdx; ++j) {
            Array v(sample.value.assetNumber());
            for (Size k = 0; k < sample.value.assetNumber(); ++k) {
                v[k] = sample.value[k][indexes_[j] + 1];
            }
            paths[j][i].swap(v);
        }
    }

    // roll back over live ex / sim dates (1,2...) and today (0)
    bool isLastExercise = true;
    for (Size ts = numIdx; ts > 0; --ts) {
        // event type and indexes
        Size exIdx = exerciseIdx_[ts - 1];
        Size simIdx = simulationIdx_[ts - 1];
        bool isExercise = exIdx != Null<Size>();
        bool isSimulation = simIdx != Null<Size>();
        // conditional expectation of underlying value
        if (calibration) {
            if (isExercise) {
                coeffsUndEx_[exIdx] = regression(paths[ts - 1], underlyingValueEx[exIdx + 1], basisFns_);
            }
            if (isSimulation) {
                Size tmpTs = ts;
                while (regressionOnExerciseOnly_ && tmpTs <= numIdx && exerciseIdx_[tmpTs - 1] == Null<Size>()) {
                    ++tmpTs;
                }
                if (tmpTs > numIdx)
                    tmpTs = ts;
                // skip if we know that all underlying flows are zero
                if (simIdx <= maxUndValDirtyIdx_) {
                    coeffsUndDirty_[simIdx] = regression(paths[tmpTs - 1], underlyingValueDirty[simIdx + 1], basisFns_);
                    if (isTrappedDate_[simIdx])
                        coeffsUndTrapped_[simIdx] =
                            regression(paths[tmpTs - 1], underlyingValueTrapped[simIdx], basisFns_);
                } else
                    coeffsUndDirty_[simIdx] = Array(basisFns_.size(), 0.0);
            }
        }
        // on the last exercise date the payoff is max(underlying,0.0) ...
        if (isExercise) {
            if (isLastExercise) {
                for (Size i = 0; i < N; ++i) {
                    if (evalRegression(coeffsUndEx_[exIdx], paths[ts - 1][i], basisFns_) > 0) {
                        option[i] = underlyingValueEx[exIdx + 1][i];
                    }
                }
                coeffsItm_[exIdx] = Array(basisFns_.size(), 0.0); // on last exercise cv = 0
            } else {
                x.clear();
                y.clear();
                itmPaths.clear();
                exerciseValue.clear();
                // otherwise compute the exercise value and collect the itm cont vals...
                for (Size i = 0; i < N; ++i) {
                    Real tmp = evalRegression(coeffsUndEx_[exIdx], paths[ts - 1][i], basisFns_);
                    if (tmp > 0.0) {
                        exerciseValue.push_back(tmp);
                        itmPaths.push_back(i);
                        if (calibration) {
                            x.push_back(paths[ts - 1][i]);
                            y.push_back(option[i]);
                        }
                    }
                }
            }
            // calibrate the continuation value coefficients
            if (calibration) {
                if (basisFns_.size() <= x.size()) {
                    coeffsItm_[exIdx] = regression(x, y, basisFns_);
                } else {
                    coeffsItm_[exIdx] = Array(basisFns_.size(), 0.0);
                }
            }
        }
        if (calibration && isSimulation) {
            // full continuation value coefficients
            if (isLastExercise)
                coeffsFull_[simIdx] = Array(basisFns_.size(), 0.0);
            else {
                // compute coefficients on next exercise date
                Size tmpTs = ts;
                while (regressionOnExerciseOnly_ && tmpTs <= numIdx && exerciseIdx_[tmpTs - 1] == Null<Size>())
                    ++tmpTs;
                QL_REQUIRE(tmpTs <= numIdx, "tmpTs > numIdx, this is unexpected");
                coeffsFull_[simIdx] = regression(paths[tmpTs - 1], option, basisFns_);
            }
        }
        if (isExercise) {
            // early exercise decision + option value update
            for (Size i = 0; i < itmPaths.size(); ++i) {
                Real cv = evalRegression(coeffsItm_[exIdx], paths[ts - 1][itmPaths[i]], basisFns_);
                if (exerciseValue[i] > cv) {
                    option[itmPaths[i]] = underlyingValueEx[exIdx + 1][itmPaths[i]];
                }
            }
            isLastExercise = false;
        }
    } // end of roll back

    // compute underlying value
    resultUnderlyingNpv_ = std::accumulate(underlyingValueDirty[0].begin(), underlyingValueDirty[0].end(), 0.0) /
                           static_cast<Real>(N) * model_->numeraire(0, 0.0, 0.0, discountCurves_[0]);
    // if we have no exercise, the underlying value is the npv by definition
    resultValue_ = exercise_ == nullptr ? resultUnderlyingNpv_
                                        : std::accumulate(option.begin(), option.end(), 0.0) / static_cast<Real>(N) *
                                              model_->numeraire(0, 0.0, 0.0, discountCurves_[0]);
}

namespace {

// return floating rate coupon (underlying for capped floored ones) or null
boost::shared_ptr<FloatingRateCoupon> flrcpn(const boost::shared_ptr<CashFlow>& c) {
    auto cfc = boost::dynamic_pointer_cast<CappedFlooredCoupon>(c);
    if (cfc)
        return flrcpn(cfc->underlying());
    auto scfc = boost::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(c);
    if (scfc)
        return flrcpn(scfc->underlying());
    auto ic = boost::dynamic_pointer_cast<IndexedCoupon>(c);
    if (ic)
        return flrcpn(ic->underlying());
    return boost::dynamic_pointer_cast<FloatingRateCoupon>(c);
}

// is coupon fixed coupon (throws if coupon type not recognised)
bool isFixedCoupon(const boost::shared_ptr<CashFlow>& c, const Date& today) {
    if (auto bma = boost::dynamic_pointer_cast<AverageBMACoupon>(c))
        return bma->fixingDates().front() <= today;
    auto flc = flrcpn(c);
    if (flc != nullptr)
        return flc->fixingDate() <= today;
    else if (boost::dynamic_pointer_cast<FixedRateCoupon>(c) != nullptr ||
             boost::dynamic_pointer_cast<SimpleCashFlow>(c) != nullptr)
        return true;
    else if (auto indexed = boost::dynamic_pointer_cast<IndexedCoupon>(c))
        return isFixedCoupon(indexed->underlying(), today);
    else
        QL_FAIL("McMultiLegBaseEngine: unrecognised coupon type");
}

// return future fixing date or pay date
Date eventDate(const boost::shared_ptr<CashFlow>& c, const Date& today) {
    if (auto bma = boost::dynamic_pointer_cast<AverageBMACoupon>(c)) {
        if (bma->fixingDates().front() <= today)
            return c->date();
        else
            return bma->fixingDates().front();
    }
    auto flc = flrcpn(c);
    if (flc == nullptr || flc->fixingDate() <= today)
        return c->date();
    else
        return flc->fixingDate();
}

} // namespace

void McMultiLegBaseEngine::calculate() const {

    // check data set by derived engines
    QL_REQUIRE(leg_.size() > 0, "McMultiLegBaseEngine: no legs given");
    QL_REQUIRE(currency_.size() == leg_.size(), "McMultiLegBaseEngine: number of legs ("
                                                    << leg_.size() << ") does not match currencies ("
                                                    << currency_.size() << ")");
    QL_REQUIRE(payer_.size() == leg_.size(), "McMultiLegBaseEngine: number of legs ("
                                                 << leg_.size() << ") does not match payer flag (" << payer_.size()
                                                 << ")");

    // collect simulation times ...
    std::vector<Date> exerciseDates, simulationDates, exSimDates; // live dates
    const Handle<YieldTermStructure>& ts = model_->irlgm1f(0)->termStructure();
    Date today = Settings::instance().evaluationDate();
    times_.clear();
    pathDates_.clear();
    pathDates_.push_back(today);
    // ... which can be coupon fixing or payment times ...
    for (Size i = 0; i < leg_.size(); ++i) {
        std::vector<Time> legTimes;
        for (Size j = 0; j < leg_[i].size(); ++j) {
            // is the payment still relevant?
            if (leg_[i][j]->date() <= today)
                continue;
            Date d = eventDate(leg_[i][j], today);
            Real t = ts->timeFromReference(d);
            times_.push_back(t);
            pathDates_.push_back(d);
        }
    }
    // ... or exercise times ...
    if (exercise_ != nullptr) {
        for (Size i = 0; i < exercise_->dates().size(); ++i) {
            if (exercise_->dates()[i] <= today)
                continue;
            times_.push_back(ts->timeFromReference(exercise_->dates()[i]));
            exerciseDates.push_back(exercise_->dates()[i]);
            pathDates_.push_back(exercise_->dates()[i]);
        }
    }
    // ... or simulation times ...
    for (Size i = 0; i < simulationDates_.size(); ++i) {
        if (simulationDates_[i] <= today)
            continue;
        times_.push_back(ts->timeFromReference(simulationDates_[i]));
        simulationDates.push_back(simulationDates_[i]);
        pathDates_.push_back(simulationDates_[i]);
    }
    // ... join the times and make them unique ...
    std::sort(times_.begin(), times_.end());
    auto it = std::unique(times_.begin(), times_.end(), [](Real x, Real y) { return QuantLib::close_enough(x, y); });
    times_.resize(it - times_.begin());
    // ... same for the path dates, and check the size matches the times vector ...
    std::sort(pathDates_.begin(), pathDates_.end());
    auto it2 = std::unique(pathDates_.begin(), pathDates_.end());
    pathDates_.resize(it2 - pathDates_.begin());
    QL_REQUIRE(pathDates_.size() == times_.size() + 1, "McMultiLegBaseEngine::calculate(): path dates size ("
                                                           << pathDates_.size() << ") does not match times size ("
                                                           << times_.size() << ") + 1, this is unexpected.");
    // ... join exercise and simulation dates and make them unique ...
    exSimDates.insert(exSimDates.end(), exerciseDates.begin(), exerciseDates.end());
    exSimDates.insert(exSimDates.end(), simulationDates.begin(), simulationDates.end());
    std::sort(exSimDates.begin(), exSimDates.end());
    auto it3 = std::unique(exSimDates.begin(), exSimDates.end());
    exSimDates.resize(it3 - exSimDates.begin());

    // determine indexes in time vector
    indexes_.clear();
    exerciseIdx_.clear();
    simulationIdx_.clear();
    numEx_ = numSim_ = 0;
    for (Size i = 0; i < exSimDates.size(); ++i) {
        Time t = ts->timeFromReference(exSimDates[i]);
        Size index =
            std::find_if(times_.begin(), times_.end(), [t](const Time s) { return QuantLib::close_enough(s, t); }) -
            times_.begin();
        QL_REQUIRE(index < times_.size(),
                   "McMultiLegBaseEngine: did not find index for time " << t << ", this is unexpected.");
        indexes_.push_back(index);
        if (std::find(exerciseDates.begin(), exerciseDates.end(), exSimDates[i]) != exerciseDates.end()) {
            exerciseIdx_.push_back(numEx_);
            ++numEx_;
        } else {
            exerciseIdx_.push_back(Null<Size>());
        }
        if (std::find(simulationDates.begin(), simulationDates.end(), exSimDates[i]) != simulationDates.end()) {
            simulationIdx_.push_back(numSim_);
            ++numSim_;
        } else {
            simulationIdx_.push_back(Null<Size>());
        }
    }

    // simulation data for each simulation time (empty where not applicable)
    maxUndValDirtyIdx_ = 0;
    indexCcyIndex_.clear();
    payCcyNum_.clear();
    payCcyIndex_.clear();
    payFxIndex_.clear();
    maxExerciseIndex_.clear();
    maxDirtyNpvIndex_.clear();
    indexFwdCurve_.clear();
    indexDscCurve_.clear();
    modelIndex_.clear();
    observationTimeIndex_.clear();
    fixingDate_.clear();
    gearing_.clear();
    spread_.clear();
    accrualTime_.clear();
    nominal_.clear();
    payTime_.clear();
    cappedRate_.clear();
    flooredRate_.clear();
    isNakedOption_.clear();
    couponIndex_.clear();
    couponIndexFixingDate_.clear();
    couponIndexQuantity_.clear();
    //
    indexCcyIndex_.resize(times_.size());
    payCcyNum_.resize(times_.size());
    payCcyIndex_.resize(times_.size());
    payFxIndex_.resize(times_.size());
    maxExerciseIndex_.resize(times_.size());
    maxDirtyNpvIndex_.resize(times_.size());
    indexFwdCurve_.resize(times_.size());
    indexDscCurve_.resize(times_.size());
    modelIndex_.resize(times_.size());
    observationTimeIndex_.resize(times_.size());
    fixingDate_.resize(times_.size());
    gearing_.resize(times_.size());
    spread_.resize(times_.size());
    accrualTime_.resize(times_.size());
    nominal_.resize(times_.size());
    payTime_.resize(times_.size());
    cappedRate_.resize(times_.size());
    flooredRate_.resize(times_.size());
    isNakedOption_.resize(times_.size());
    couponIndex_.resize(times_.size());
    couponIndexFixingDate_.resize(times_.size());
    couponIndexQuantity_.resize(times_.size());
    //
    trappedCoupons_.clear();
    trappedCoupons_.resize(times_.size());
    isTrappedDate_.clear();
    isTrappedDate_.resize(simulationDates.size(), false);

    // determine last exercise dates before sim dates (used below for trapped coupons)
    std::vector<Date> lastExDateBeforeSim(simulationDates.size(), Date::minDate());
    for (Size i = 0; i < simulationDates.size(); ++i) {
        auto it = std::lower_bound(exerciseDates.begin(), exerciseDates.end(), simulationDates[i]);
        if (it == exerciseDates.begin())
            continue;
        --it;
        lastExDateBeforeSim[i] = *it;
    }

    // loop through all coupons
    for (Size i = 0; i < leg_.size(); ++i) {
        for (Size j = 0; j < leg_[i].size(); ++j) {

            // is the payment still relevant?

            if (leg_[i][j]->date() <= today)
                continue;

            // look up the time index

            Date d = eventDate(leg_[i][j], today);
            Time t = ts->timeFromReference(d);
            Size index =
                std::find_if(times_.begin(), times_.end(), [t](const Time s) { return QuantLib::close_enough(t, s); }) -
                times_.begin();
            QL_REQUIRE(index < times_.size(),
                       "McMultiLegBaseEngine: did not find index for time " << t << ", this is unexpected.");

            // this is the maximum exercise / sim date such that the coupon or flow belongs to it or requires
            // a trapped coupon computation (to determine the mimimal possible observation time for vanilla coupons)
            Date minObsDatePossible = Date::minDate();

            // determine max exercise index coupon or flow belongs to

            auto cpn = boost::dynamic_pointer_cast<Coupon>(leg_[i][j]);
            Date criterionDate = cpn != nullptr ? cpn->accrualStartDate() : leg_[i][j]->date();
            Size k = 0;
            for (Size i = 0; i < exerciseDates.size(); ++i) {
                if (exerciseDates[i] <= criterionDate) {
                    k = i + 1;
                    minObsDatePossible = std::max(minObsDatePossible, exerciseDates[i]);
                }
            }
            maxExerciseIndex_[index].push_back(k);

            // determine max dirty npv sim index flow belongs to

            criterionDate = leg_[i][j]->date();
            k = 0;
            for (Size i = 0; i < simulationDates.size(); ++i) {
                if (simulationDates[i] < criterionDate) {
                    k = i + 1;
                    minObsDatePossible = std::max(minObsDatePossible, simulationDates[i]);
                }
            }
            maxDirtyNpvIndex_[index].push_back(k);
            maxUndValDirtyIdx_ = std::max(maxUndValDirtyIdx_, k - 1);

            // populate trapped coupons' sim indexes

            std::vector<Size> vec;
            if (cpn != nullptr) {
                for (Size i = 0; i < simulationDates.size(); ++i) {
                    if (lastExDateBeforeSim[i] > cpn->accrualStartDate() && simulationDates[i] < cpn->date()) {
                        vec.push_back(i);
                        isTrappedDate_[i] = true;
                        minObsDatePossible = std::max(minObsDatePossible, simulationDates[i]);
                    }
                }
            }
            trappedCoupons_[index].push_back(vec);

            // handle coupon types

            Real multiplier = payer_[i];
            bool allowsVanillaValuation = true; // true if zero vol fwd value can be used for valuation

            if (isFixedCoupon(leg_[i][j], today)) {
                indexCcyIndex_[index].push_back(Null<Size>());
                indexFwdCurve_[index].push_back(nullptr);
                indexDscCurve_[index].push_back(nullptr);
                modelIndex_[index].push_back(nullptr);
                fixingDate_[index].push_back(Null<Date>());
                gearing_[index].push_back(1.0);
                spread_[index].push_back(cpn != nullptr ? cpn->rate() : 1.0);
                nominal_[index].push_back(multiplier * (cpn != nullptr ? cpn->nominal() : leg_[i][j]->amount()));
                cappedRate_[index].push_back(QL_MAX_REAL);
                flooredRate_[index].push_back(-QL_MAX_REAL);
                isNakedOption_[index].push_back(false);
            } else {
                auto flr = flrcpn(leg_[i][j]);
                QL_REQUIRE(flr != nullptr,
                           "McMultiLegBaseEngine: unexpected coupon type, expected fixed or (cf) floating rate coupon");

                // proxy ON compounded, averaged and BMA by ibor coupons, ibor coupons are handled below

                if (auto on = boost::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(cpn)) {
                    flr = boost::make_shared<IborCoupon>(
                        on->date(), on->nominal(), on->accrualStartDate(), on->accrualEndDate(), on->fixingDays(),
                        boost::make_shared<IborIndex>(
                            "proxy-ibor", (on->accrualEndDate() - on->accrualStartDate()) * Days, on->overnightIndex()->fixingDays(),
                            on->overnightIndex()->currency(), on->overnightIndex()->fixingCalendar(),
                            on->overnightIndex()->businessDayConvention(), on->overnightIndex()->endOfMonth(),
                            on->overnightIndex()->dayCounter(), on->overnightIndex()->forwardingTermStructure()),
                        on->gearing(), on->spread(), on->referencePeriodStart(), on->referencePeriodEnd(),
                        on->dayCounter(), true, Date());
                } else if (auto on = boost::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(cpn)) {
                    flr = boost::make_shared<IborCoupon>(
                        on->date(), on->nominal(), on->accrualStartDate(), on->accrualEndDate(), on->fixingDays(),
                        boost::make_shared<IborIndex>(
                            "proxy-ibor", (on->accrualEndDate() - on->accrualStartDate()) * Days, on->overnightIndex()->fixingDays(),
                            on->overnightIndex()->currency(), on->overnightIndex()->fixingCalendar(),
                            on->overnightIndex()->businessDayConvention(), on->overnightIndex()->endOfMonth(),
                            on->overnightIndex()->dayCounter(), on->overnightIndex()->forwardingTermStructure()),
                        on->gearing(), on->spread(), on->referencePeriodStart(), on->referencePeriodEnd(),
                        on->dayCounter(), true, Date());
                } else if (auto aon = boost::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCoupon>(cpn)) {
                    auto on = aon->underlying();
                    flr = boost::make_shared<IborCoupon>(
                        on->date(), on->nominal(), on->accrualStartDate(), on->accrualEndDate(), on->fixingDays(),
                        boost::make_shared<IborIndex>(
                            "proxy-ibor", (on->accrualEndDate() - on->accrualStartDate()) * Days, on->overnightIndex()->fixingDays(),
                            on->overnightIndex()->currency(), on->overnightIndex()->fixingCalendar(),
                            on->overnightIndex()->businessDayConvention(), on->overnightIndex()->endOfMonth(),
                            on->overnightIndex()->dayCounter(), on->overnightIndex()->forwardingTermStructure()),
                        on->gearing(), on->spread(), on->referencePeriodStart(), on->referencePeriodEnd(),
                        on->dayCounter(), true, Date());
                } else if (auto aon = boost::dynamic_pointer_cast<QuantExt::CappedFlooredAverageONIndexedCoupon>(cpn)) {
                    auto on = aon->underlying();
                    flr = boost::make_shared<IborCoupon>(
                        on->date(), on->nominal(), on->accrualStartDate(), on->accrualEndDate(), on->fixingDays(),
                        boost::make_shared<IborIndex>(
                            "proxy-ibor", (on->accrualEndDate() - on->accrualStartDate()) * Days, on->overnightIndex()->fixingDays(),
                            on->overnightIndex()->currency(), on->overnightIndex()->fixingCalendar(),
                            on->overnightIndex()->businessDayConvention(), on->overnightIndex()->endOfMonth(),
                            on->overnightIndex()->dayCounter(), on->overnightIndex()->forwardingTermStructure()),
                        on->gearing(), on->spread(), on->referencePeriodStart(), on->referencePeriodEnd(),
                        on->dayCounter(), true, Date());
                } else if (auto bma = boost::dynamic_pointer_cast<QuantLib::AverageBMACoupon>(cpn)) {
                    auto bmaIndex = boost::dynamic_pointer_cast<BMAIndex>(bma->index());
                    QL_REQUIRE(bmaIndex, "McMultiLegBaseEngine: could not cast to BMAIndex, internal error.");
                    flr = boost::make_shared<IborCoupon>(
                        bma->date(), bma->nominal(), bma->accrualStartDate(), bma->accrualEndDate(), bma->fixingDays(),
                        boost::make_shared<IborIndex>(
                            "proxy-ibor", (bma->accrualEndDate() - bma->accrualStartDate()) * Days, bmaIndex->fixingDays(),
                            bmaIndex->currency(), bmaIndex->fixingCalendar(), Following, false, bmaIndex->dayCounter(),
                            bmaIndex->forwardingTermStructure()),
                        bma->gearing(), bma->spread(), bma->referencePeriodStart(), bma->referencePeriodEnd(),
                        bma->dayCounter(), false, Date());
                }

                // in arrears?

                if (flr->isInArrears())
                    allowsVanillaValuation = false;

                // cap / floors

                Real cappedRate = QL_MAX_REAL, flooredRate = -QL_MAX_REAL;
                bool isNakedOption = false;
                if (auto cfc = boost::dynamic_pointer_cast<CappedFlooredCoupon>(leg_[i][j])) {
                    allowsVanillaValuation = false;
                    if (cfc->isCapped())
                        cappedRate = cfc->cap();
                    if (cfc->isFloored())
                        flooredRate = cfc->floor();
                } else if (auto scfc = boost::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(leg_[i][j])) {
                    allowsVanillaValuation = false;
                    isNakedOption = true;
                    if (scfc->isCap())
                        cappedRate = scfc->cap();
                    if (scfc->isFloor())
                        flooredRate = scfc->floor();
                } else if (auto cfonc = boost::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCoupon>(leg_[i][j])) {
                    allowsVanillaValuation = false;
                    isNakedOption = cfonc->nakedOption();
                    if (cfonc->isCapped())
                        cappedRate = cfonc->cap();
                    if (cfonc->isFloored())
                        flooredRate = cfonc->floor();
                } else if (auto cfaonc = boost::dynamic_pointer_cast<QuantExt::CappedFlooredAverageONIndexedCoupon>(leg_[i][j])) {
                    allowsVanillaValuation = false;
                    isNakedOption = cfaonc->nakedOption();
                    if (cfaonc->isCapped())
                        cappedRate = cfaonc->cap();
                    if (cfaonc->isFloored())
                        flooredRate = cfaonc->floor();
                }
                cappedRate_[index].push_back(cappedRate);
                flooredRate_[index].push_back(flooredRate);
                isNakedOption_[index].push_back(isNakedOption);

                // handle the floating rate coupon types

                Size ccyIndex = model_->ccyIndex(flr->index()->currency());
                fixingDate_[index].push_back(flr->fixingDate());
                gearing_[index].push_back(flr->gearing());
                spread_[index].push_back(flr->spread());
                nominal_[index].push_back(multiplier * flr->nominal());

                bool cpnrec = false;

                if (auto iborcpn = boost::dynamic_pointer_cast<IborCoupon>(flr)) {
                    // IBOR
                    indexCcyIndex_[index].push_back(model_->pIdx(CrossAssetModel::AssetType::IR, ccyIndex));
                    indexFwdCurve_[index].push_back(boost::make_shared<LgmImpliedYtsFwdFwdCorrected>(
                        model_->lgm(ccyIndex), iborcpn->iborIndex()->forwardingTermStructure()));
                    indexDscCurve_[index].push_back(nullptr);
                    modelIndex_[index].push_back(
                        iborcpn->iborIndex()->clone(Handle<YieldTermStructure>(indexFwdCurve_[index].back())));
                    cpnrec = true;
                } else if (auto cmscpn = boost::dynamic_pointer_cast<CmsCoupon>(flr)) {
                    // CMS
                    allowsVanillaValuation = false;
                    indexCcyIndex_[index].push_back(model_->pIdx(CrossAssetModel::AssetType::IR, ccyIndex));
                    indexFwdCurve_[index].push_back(boost::make_shared<LgmImpliedYtsFwdFwdCorrected>(
                        model_->lgm(ccyIndex), cmscpn->swapIndex()->forwardingTermStructure()));
                    indexDscCurve_[index].push_back(boost::make_shared<LgmImpliedYtsFwdFwdCorrected>(
                        model_->lgm(ccyIndex), cmscpn->swapIndex()->discountingTermStructure()));
                    modelIndex_[index].push_back(
                        cmscpn->swapIndex()->clone(Handle<YieldTermStructure>(indexFwdCurve_[index].back())));
                    cpnrec = true;
                }
                // add more coupon types here ...

                QL_REQUIRE(cpnrec, "McMultiLegBaseEngine: floating rate coupon type in leg " << i << ", coupon " << j
                                                                                             << " not supported.");
            }

            // indexed coupon
            boost::shared_ptr<Index> couponIndex = nullptr;
            Date couponIndexFixingDate = Null<Date>();
            Real couponIndexQuantity = Null<Real>();
            if (auto ic = boost::dynamic_pointer_cast<IndexedCoupon>(leg_[i][j])) {
                couponIndex = ic->index();
                couponIndexFixingDate = ic->fixingDate();
                couponIndexQuantity = ic->quantity();
                if (!QuantLib::close_enough(ic->multiplier(), 0.0))
                    nominal_[index][nominal_[index].size()-1] /= ic->multiplier();
            }
            couponIndex_[index].push_back(couponIndex);
            couponIndexFixingDate_[index].push_back(couponIndexFixingDate);
            couponIndexQuantity_[index].push_back(couponIndexQuantity);

            // other data
            payCcyNum_[index].push_back(model_->ccyIndex(currency_[i]));
            payCcyIndex_[index].push_back(model_->pIdx(CrossAssetModel::AssetType::IR, payCcyNum_[index].back()));
            payFxIndex_[index].push_back(payCcyNum_[index].back() == 0 ? Null<Size>()
                                                                       : model_->pIdx(CrossAssetModel::AssetType::FX,
                                                                                      payCcyNum_[index].back() - 1));
            accrualTime_[index].push_back(cpn != nullptr ? cpn->accrualPeriod() : 1.0);
            payTime_[index].push_back(ts->timeFromReference(leg_[i][j]->date()));

            // set minimal possible path index that can be used for a zero vol fwd estimation of a coupon
            if (minimalObsDate_ && allowsVanillaValuation) {
                if (minObsDatePossible == Date::minDate())
                    observationTimeIndex_[index].push_back(0);
                else {
                    auto it = std::find(exSimDates.begin(), exSimDates.end(), minObsDatePossible);
                    QL_REQUIRE(it != exSimDates.end(), "McMultiLegBaseEngine: minObsDatePossible ("
                                                           << minObsDatePossible
                                                           << ") not found in ex/sim dates, this is unexpected");
                    observationTimeIndex_[index].push_back(
                        std::min(index + 1, indexes_[std::distance(exSimDates.begin(), it)] + 1));
                }
            } else {
                observationTimeIndex_[index].push_back(Null<Size>());
            }

        } // for j (coupons in leg i)
    }     // for i (legs)

    // build the path generators and do the mc simulation

    TimeGrid timeGrid(times_.begin(), times_.end());
    pathGeneratorCalibration_ = makeMultiPathGenerator(calibrationPathGenerator_, model_->stateProcess(), timeGrid,
                                                       calibrationSeed_, ordering_, directionIntegers_);
    pathGeneratorPricing_ = makeMultiPathGenerator(pricingPathGenerator_, model_->stateProcess(), timeGrid,
                                                   pricingSeed_, ordering_, directionIntegers_);
    // calibration phase
    rollback(true);

    // maybe we don't want a separate pricing run (then we just take the calibration run results)
    if (pricingSamples_ > 0)
        rollback(false);
}

boost::shared_ptr<AmcCalculator> McMultiLegBaseEngine::amcCalculator() const {
    return boost::make_shared<MultiLegBaseAmcCalculator>(
        model_->irlgm1f(0)->currency(), model_->stateProcess()->initialValues(), externalModelIndices_,
        exercise_ != nullptr, optionSettlement_ == Settlement::Physical, times_, indexes_, exerciseIdx_, simulationIdx_,
        isTrappedDate_, numSim_, resultValue_, coeffsItm_, coeffsUndEx_, coeffsFull_, coeffsUndTrapped_,
        coeffsUndDirty_, basisFns_);
}

std::vector<QuantExt::RandomVariable>
MultiLegBaseAmcCalculator::simulatePath(const std::vector<QuantLib::Real>& pathTimes,
                                        std::vector<std::vector<QuantExt::RandomVariable>>& paths,
                                        const std::vector<bool>& isRelevantTime, const bool stickyCloseOutRun) {

    QL_REQUIRE(!paths.empty(), "MultiLegBaseAmcCalculator: no future path times, this is not allowed.");
    QL_REQUIRE(pathTimes.size() == paths.size(), "MultiLegBaseAmcCalculator: inconsistent pathTimes size ("
                                                     << pathTimes.size() << ") and paths size (" << paths.size()
                                                     << ") - internal error.");

    if(storedExerciseIndex_.empty())
        storedExerciseIndex_.resize(paths.front().front().size());

    std::vector<Real> simTimes(1, 0.0);
    for (Size i = 0; i < pathTimes.size(); ++i) {
        if (isRelevantTime[i]) {
            int ind = stickyCloseOutRun ? i - 1 : i;
            QL_REQUIRE(ind >= 0,
                       "MultiLegBaseAmcCalculator: sticky close out run time index is negative - internal error.");
            simTimes.push_back(pathTimes[ind]);
        }
    }
    TimeGrid timeGrid(simTimes.begin(), simTimes.end());

    std::vector<QuantExt::RandomVariable> result(simTimes.size(), RandomVariable(paths.front().front().size(), 0.0));

    for (Size sample = 0; sample < paths.front().front().size(); ++sample) {

        MultiPath path(externalModelIndices_.size(), timeGrid);
        for (Size j = 0; j < externalModelIndices_.size(); ++j) {
            path[j][0] = x0_[j];
        }

        Size timeIndex = 0;
        for (Size i = 0; i < pathTimes.size(); ++i) {
            if (isRelevantTime[i]) {
                ++timeIndex;
                for (Size j = 0; j < externalModelIndices_.size(); ++j) {
                    path[j][timeIndex] = paths[i][externalModelIndices_[j]][sample];
                }
            }
        }

        auto res = simulatePath(path, stickyCloseOutRun, sample);

        for (Size k = 0; k < res.size(); ++k) {
            result[k].set(sample, res[k]);
        }
    }

    return result;
}

Array MultiLegBaseAmcCalculator::simulatePath(const MultiPath& path, const bool reuseLastEvents, const Size sample) {
    // we assume that the path exactly contains the simulation times (and 0)
    // we only check that the path has the right length here
    QL_REQUIRE(path[0].length() == numSim_ + 1, "MultiLegBaseAmcCalculator::simulatePath: expected path size "
                                                    << numSim_ + 1 << ", got " << path[0].length());
    Array result(numSim_ + 1, 0.0);
    result[0] = resultValue_; // reference date npv
    Array s(externalModelIndices_.size());
    // if we don't have an exercise, we always return the dirty npv of the underlying
    if (!hasExercise_) {
        for (Size i = 0; i < indexes_.size(); ++i) {
            Size simIdx = simulationIdx_[i];
            if (simIdx == Null<Size>())
                continue;
            for (Size j = 0; j < externalModelIndices_.size(); ++j) {
                s[j] = path[j][simIdx + 1];
            }
            result[simIdx + 1] = evalRegression(coeffsUndDirty_[simIdx], s, basisFns_);
        }
        return result;
    }
    // otherwise first determine whether the trade is exercised and if so, at which exercise time
    Size exIdx = Null<Size>();
    if (reuseLastEvents) {
        // if reuseLastEvents is true, we set the exercise index to the one from the last simulationPath() call
        exIdx = storedExerciseIndex_[sample];
    } else {
        // otherweise we determine the exercise index explicitly
        for (Size i = 0; i < indexes_.size(); ++i) {
            if (indexes_[i] == Null<Size>())
                continue;
            Size exIdxTmp = exerciseIdx_[i];
            if (exIdxTmp == Null<Size>())
                continue;
            Real tex = times_[indexes_[i]];
            Size ind =
                std::lower_bound(path[0].timeGrid().begin(), path[0].timeGrid().end(), tex,
                                 [](const Real a, const Real b) { return a < b && !QuantLib::close_enough(a, b); }) -
                path[0].timeGrid().begin();
            // if the exercise time is after the last simulation time, we do not exercise on the sim path
            if (ind >= path[0].length())
                break;
            QL_REQUIRE(ind > 0, "MultiLegBaseAmcCalculator: t_ex=" << tex << " yields invalid index " << ind);
            // we just interpolate between the adjacent path states to get a proxy for
            // the state on the exercise date (assuming the simulation grid is fine enough)
            // TODO a better alternative would be to use a Brownian Bridge
            Real t1 = path[0].time(ind - 1), t2 = path[0].time(ind);
            Array s1(externalModelIndices_.size()), s2(externalModelIndices_.size());
            for (Size j = 0; j < externalModelIndices_.size(); ++j) {
                s1[j] = path[j][ind - 1];
                s2[j] = path[j][ind];
            }
            s = ((tex - t1) * s2 + (t2 - tex) * s1) / (t2 - t1);
            Real exVal = evalRegression(coeffsUndEx_[exIdxTmp], s, basisFns_);
            if (exVal > 0.0) {
                Real contVal = evalRegression(coeffsItm_[exIdxTmp], s, basisFns_);
                if (exVal > contVal) {
                    exIdx = exIdxTmp;
                    break;
                }
            }
        }
        storedExerciseIndex_[sample] = exIdx;
    }
    // now populate the result array using the exercise time information
    bool exercised = false, exercisedNow = false;
    for (Size i = 0; i < indexes_.size(); ++i) {
        Size idx = indexes_[i];
        if (idx == Null<Size>())
            continue;
        if (!exercised && exerciseIdx_[i] != Null<Size>() && exerciseIdx_[i] == exIdx) {
            exercisedNow = exercised = true;
        }
        Size simIdx = simulationIdx_[i];
        if (simIdx == Null<Size>())
            continue;
        for (Size j = 0; j < externalModelIndices_.size(); ++j) {
            s[j] = path[j][simIdx + 1];
        }
        if (exercisedNow) {
            // we are on the first sim date on or after the exercise ...
            if (exIdx == exerciseIdx_[i]) {
                // ... if we are on the exercise date, we can use the regression coeffs from there
                result[simIdx + 1] = std::max(evalRegression(coeffsUndEx_[exIdx], s, basisFns_), 0.0);
            } else if (isTrappedDate_[simIdx]) {
                // ... otherwise underlying dirty, but excluding any trapped coupons for that
                // sim date, if there are any
                result[simIdx + 1] = evalRegression(coeffsUndTrapped_[simIdx], s, basisFns_);
            } else {
                // otherwise underlying dirty in the usual sense
                result[simIdx + 1] = evalRegression(coeffsUndDirty_[simIdx], s, basisFns_);
            }
            exercisedNow = false;
        } else if (exercised) {
            // if exercised before, underlying dirty in case of physical settlement
            if (isPhysicalSettlement_) {
                result[simIdx + 1] = evalRegression(coeffsUndDirty_[simIdx], s, basisFns_);
            }
        } else {
            // if not yet exercised, cv
            result[simIdx + 1] = std::max(evalRegression(coeffsFull_[simIdx], s, basisFns_), 0.0);
        }
    }
    return result;
}

} // namespace QuantExt
