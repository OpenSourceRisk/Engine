/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/numericlgmflexiswapengine.hpp>

namespace QuantExt {

namespace {
// this op is not defined for QL arrays, but it's handy for the rollback below
Array max(Array x, const Real b) {
    std::transform(x.begin(), x.end(), x.begin(), [b](const Real u) { return std::max(u, b); });
    return x;
}
Array max(Array x, const Array& y) {
    QL_REQUIRE(x.size() == y.size(),
               "max(Array,Array) requires arrays of equal size, got " << x.size() << " and " << y.size());
    for (Size i = 0; i < x.size(); ++i) {
        x[i] = std::max(x[i], y[i]);
    }
    return x;
}
} // namespace

NumericLgmFlexiSwapEngineBase::NumericLgmFlexiSwapEngineBase(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                                             const Real sy, const Size ny, const Real sx, const Size nx,
                                                             const Handle<YieldTermStructure>& discountCurve,
                                                             const Method method, const Real singleSwaptionThreshold)
    : LgmConvolutionSolver(model, sy, ny, sx, nx), discountCurve_(discountCurve), method_(method),
      singleSwaptionThreshold_(singleSwaptionThreshold) {}

Real NumericLgmFlexiSwapEngineBase::underlyingValue(const Real x, const Real t, const Date& d, const Size fltIndex,
                                                    const Size fixIndex, const Real fltPayTime,
                                                    const Real fixPayTime) const {
    Real val = 0.0;
    Real om = type == VanillaSwap::Payer ? -1.0 : 1.0;
    if (fixIndex != Null<Size>()) {
        if (!QuantLib::close_enough(fixedNominal[fixIndex], 0.0))
            val = om * fixedCoupons[fixIndex] / fixedNominal[fixIndex] *
                  model()->reducedDiscountBond(t, fixPayTime, x, discountCurve_);
    }
    if (fltIndex != Null<Size>()) {
        iborModelCurve_->move(d, x);
        Real fixing = floatingGearings[fltIndex] * iborModelIndex_->fixing(d) + floatingSpreads[fltIndex];
        if (cappedRate[fltIndex] != Null<Real>())
            fixing = std::min(fixing, cappedRate[fltIndex]);
        if (flooredRate[fltIndex] != Null<Real>())
            fixing = std::max(fixing, flooredRate[fltIndex]);
        val += -om * fixing * floatingAccrualTimes[fltIndex] *
               model()->reducedDiscountBond(t, fltPayTime, x, discountCurve_);
    }
    return val;
} // NumericLgmFlexiSwapEngineBase::underlyingValue()

std::pair<Real, Real> NumericLgmFlexiSwapEngineBase::calculate() const {

    Date today = model()->parametrization()->termStructure()->referenceDate();
    Real phi = optionPosition == Position::Long ? 1.0 : -1.0;

    // the event times are the floating leg's fixing times > 0 and zero itself, also mark those events where a notional
    // decrease is admissable we might miss some payments, those are handled below
    QL_REQUIRE(fixedNominal.size() > 0, "NumericLgmFlexiSwapEngine::calculate(): fixed nominal size is zero");
    QL_REQUIRE(floatingNominal.size() > 0, "NumericLgmFlexiSwapEngine::calculate(): floating nominal size is zero");
    Size legRatio = floatingNominal.size() / fixedNominal.size(); // we know there is no remainder
    std::vector<Real> times;
    std::vector<Date> dates;
    std::vector<Size> fixCpnIndex, fltCpnIndex; // argument's vector index per event date / time
    std::vector<Real> fixPayTime, fltPayTime;   // per event date / time
    Size firstAliveIndex = Null<Size>();
    for (Size i = 0; i < floatingFixingDates.size(); ++i) {
        Date d = floatingFixingDates[i];
        if (d > today) {
            if (firstAliveIndex == Null<Size>())
                firstAliveIndex = i;
            times.push_back(model()->parametrization()->termStructure()->timeFromReference(d));
            dates.push_back(d);
            fltCpnIndex.push_back(i);
            fltPayTime.push_back(model()->parametrization()->termStructure()->timeFromReference(floatingPayDates[i]));
            if (i % legRatio == 0) {
                Size idx = static_cast<Size>(i / legRatio);
                fixCpnIndex.push_back(idx);
                fixPayTime.push_back(
                    model()->parametrization()->termStructure()->timeFromReference(fixedPayDates[idx]));
            } else {
                fixCpnIndex.push_back(Null<Size>());
                fixPayTime.push_back(Null<Real>());
            }
        }
    }

    int n = times.size();

    // construct swaption basket

    std::vector<Real> swaptionVolTmp;
    std::vector<Size> swaptionStartIdx, swaptionEndIdx;
    // skip indices where there is no optionality (and the notional might also still increase)
    // or where the lower notional bound is ignored, because the corresponding option date is in the past
    Size i = 0;
    while (i < fixedNominal.size() &&
           (QuantLib::close_enough(fixedNominal[i], lowerNotionalBound[i]) || i * legRatio < firstAliveIndex))
        ++i;
    Size firstIndex = i;
    for (; i < fixedNominal.size(); ++i) {
        // volume attach and detach points for which we have to generate swaptions
        Real currentVolUpper =
            i == firstIndex ? fixedNominal[firstIndex] : std::min(lowerNotionalBound[i - 1], fixedNominal[i]);
        Real currentVolLower = lowerNotionalBound[i];
        if (!QuantLib::close_enough(currentVolUpper, currentVolLower)) {
            for (Size j = i; j < fixedNominal.size(); ++j) {
                Real nextNotional = j == fixedNominal.size() - 1 ? 0.0 : fixedNominal[j + 1];
                if (nextNotional < currentVolUpper && !QuantLib::close_enough(currentVolLower, currentVolUpper)) {
                    Real tmpVol = std::min(currentVolUpper - nextNotional, currentVolUpper - currentVolLower);
                    if (!QuantLib::close_enough(tmpVol, 0.0)) {
                        swaptionStartIdx.push_back(i);
                        swaptionEndIdx.push_back(j + 1);
                        swaptionVolTmp.push_back(tmpVol);
                        currentVolUpper = std::max(nextNotional, currentVolLower);
                    }
                }
            }
            QL_REQUIRE(QuantLib::close_enough(currentVolUpper, currentVolLower),
                       "NumericLgmFlexiSwapEngine:calculate(): currentVolUpper ("
                           << currentVolUpper << ") does not match currentVolLower (" << currentVolLower
                           << "), this is unexpected");
        }
    }

    int m = swaptionVolTmp.size(); // number of generated swaptions

    // compute equivalent number of swaptions with full grid, decide on whether to price a swaption array
    // or a series of single swaptions

    Real fullGridSwaptions = 0.0;
    for (int i = 0; i < m; ++i) {
        fullGridSwaptions += static_cast<Size>(swaptionEndIdx[i] - swaptionStartIdx[i]);
    }
    fullGridSwaptions /= static_cast<Real>(n);
    Method effectiveMethod =
        method_ != Method::Automatic
            ? method_
            : (fullGridSwaptions < singleSwaptionThreshold_ ? Method::SingleSwaptions : Method::SwaptionArray);
    // if we do not have any swaptions, swaption array is the only method that works (and which is fast also,
    // since the operations on empty arrays do not cost much)
    if (m == 0)
        effectiveMethod = Method::SwaptionArray;

    // per event date, per swaption, indicator if coupon belongs to underlying
    std::vector<Array> underlyingMultiplier(n, Array(m, 0.0));
    // per event date, per swaption, indicator if exercise is possible
    std::vector<Array> exerciseIndicator(n, Array(m, 0.0));
    // per swaption, its notional
    Array notionals(m);

    for (int i = 0; i < m; ++i) {
        notionals[i] = swaptionVolTmp[i];
        for (Size j = swaptionStartIdx[i]; j < swaptionEndIdx[i]; ++j) {
            Size index = j * legRatio;
            if (notionalCanBeDecreased[j] && index >= firstAliveIndex) {
                index -= firstAliveIndex;
                exerciseIndicator[index][i] = 1.0;
                for (Size k = 0; k < legRatio; ++k) {
                    underlyingMultiplier[index + k][i] = swaptionVolTmp[i];
                }
            }
        }
    }

    // model linked ibor index curve

    iborModelCurve_ = QuantLib::ext::make_shared<LgmImpliedYtsFwdFwdCorrected>(model(), iborIndex->forwardingTermStructure());
    iborModelIndex_ = iborIndex->clone(Handle<YieldTermStructure>(iborModelCurve_));

    // x grid for each expiry

    // underlying u and continuation value v for single swaption (_s) and array swaption (_a) approach)
    std::vector<Array> u_a, v_a;
    std::vector<Real> u_s, v_s;
    if (effectiveMethod == Method::SingleSwaptions) {
        u_s.resize(gridSize(), 0.0);
        v_s.resize(gridSize(), 0.0);
    } else {
        u_a.resize(gridSize(), Array(m, 0.0));
        v_a.resize(gridSize(), Array(m, 0.0));
    }

    Real undValAll0 = 0.0;    // underlying value valued on the grid
    int undValAllIdx = n + 1; // index until which we have collected the underlying coupons
    Array value0(m, 0.0);     // option values

    // loop over swaption for single swaptions method, otherwise this is one loop
    for (int sw = (effectiveMethod == Method::SingleSwaptions ? m - 1 : 0); sw >= 0; --sw) {

        // per grid index underlying value (independent of swaptions, just to collect the underlying value)
        std::vector<Real> uAll(gridSize(), 0.0);

        // init at last grid point
        // determine last time index for relevant swaption if in single swaption mode
        // for the first swaption we start at the maximum always to make sure we collect
        // all coupons for the underlying value
        if (effectiveMethod == Method::SingleSwaptions && sw != m - 1) {
            QL_REQUIRE(swaptionEndIdx[sw] * legRatio >= firstAliveIndex,
                       "swaptionEndIndex[" << sw << "] * legRatio (" << legRatio << ") < firstAliveIndex ("
                                           << firstAliveIndex << ") - this is unexpected.");
            n = swaptionEndIdx[sw] * legRatio - firstAliveIndex;
        }

        auto states = stateGrid(times[n - 1]);
        for (Size k = 0; k < gridSize(); ++k) {
            Real tmp = underlyingValue(states[k], times[n - 1], dates[n - 1], fltCpnIndex[n - 1], fixCpnIndex[n - 1],
                                       fltPayTime[n - 1], fixPayTime[n - 1]);
            // we can use the floating notional for both legs, since they have a consistent notional by construction
            if (n < undValAllIdx) {
                uAll[k] = tmp * floatingNominal[fltCpnIndex[n - 1]];
            }
            if (effectiveMethod == Method::SingleSwaptions) {
                u_s[k] = tmp * underlyingMultiplier[n - 1][sw];
                v_s[k] = exerciseIndicator[n - 1][sw] * std::max(-phi * u_s[k], 0.0);
            } else {
                u_a[k] = tmp * underlyingMultiplier[n - 1];
                v_a[k] = exerciseIndicator[n - 1] * max(-phi * u_a[k], 0.0);
            }
        }

        // roll back to first positive event time (in single swaption mode this might be > 1 though)
        // for the last swaption we roll back to 1 in every case to make sure that we collect all
        // coupons for the underlying value

        int minIndex = 0;
        if (effectiveMethod == Method::SingleSwaptions && sw != 0) {
            QL_REQUIRE(swaptionStartIdx[sw] * legRatio >= firstAliveIndex,
                       "swaptionStartIndex[" << sw << "] * legRatio (" << legRatio << ") < firstAliveIndex ("
                                             << firstAliveIndex << ") - this is unexpected.");
            minIndex = swaptionStartIdx[sw] * legRatio - firstAliveIndex;
        }

        for (int j = n - 1; j > minIndex; j--) {
            // rollback
            auto states = stateGrid(times[j - 1]);
            if (effectiveMethod == Method::SingleSwaptions) {
                u_s = rollback(u_s, times[j], times[j - 1]);
                v_s = rollback(v_s, times[j], times[j - 1]);
            } else {
                u_a = rollback(u_a, times[j], times[j - 1], Array(m, 0.0));
                v_a = rollback(v_a, times[j], times[j - 1], Array(m, 0.0));
            }
            if (j < undValAllIdx) {
                uAll = rollback(uAll, times[j], times[j - 1]);
            }
            // update
            for (Size k = 0; k < gridSize(); ++k) {
                Real tmp = underlyingValue(states[k], times[j - 1], dates[j - 1], fltCpnIndex[j - 1],
                                           fixCpnIndex[j - 1], fltPayTime[j - 1], fixPayTime[j - 1]);
                uAll[k] += (j < undValAllIdx ? tmp * floatingNominal[fltCpnIndex[j - 1]] : 0.0);
                if (effectiveMethod == Method::SingleSwaptions) {
                    u_s[k] += tmp * underlyingMultiplier[j - 1][sw];
                    v_s[k] = exerciseIndicator[j - 1][sw] * std::max(v_s[k], -phi * u_s[k]) +
                             (1.0 - exerciseIndicator[j - 1][sw]) * v_s[k];
                } else {
                    u_a[k] += tmp * underlyingMultiplier[j - 1];
                    v_a[k] = exerciseIndicator[j - 1] * max(v_a[k], -phi * u_a[k]) +
                             (1.0 - exerciseIndicator[j - 1]) * v_a[k];
                }
            } // for k (lgm grid)
        }     // for j (options)

        // roll back to time zero

        if (effectiveMethod == Method::SingleSwaptions) {
            v_s = rollback(v_s, times[minIndex], 0.0);
        } else {
            v_a = rollback(v_a, times[minIndex], 0.0, Array(m, 0.0));
        }
        uAll = rollback(uAll, times[minIndex], 0.0);

        // update undValAllIndx
        undValAllIdx = minIndex + 1;

        // populate option values
        if (effectiveMethod == Method::SingleSwaptions)
            value0[sw] = v_s[0];
        else
            value0 = v_a[0];

        // update underlying value
        undValAll0 += uAll[0];

    } // for swaptions (in single swaption mode, otherwise this is a loop that runs once only)

    // handle coupons we omitted above and add them to underlying value

    Size minFltCpnIdx = fltCpnIndex.empty() ? 0 : *std::min_element(fltCpnIndex.begin(), fltCpnIndex.end());
    Size minFixCpnIdx = fixCpnIndex.empty() ? 0 : *std::min_element(fixCpnIndex.begin(), fixCpnIndex.end());

    for (Size i = 0; i < fixedCoupons.size(); ++i) {
        if (fixedPayDates[i] > today && i < minFixCpnIdx) {
            undValAll0 += (type == VanillaSwap::Payer ? -1.0 : 1.0) * fixedCoupons[i] *
                          (discountCurve_.empty() ? model()->parametrization()->termStructure() : discountCurve_)
                              ->discount(fixedPayDates[i]);
        }
    }

    for (Size i = 0; i < floatingCoupons.size(); ++i) {
        if (floatingPayDates[i] > today && i < minFltCpnIdx) {
            QL_REQUIRE(floatingCoupons[i] != Null<Real>(),
                       "NumericLgmFlexiSwapEngineBase: no floating coupon provided for fixing date "
                           << floatingFixingDates[i]);
            undValAll0 += (type == VanillaSwap::Payer ? 1.0 : -1.0) * floatingCoupons[i] *
                          (discountCurve_.empty() ? model()->parametrization()->termStructure() : discountCurve_)
                              ->discount(floatingPayDates[i]);
        }
    }

    // sum over option values
    Real sumOptions = std::accumulate(value0.begin(), value0.end(), 0.0);

    // debug logging
    // std::cerr << "Flexi-Swap pricing engine log:\n";
    // std::cerr << "===========================\n";
    // std::cerr << "underlying value = " << undValAll0 << "\n";
    // std::cerr << "option value     = " << phi * sumOptions << "\n";
    // std::cerr << "===========================\n";
    // std::cerr << "swaption basket (" << m << "):\n";
    // Real sumNot = 0.0;
    // for (Size i = 0; i < m; ++i) {
    //     std::cerr << "swaption #" << i << " (start,end)=(" << swaptionStartIdx[i] << "," << swaptionEndIdx[i]
    //               << ") notional = " << notionals[i] << " NPV = " << phi * value0[i] << "\n";
    //     sumNot += notionals[i];
    // }
    // std::cerr << "sum of swaption notional = " << sumNot << "\n";
    // std::cerr << "number of equivalent full swaptions = " << fullGridSwaptions
    //           << ", singleSwaptionThreshold = " << singleSwaptionThreshold_ << "\n";
    // std::cerr << "method = " << static_cast<int>(method_) << " (0=array, 1=single, 2=auto)\n";
    // details
    // std::cerr << "===========================\n";
    // std::cerr << "times  underlying  exercise\n";
    // for (Size i = 0; i < times.size(); ++i) {
    //     std::cerr << times[i] << " " << underlyingMultiplier[i] << " " << exerciseIndicator[i] << "\n";
    // }
    // end details
    // std::cerr << "Flex-Swap pricing engine log end" << std::endl;
    // end logging

    return std::make_pair(phi * sumOptions + undValAll0, undValAll0);

} // NumericLgmFlexiSwapEngineBase::calculate()

NumericLgmFlexiSwapEngine::NumericLgmFlexiSwapEngine(const QuantLib::ext::shared_ptr<LinearGaussMarkovModel>& model,
                                                     const Real sy, const Size ny, const Real sx, const Size nx,
                                                     const Handle<YieldTermStructure>& discountCurve,
                                                     const Method method, const Real singleSwaptionThreshold)
    : NumericLgmFlexiSwapEngineBase(model, sy, ny, sx, nx, discountCurve, method, singleSwaptionThreshold) {
    registerWith(this->model());
    registerWith(discountCurve_);
} // NumericLgmFlexiSwapEngine::NumericLgmFlexiSwapEngine

void NumericLgmFlexiSwapEngine::calculate() const {
    // set arguments in base engine
    type = arguments_.type;
    fixedNominal = arguments_.fixedNominal;
    floatingNominal = arguments_.floatingNominal;
    fixedResetDates = arguments_.fixedResetDates;
    fixedPayDates = arguments_.fixedPayDates;
    floatingAccrualTimes = arguments_.floatingAccrualTimes;
    floatingResetDates = arguments_.floatingResetDates;
    floatingFixingDates = arguments_.floatingFixingDates;
    floatingPayDates = arguments_.floatingPayDates;
    fixedCoupons = arguments_.fixedCoupons;
    fixedRate = arguments_.fixedRate;
    floatingGearings = arguments_.floatingGearings;
    floatingSpreads = arguments_.floatingSpreads;
    cappedRate = arguments_.cappedRate;
    flooredRate = arguments_.flooredRate;
    floatingCoupons = arguments_.floatingCoupons;
    iborIndex = arguments_.iborIndex;
    lowerNotionalBound = arguments_.lowerNotionalBound;
    optionPosition = arguments_.optionPosition;
    notionalCanBeDecreased = arguments_.notionalCanBeDecreased;

    // calculate and set results
    auto result = NumericLgmFlexiSwapEngineBase::calculate();
    results_.value = result.first;
    results_.underlyingValue = result.second;
    results_.additionalResults = getAdditionalResultsMap(model()->getCalibrationInfo());
} // NumericLgmFlexiSwapEngine::calculate

} // namespace QuantExt
