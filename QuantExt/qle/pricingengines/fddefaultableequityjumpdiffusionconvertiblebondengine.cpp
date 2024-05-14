/*
 Copyright (C) 2021 Quaternion Risk Management Ltd.
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

#include <qle/instruments/convertiblebond2.hpp>
#include <qle/methods/fdmdefaultableequityjumpdiffusionop.hpp>
#include <qle/pricingengines/fdconvertiblebondevents.hpp>
#include <qle/pricingengines/fddefaultableequityjumpdiffusionconvertiblebondengine.hpp>

#include <ql/cashflows/coupon.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/methods/finitedifferences/meshers/fdmmeshercomposite.hpp>
#include <ql/methods/finitedifferences/meshers/uniform1dmesher.hpp>
#include <ql/methods/finitedifferences/solvers/fdmbackwardsolver.hpp>
#include <ql/timegrid.hpp>

namespace QuantExt {

// get amount to be paid on call (or put) exercise dependent on outstanding notional and call details

Real getCallPriceAmount(const FdConvertibleBondEvents::CallData& cd, Real notional, Real accruals) {
    Real price = cd.price * notional;
    if (cd.priceType == ConvertibleBond2::CallabilityData::PriceType::Clean)
        price += accruals;
    if (!cd.includeAccrual)
        price -= accruals;
    return price;
}

// interpolate value from PDE planes
Real interpolateValueFromPlanes(const Real conversionRatio, const std::vector<Array>& value,
                                const std::vector<Real>& stochasticConversionRatios, const Size j) {
    if (value.size() == 1)
        return value.front()[j];
    // linear interpolation and flat extrapolation
    Size idx = std::distance(
        stochasticConversionRatios.begin(),
        std::upper_bound(stochasticConversionRatios.begin(), stochasticConversionRatios.end(), conversionRatio));
    if (idx == 0)
        return value.front()[j];
    else if (idx == stochasticConversionRatios.size())
        return value.back()[j];
    else {
        Real x0 = stochasticConversionRatios[idx - 1];
        Real x1 = stochasticConversionRatios[idx];
        Real y0 = value[idx - 1][j];
        Real y1 = value[idx][j];
        Real alpha = (x1 - conversionRatio) / (x1 - x0);
        return alpha * y0 + (1.0 - alpha) * y1;
    }
}

FdDefaultableEquityJumpDiffusionConvertibleBondEngine::FdDefaultableEquityJumpDiffusionConvertibleBondEngine(
    const Handle<DefaultableEquityJumpDiffusionModel>& model,
    const Handle<QuantLib::YieldTermStructure>& discountingCurve, const Handle<QuantLib::Quote>& discountingSpread,
    const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve, const Handle<QuantLib::Quote>& recoveryRate,
    const Handle<FxIndex>& fxConversion, const bool staticMesher, const Size timeStepsPerYear,
    const Size stateGridPoints, const Real mesherEpsilon, const Real mesherScaling,
    const std::vector<Real> conversionRatioDiscretisationGrid, const bool generateAdditionalResults)
    : model_(model), discountingCurve_(discountingCurve), discountingSpread_(discountingSpread),
      creditCurve_(creditCurve), recoveryRate_(recoveryRate), fxConversion_(fxConversion), staticMesher_(staticMesher),
      timeStepsPerYear_(timeStepsPerYear), stateGridPoints_(stateGridPoints), mesherEpsilon_(mesherEpsilon),
      mesherScaling_(mesherScaling), conversionRatioDiscretisationGrid_(conversionRatioDiscretisationGrid),
      generateAdditionalResults_(generateAdditionalResults) {
    registerWith(model_);
    registerWith(discountingCurve_);
    registerWith(discountingSpread_);
    registerWith(creditCurve_);
    registerWith(recoveryRate_);
    registerWith(fxConversion_);
}

void FdDefaultableEquityJumpDiffusionConvertibleBondEngine::calculate() const {

    // 0 if there are no cashflows in the underlying bond, we do not calculate anyything

    if (arguments_.cashflows.empty())
        return;

    // 1 set up events

    Date today = Settings::instance().evaluationDate();
    FdConvertibleBondEvents events(today, model_->volDayCounter(), arguments_.notionals.front(), model_->equity(),
                                   fxConversion_.empty() ? nullptr : *fxConversion_);

    // 1a bond cashflows

    for (Size i = 0; i < arguments_.cashflows.size(); ++i) {
        if (arguments_.cashflows[i]->date() > today) {
            events.registerBondCashflow(arguments_.cashflows[i]);
        }
    }

    // 1b call and put data

    for (auto const& c : arguments_.callData) {
        events.registerCall(c);
    }
    for (auto const& c : arguments_.putData) {
        events.registerPut(c);
    }

    // 1c conversion ratio data
    for (auto const& c : arguments_.conversionRatioData) {
        events.registerConversionRatio(c);
    }

    // 1d conversion data

    for (auto const& c : arguments_.conversionData) {
        events.registerConversion(c);
    }

    // 1e mandatory conversion data

    for (auto const& c : arguments_.mandatoryConversionData) {
        events.registerMandatoryConversion(c);
    }

    // 1f conversion reset data

    for (auto const& c : arguments_.conversionResetData) {
        events.registerConversionReset(c);
    }

    // 1g dividend protection data

    for (auto const& c : arguments_.dividendProtectionData) {
        events.registerDividendProtection(c);
    }

    // 1h make whole data

    events.registerMakeWhole(arguments_.makeWholeData);

    // 2 set up PDE time grid

    QL_REQUIRE(!events.times().empty(),
               "FdDefaultableEquityJumpDiffusionConvertibleEngine: internal error, times are empty");
    Size steps = std::max<Size>(std::lround(timeStepsPerYear_ * (*events.times().rbegin()) + 0.5), 1);
    TimeGrid grid(events.times().begin(), events.times().end(), steps);

    // 3 build mesher if we do not have one or if we want to rebuild the mesher every time

    Real spot = model_->equity()->equitySpot()->value();
    Real logSpot = std::log(spot);

    if (mesher_ == nullptr || !staticMesher_) {
        Real mi = spot;
        Real ma = spot;
        Real forward = spot;
        for (Size i = 1; i < grid.size(); ++i) {
            forward = spot * model_->equity()->equityDividendCurve()->discount(grid[i]) /
                      model_->equity()->equityForecastCurve()->discount(grid[i]);
            mi = std::min(mi, forward);
            ma = std::max(ma, forward);
        }
        Real sigmaSqrtT = std::max(1E-2 * std::sqrt(grid.back()), std::sqrt(model_->totalBlackVariance()));
        Real normInvEps = InverseCumulativeNormal()(1.0 - mesherEpsilon_);
        Real xMin = std::log(mi) - sigmaSqrtT * normInvEps * mesherScaling_;
        Real xMax = std::log(ma) + sigmaSqrtT * normInvEps * mesherScaling_;

        mesher_ = QuantLib::ext::make_shared<Uniform1dMesher>(xMin, xMax, stateGridPoints_);
    }

    // 4 set up functions accrual(t), notional(t), recovery(t, S)

    Real N0 = arguments_.notionals.front();
    std::vector<Real> notionalTimes = {0.0};
    std::vector<Real> notionals = {N0};
    std::vector<Real> couponAmounts, couponAccrualStartTimes, couponAccrualEndTimes, couponPayTimes;
    for (auto const& c : arguments_.cashflows) {
        if (c->date() <= today)
            continue;
        if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
            if (!close_enough(cpn->nominal(), notionals.back())) {
                notionalTimes.push_back(model_->timeFromReference(c->date()));
                notionals.push_back(cpn->nominal());
            }
            couponAmounts.push_back(cpn->amount());
            couponAccrualStartTimes.push_back(model_->timeFromReference(cpn->accrualStartDate()));
            couponAccrualEndTimes.push_back(model_->timeFromReference(cpn->accrualEndDate()));
            couponPayTimes.push_back(model_->timeFromReference(cpn->date()));
        }
    }

    auto notional = [&notionalTimes, &notionals](const Real t) {
        auto cn = std::upper_bound(notionalTimes.begin(), notionalTimes.end(), t,
                                   [](Real s, Real t) { return s < t && !close_enough(s, t); });
        return notionals[std::max<Size>(std::distance(notionalTimes.begin(), cn), 1) - 1];
    };

    auto recovery = [this, &notional, N0](const Real t, const Real S, const Real conversionRatio) {
        Real currentBondNotional = notional(t);
        Real rr = this->recoveryRate_.empty() ? 0.0 : this->recoveryRate_->value();
        Real conversionValue = 0.0;
        if (conversionRatio != Null<Real>())
            conversionValue = currentBondNotional / N0 * conversionRatio * S * (1.0 - this->model_->eta());
        if (!this->arguments_.exchangeableData.isExchangeable) {
            // recovery term for non-exchangeables
            return std::max(rr * currentBondNotional, conversionValue);
        } else {
            // equity-related recovery term for exchangeables (same for secured / non-secured)
            return currentBondNotional;
        }
    };

    std::function<Real(Real, Real, Real)> addRecovery;
    if (arguments_.exchangeableData.isExchangeable) {
        addRecovery = [this, &notional, N0](const Real t, const Real S, const Real conversionRatio) {
            Real currentBondNotional = notional(t);
            Real rr = this->recoveryRate_.empty() ? 0.0 : this->recoveryRate_->value();
            Real conversionValue = 0.0;
            if (conversionRatio != Null<Real>())
                conversionValue = currentBondNotional / N0 * conversionRatio * S * (1.0 - this->model_->eta());
            if (!this->arguments_.exchangeableData.isSecured) {
                // bond-related recovery term for exchangeables / non-secured
                return rr * currentBondNotional;
            } else {
                // bond-related recovery term for exchangeables / secured
                return conversionValue + rr * std::max(currentBondNotional - conversionValue, 0.0);
            }
        };
    }

    auto accrual = [&couponAmounts, &couponPayTimes, &couponAccrualStartTimes, &couponAccrualEndTimes](const Real t) {
        Real accruals = 0.0;
        for (Size i = 0; i < couponAmounts.size(); ++i) {
            if (couponPayTimes[i] > t && t > couponAccrualStartTimes[i]) {
                accruals += (t - couponAccrualStartTimes[i]) / (couponAccrualEndTimes[i] - couponAccrualStartTimes[i]) *
                            couponAmounts[i];
            }
        }
        return accruals;
    };

    // 5 build operator

    auto fdmOp = QuantLib::ext::make_shared<FdmDefaultableEquityJumpDiffusionOp>(
        QuantLib::ext::make_shared<FdmMesherComposite>(mesher_), *model_, 0, recovery, discountingCurve_, discountingSpread_,
        creditCurve_, addRecovery);

    // 6 build solver

    auto solver = QuantLib::ext::make_shared<FdmBackwardSolver>(
        fdmOp, std::vector<QuantLib::ext::shared_ptr<BoundaryCondition<FdmLinearOp>>>(), nullptr, FdmSchemeDesc::Douglas());

    // 7 prepare event container

    events.finalise(grid);

    // 8 set up discretisation grid for conversion ratio (for cr resets and div protection with cr adjustment)

    std::vector<Real> stochasticConversionRatios(1, Null<Real>());
    if (events.hasStochasticConversionRatio(grid.size() - 1)) {
        stochasticConversionRatios.resize(conversionRatioDiscretisationGrid_.size());
        for (Size i = 0; i < conversionRatioDiscretisationGrid_.size(); ++i) {
            stochasticConversionRatios[i] = events.getInitialConversionRatio() * conversionRatioDiscretisationGrid_[i];
        }
    }

    // 9 set boundary value at last grid point

    Size n = mesher_->locations().size();
    std::vector<Array> value(stochasticConversionRatios.size(), Array(n, 0.0)), valueTmp;
    std::vector<Array> conversionIndicator;
    if (generateAdditionalResults_)
        conversionIndicator.resize(stochasticConversionRatios.size(), Array(n, 0.0));

    // 10 add no-conversion variants for start of period coco feature

    std::vector<Array> valueNoConversion, valueNoConversionTmp;
    std::vector<Array> conversionIndicatorNoConversion;

    // 11 perform the backward PDE pricing

    Array S(mesher_->locations().begin(), mesher_->locations().end());
    S = Exp(S);

    for (Size i = grid.size() - 1; i > 0; --i) {

        // 11.1 we will roll back from t_i = t_from to t_{i-1} = t_to in this step

        Real t_from = grid[i];
        Real t_to = grid[i - 1];

        // 11.2 Create the no-conversion value array if required (for contingent conversion)

        if (events.hasNoConversionPlane(i) && valueNoConversion.empty()) {
            valueNoConversion = value;
            conversionIndicatorNoConversion = conversionIndicator;
        }

        // 11.3a handle voluntary (contingent) conversion on t_i (overrides call and put)

        std::vector<std::vector<bool>> conversionExercised(value.size(), std::vector<bool>(n, false));
        for (Size plane = 0; plane < value.size(); ++plane) {
            if (events.hasConversion(i)) {
                Real cr = value.size() > 1 ? stochasticConversionRatios[plane] : events.getCurrentConversionRatio(i);
                for (Size j = 0; j < n; ++j) {
                    bool cocoTriggered = true;
                    if (events.hasContingentConversion(i) && !events.hasNoConversionPlane(i)) {
                        cocoTriggered = cr * S[j] > events.getConversionData(i).cocoBarrier;
                        // update value from no conversion plane, if there is one and coco is not triggered
                        if (!valueNoConversion.empty()) {
                            if (!cocoTriggered) {
                                value[plane][j] = valueNoConversion[plane][j];
                                if (!conversionIndicator.empty())
                                    conversionIndicator[plane][j] = conversionIndicatorNoConversion[plane][j];
                            }
                        }
                    }
                    Real exerciseValue = S[j] * cr * notional(grid[i]) / N0 + accrual(grid[i]);
                    // see 11.9, if we do not exercise, we are entitled to receive the final redempion flow
                    if (cocoTriggered && exerciseValue > value[plane][j] + events.getBondFinalRedemption(i)) {
                        value[plane][j] = exerciseValue;
                        conversionExercised[plane][j] = true;
                        if (!conversionIndicator.empty())
                            conversionIndicator[plane][j] = 1.0;
                    }
                }
            }
        } // for plane

        // 11.3b collapse no conversion plane if adequate

        if (events.hasContingentConversion(i) && !events.hasNoConversionPlane(i) && !valueNoConversion.empty()) {
            valueNoConversion.clear();
            conversionIndicatorNoConversion.clear();
        }

        // 11.4 handle cr / DP induced cr resets and resets to specific value on t_i

        valueTmp = value;
        if (!valueNoConversion.empty())
            valueNoConversionTmp = valueNoConversion;

        for (Size plane = 0; plane < value.size(); ++plane) {
            if (events.hasConversionReset(i)) {
                // this implies we have several planes with stochasticConversionRatios filled
                const FdConvertibleBondEvents::ConversionResetData& rd = events.getConversionResetData(i);
                Array adjustedConversionRatio(n, Null<Real>());
                if (rd.resetActive) {
                    Real cr;
                    if (rd.reference == ConvertibleBond2::ConversionResetData::ReferenceType::CurrentCP) {
                        cr = value.size() > 1 ? stochasticConversionRatios[plane] : events.getCurrentConversionRatio(i);
                    } else {
                        cr = events.getInitialConversionRatio();
                    }
                    if (close_enough(cr, 0.0)) {
                        std::fill(adjustedConversionRatio.begin(), adjustedConversionRatio.end(), 0.0);
                    } else {
                        Real referenceCP = N0 / cr;
                        for (Size j = 0; j < n; ++j) {
                            if (S[j] < rd.threshold * referenceCP) {
                                adjustedConversionRatio[j] = QL_MAX_REAL;
                                if (!close_enough(rd.gearing, 0.0)) {
                                    adjustedConversionRatio[j] =
                                        std::min(adjustedConversionRatio[j], N0 / (rd.gearing * S[j]));
                                }
                                if (!close_enough(rd.floor, 0.0)) {
                                    adjustedConversionRatio[j] =
                                        std::min(adjustedConversionRatio[j], N0 / (rd.floor * referenceCP));
                                }
                                if (!close_enough(rd.globalFloor, 0.0)) {
                                    adjustedConversionRatio[j] =
                                        std::min(adjustedConversionRatio[j], N0 / (rd.globalFloor * referenceCP));
                                }
                                adjustedConversionRatio[j] =
                                    std::max(cr, adjustedConversionRatio[j] != QL_MAX_REAL ? adjustedConversionRatio[j]
                                                                                           : -QL_MAX_REAL);
                            }
                        }
                    }
                }
                if (rd.divProtActive) {
                    bool absolute = rd.dividendType == ConvertibleBond2::DividendProtectionData::DividendType::Absolute;
                    Real H = rd.divThreshold;
                    Real s = grid[rd.lastDividendProtectionTimeIndex + 1];
                    Real t = grid[i];
                    for (Size j = 0; j < n; ++j) {
                        // we might have adjusted the cr already above
                        if (adjustedConversionRatio[j] == Null<Real>()) {
                            adjustedConversionRatio[j] = value.size() > 1 ? stochasticConversionRatios[plane]
                                                                          : events.getCurrentConversionRatio(i);
                        }
                        Real D = rd.accruedHistoricalDividends +
                                 (std::exp(model_->dividendYield(s, t) * (t - s)) - 1.0) * S[j];
                        if (rd.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly ||
                            rd.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpDown) {
                            Real d = absolute ? D : D / S[j];
                            Real C = rd.adjustmentStyle ==
                                             ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly
                                         ? std::max(d - H, 0.0)
                                         : d - H;
                            adjustedConversionRatio[j] *= (absolute ? S[j] / std::max(S[j] - C, 1E-4) : (1.0 + C));
                        } else {
                            Real f = std::max(S[j] - H, 0.0) / std::max(S[j] - D, 1E-4);
                            if (rd.adjustmentStyle ==
                                ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly2)
                                f = std::max(f, 1.0);
                            adjustedConversionRatio[j] *= f;
                        }
                    }
                }
                for (Size j = 0; j < n; ++j) {
                    Real lookupValue = adjustedConversionRatio[j];
                    if (rd.resetToSpecificValue) {
                        lookupValue = rd.newCr;
                    }
                    if (lookupValue != Null<Real>()) {
                        // update value by interpolating from other planes if cr was reset on this date
                        valueTmp[plane][j] =
                            interpolateValueFromPlanes(lookupValue, value, stochasticConversionRatios, j);
                        if (!valueNoConversion.empty()) {
                            valueNoConversionTmp[plane][j] = interpolateValueFromPlanes(lookupValue, valueNoConversion,
                                                                                        stochasticConversionRatios, j);
                        }
                    }
                }
            } // has conversion reset
        }     // for plane (stoch cr)

        value.swap(valueTmp);
        if (!valueNoConversion.empty())
            valueNoConversion.swap(valueNoConversionTmp);

        // 11.5 collapse stochastic conversion ratio planes to one plane on t_i if possible

        if (!events.hasStochasticConversionRatio(i) && value.size() > 1) {
            Array collapsedValue(n);
            for (Size j = 0; j < n; ++j) {
                collapsedValue[j] = interpolateValueFromPlanes(events.getCurrentConversionRatio(i), value,
                                                               stochasticConversionRatios, j);
            }
            value = std::vector<Array>(1, collapsedValue);
            if (!valueNoConversion.empty()) {
                for (Size j = 0; j < n; ++j) {
                    collapsedValue[j] = interpolateValueFromPlanes(events.getCurrentConversionRatio(i),
                                                                   valueNoConversion, stochasticConversionRatios, j);
                }
                valueNoConversion = std::vector<Array>(1, collapsedValue);
            }
            if (!conversionIndicator.empty()) {
                for (Size j = 0; j < n; ++j) {
                    collapsedValue[j] = interpolateValueFromPlanes(events.getCurrentConversionRatio(i),
                                                                   conversionIndicator, stochasticConversionRatios, j);
                }
                conversionIndicator = std::vector<Array>(1, collapsedValue);
            }
            if (!conversionIndicatorNoConversion.empty()) {
                for (Size j = 0; j < n; ++j) {
                    collapsedValue[j] =
                        interpolateValueFromPlanes(events.getCurrentConversionRatio(i), conversionIndicatorNoConversion,
                                                   stochasticConversionRatios, j);
                }
                conversionIndicatorNoConversion = std::vector<Array>(1, collapsedValue);
            }
        }

        for (Size plane = 0; plane < value.size(); ++plane) {

            // 11.6 handle mandatory conversion (overwrites value from voluntary conversion if on same date)

            if (events.hasMandatoryConversion(i)) {
                for (Size j = 0; j < n; ++j) {
                    // PEPS
                    const auto& d = events.getMandatoryConversionData(i);
                    Real payoff = 0.0;
                    if (S[j] < d.pepsLowerBarrier) {
                        payoff = d.pepsLowerConversionRatio * S[j] * notional(grid[i]) / N0 + accrual(grid[i]);
                    } else if (S[j] > d.pepsUpperBarrier) {
                        payoff = d.pepsUpperConversionRatio * S[j] * notional(grid[i]) / N0 + accrual(grid[i]);
                    } else {
                        payoff = notional(grid[i]) + accrual(grid[i]);
                    }
                    value[plane][j] = payoff;
                    conversionExercised[plane][j] = true;
                    if (!valueNoConversion.empty())
                        valueNoConversion[plane][j] = payoff;
                    if (!conversionIndicator.empty())
                        conversionIndicator[plane][j] = 1.0;
                    if (!conversionIndicatorNoConversion.empty())
                        conversionIndicatorNoConversion[plane][j] = 1.0;
                }
            }

            // 11.7 handle call, put on t_i (assume put overrides call, if both are exercised)

            if (events.hasCall(i)) {
                Real c = getCallPriceAmount(events.getCallData(i), notional(t_from), accrual(t_from));
                Real cr0 = value.size() > 1 ? stochasticConversionRatios[plane] : events.getCurrentConversionRatio(i);
                for (Size j = 0; j < n; ++j) {
                    if (!conversionExercised[plane][j]) {
                        // check soft call trigger if applicable
                        if (!events.getCallData(i).isSoft ||
                            S[j] > events.getCallData(i).softTriggerRatio * notionals.front() / cr0) {
                            // apply mw cr increase if applicable
                            Real cr = cr0;
                            if (events.getCallData(i).mwCr)
                                cr = events.getCallData(i).mwCr(S[j], cr0);
                            // compuate forced conversion value and update npv node
                            Real forcedConversionValue = S[j] * cr * notional(grid[i]) / N0 + accrual(grid[i]);
                            if (forcedConversionValue > c && forcedConversionValue < value[plane][j]) {
                                // conversion is forced -> update flags
                                if (!conversionIndicator.empty())
                                    conversionIndicator[plane][j] = 0.0;
                                conversionExercised[plane][j] = false;
                            }
                            value[plane][j] = std::min(std::max(forcedConversionValue, c), value[plane][j]);
                            if (!valueNoConversion.empty()) {
                                valueNoConversion[plane][j] =
                                    std::min(std::max(forcedConversionValue, c), valueNoConversion[plane][j]);
                            }
                        }
                    }
                }
            }

            if (events.hasPut(i)) {
                Real c = getCallPriceAmount(events.getPutData(i), notional(t_from), accrual(t_from));
                for (Size j = 0; j < n; ++j) {
                    if (c > value[plane][j]) {
                        // put is more favorable than conversion (if that happened above)
                        value[plane][j] = c;
                        if (!conversionIndicator.empty())
                            conversionIndicator[plane][j] = 0.0;
                        conversionExercised[plane][j] = false;
                    }
                }
                if (!valueNoConversion.empty()) {
                    for (Size j = 0; j < n; ++j) {
                        valueNoConversion[plane][j] = std::max(c, valueNoConversion[plane][j]);
                    }
                }
            }

            // 11.8 handle dividend protection pass through on t_i, paid even if converted or called / put

            if (events.hasDividendPassThrough(i)) {
                const FdConvertibleBondEvents::DividendPassThroughData& d = events.getDividendPassThroughData(i);
                Real H = d.divThreshold;
                Real s = grid[d.lastDividendProtectionTimeIndex + 1];
                Real t = grid[i];
                Real cr = value.size() > 1 ? stochasticConversionRatios[plane] : events.getCurrentConversionRatio(i);
                for (Size j = 0; j < n; ++j) {
                    Real D =
                        d.accruedHistoricalDividends + (std::exp(model_->dividendYield(s, t) * (t - s)) - 1.0) * S[j];
                    Real a = d.adjustmentStyle ==
                                     ConvertibleBond2::DividendProtectionData::AdjustmentStyle::PassThroughUpOnly
                                 ? std::max(D - H, 0.0)
                                 : D - H;
                    value[plane][j] += a * cr;
                    if (!valueNoConversion.empty())
                        valueNoConversion[plane][j] += a * cr;
                }
            }

            // 11.9 handle bond cashflows on t_i (after calls / puts)

            if (events.hasBondCashflow(i)) {
                value[plane] += events.getBondCashflow(i);
                if (!valueNoConversion.empty())
                    valueNoConversion[plane] += events.getBondCashflow(i);
                // the final redemption flow is only paid, if no conversion was exercised on the same date
                // and if the bond is not perpetual
                for (Size j = 0; j < n; ++j) {
                    if (!conversionExercised[plane][j] && !arguments_.perpetual) {
                        value[plane][j] += events.getBondFinalRedemption(i);
                        if (!valueNoConversion.empty())
                            valueNoConversion[plane] += events.getBondFinalRedemption(i);
                    }
                }
            }

            // 11.10 set conversion rate function in operator for rollback

            Real cr = value.size() > 1 ? stochasticConversionRatios[plane] : events.getCurrentConversionRatio(i);
            fdmOp->setConversionRatio([cr](const Real S) { return cr; });

            // 11.11 roll back value from time t_i to t_i{-1}

            solver->rollback(value[plane], t_from, t_to, 1, 0);
            if (!valueNoConversion.empty())
                solver->rollback(valueNoConversion[plane], t_from, t_to, 1, 0);
            if (!conversionIndicator.empty())
                solver->rollback(conversionIndicator[plane], t_from, t_to, 1, 0);
            if (!conversionIndicatorNoConversion.empty())
                solver->rollback(conversionIndicatorNoConversion[plane], t_from, t_to, 1, 0);

        } // loop over stochastic conversion ratio planes

    } // loop over times (PDE rollback)

    // 12 do a second roll back to compute the bond floor (include final redemption even for perpetuals)

    fdmOp->setConversionRatio([](const Real S) { return Null<Real>(); });

    Array valueBondFloor(n, 0.0);
    for (Size i = grid.size() - 1; i > 0; --i) {
        Real t_from = grid[i];
        Real t_to = grid[i - 1];
        if (events.hasBondCashflow(i)) {
            valueBondFloor += events.getBondCashflow(i) + events.getBondFinalRedemption(i);
        }
        solver->rollback(valueBondFloor, t_from, t_to, 1, 0);
    }

    // 13 set result

    QL_REQUIRE(
        value.size() == 1,
        "FdDefaultableEquityJumpDiffusionConvertibleEngine: internal error, have "
            << value.size()
            << " pde planes after complete rollback, the planes should have been collapsed to one during the rollback");

    MonotonicCubicNaturalSpline interpolationValue(mesher_->locations().begin(), mesher_->locations().end(),
                                                   value[0].begin());
    MonotonicCubicNaturalSpline interpolationBondFloor(mesher_->locations().begin(), mesher_->locations().end(),
                                                       valueBondFloor.begin());
    interpolationValue.enableExtrapolation();
    interpolationBondFloor.enableExtrapolation();
    Real npv = interpolationValue(logSpot);
    Real npvBondFloor = interpolationBondFloor(logSpot);
    results_.additionalResults["BondFloor"] = npvBondFloor;

    results_.value = arguments_.detachable ? npv - npvBondFloor : npv;

    results_.settlementValue = results_.value; // FIXME this is not entirely correct of course

    // 14 set additional results, if not disabled

    if (!generateAdditionalResults_)
        return;

    // 14.1 output events table

    constexpr Size width = 12;
    std::ostringstream header;
    header << std::left << "|" << std::setw(width) << "time"
           << "|" << std::setw(width) << "date"
           << "|" << std::setw(width) << "notional"
           << "|" << std::setw(width) << "accrual"
           << "|" << std::setw(width) << "flow"
           << "|" << std::setw(width) << "call"
           << "|" << std::setw(width) << "put"
           << "|" << std::setw(2 * width) << "conversion"
           << "|" << std::setw(2 * width) << "CR_reset"
           << "|" << std::setw(width) << "div_passth"
           << "|" << std::setw(width) << "curr_cr"
           << "|" << std::setw(width) << "fxConv"
           << "|" << std::setw(width) << "eq_fwd"
           << "|" << std::setw(width) << "div_amt"
           << "|" << std::setw(width) << "conv_val"
           << "|" << std::setw(width) << "conv_prc"
           << "|";

    results_.additionalResults["event_0000!"] = header.str();

    Size counter = 0;
    for (Size i = 0; i < grid.size(); ++i) {
        if (true || events.hasBondCashflow(i) || events.hasCall(i) || events.hasPut(i) || events.hasConversion(i)) {
            std::ostringstream dateStr, bondFlowStr, callStr, putStr, convStr, convResetStr, divStr, currentConvStr,
                fxConvStr, eqFwdStr, divAmtStr, convValStr, convPrcStr;

            Real eqFwd = model_->equity()->equitySpot()->value() /
                         model_->equity()->equityForecastCurve()->discount(grid[i]) *
                         model_->equity()->equityDividendCurve()->discount(grid[i]);
            Real divAmt = 0.0;

            if (events.getAssociatedDate(i) != Null<Date>()) {
                dateStr << QuantLib::io::iso_date(events.getAssociatedDate(i));
            }
            if (events.hasBondCashflow(i)) {
                bondFlowStr << events.getBondCashflow(i) + events.getBondFinalRedemption(i);
            }
            if (events.hasCall(i)) {
                const auto& cd = events.getCallData(i);
                callStr << "@" << cd.price;
                if (cd.isSoft)
                    callStr << " s@" << cd.softTriggerRatio;
            }
            if (events.hasPut(i)) {
                const auto& cd = events.getPutData(i);
                putStr << "@" << cd.price;
            }
            if (events.hasConversion(i)) {
                convStr << "@" << events.getCurrentConversionRatio(i);
                if (events.hasContingentConversion(i)) {
                    const auto& cd = events.getConversionData(i);
                    convStr << " c@" << cd.cocoBarrier;
                    if (events.hasNoConversionPlane(i)) {
                        convStr << "b";
                    }
                }
            }
            if (events.hasMandatoryConversion(i)) {
                const auto& cd = events.getMandatoryConversionData(i);
                convStr << "peps(" << cd.pepsLowerConversionRatio << "/" << cd.pepsUpperConversionRatio << ")";
            }
            if (events.hasConversionReset(i)) {
                const auto& cd = events.getConversionResetData(i);
                if (cd.resetToSpecificValue) {
                    convResetStr << "->" << cd.newCr << " ";
                }
                if (cd.resetActive) {
                    convResetStr << cd.gearing << "@" << cd.threshold;
                    if (cd.reference == ConvertibleBond2::ConversionResetData::ReferenceType::CurrentCP)
                        convResetStr << "/CPt ";
                    else
                        convResetStr << "/CP0 ";
                }
                if (cd.divProtActive) {
                    convResetStr << "DP(" << cd.lastDividendProtectionTimeIndex << "/"
                                 << model_->dividendYield(grid[cd.lastDividendProtectionTimeIndex], grid[i]) << ")@"
                                 << cd.divThreshold;
                    Real s = grid[cd.lastDividendProtectionTimeIndex + 1];
                    Real t = grid[i];
                    divAmt +=
                        cd.accruedHistoricalDividends + (std::exp(model_->dividendYield(s, t) * (t - s)) - 1.0) * eqFwd;
                }
            }
            if (events.hasDividendPassThrough(i)) {
                const auto& cd = events.getDividendPassThroughData(i);
                divStr << "@" << cd.divThreshold;
                Real s = grid[cd.lastDividendProtectionTimeIndex + 1];
                Real t = grid[i];
                divAmt +=
                    cd.accruedHistoricalDividends + (std::exp(model_->dividendYield(s, t) * (t - s)) - 1.0) * eqFwd;
            }
            if (events.getCurrentConversionRatio(i) == Null<Real>()) {
                currentConvStr << "NA";
            } else {
                currentConvStr << events.getCurrentConversionRatio(i);
            }
            if (events.hasStochasticConversionRatio(i)) {
                currentConvStr << "s";
            }
            fxConvStr << events.getCurrentFxConversion(i);
            eqFwdStr << eqFwd;
            if (!close_enough(divAmt, 0.0))
                divAmtStr << divAmt;
            convValStr << events.getCurrentConversionRatio(i) * eqFwd;
            convPrcStr << N0 / (events.getCurrentConversionRatio(i) * events.getCurrentFxConversion(i));

            std::ostringstream eventDescription;
            eventDescription << std::left << "|" << std::setw(width) << grid[i] << "|" << std::setw(width)
                             << dateStr.str() << "|" << std::setw(width) << notional(grid[i]) << "|" << std::setw(width)
                             << accrual(grid[i]) << "|" << std::setw(width) << bondFlowStr.str() << "|"
                             << std::setw(width) << callStr.str() << "|" << std::setw(width) << putStr.str() << "|"
                             << std::setw(2 * width) << convStr.str() << "|" << std::setw(2 * width)
                             << convResetStr.str() << "|" << std::setw(width) << divStr.str() << "|" << std::setw(width)
                             << currentConvStr.str() << "|" << std::setw(width) << fxConvStr.str() << "|"
                             << std::setw(width) << eqFwdStr.str() << "|" << std::setw(width) << divAmtStr.str() << "|"
                             << std::setw(width) << convValStr.str() << "|" << std::setw(width) << convPrcStr.str()
                             << "|";
            std::string label = "0000" + std::to_string(counter++);
            results_.additionalResults["event_" + label.substr(label.size() - 5)] = eventDescription.str();
        }
        // do not log more than 100k events, unlikely that this is ever necessary
        if (counter >= 100000)
            break;
    }

    // 14.2 more additional results

    results_.additionalResults.insert(events.additionalResults().begin(), events.additionalResults().end());

    Real tMax = grid[grid.size() - 1];
    results_.additionalResults["trade.tMax"] = tMax;

    Real eqFwd = model_->equity()->equitySpot()->value() / model_->equity()->equityForecastCurve()->discount(tMax) *
                 model_->equity()->equityDividendCurve()->discount(tMax);
    results_.additionalResults["market.discountRate(tMax)"] =
        discountingCurve_.empty() ? model_->equity()->equityForecastCurve()->zeroRate(tMax, Continuous).rate()
                                  : discountingCurve_->zeroRate(tMax, Continuous).rate();
    results_.additionalResults["market.discountingSpread"] =
        discountingSpread_.empty() ? 0.0 : discountingSpread_->value();
    results_.additionalResults["market.creditSpread(tMax)"] =
        -std::log(model_->creditCurve()->survivalProbability(tMax)) / tMax;
    if (!creditCurve_.empty()) {
        results_.additionalResults["market.exchangeableBondSpread(tMax)"] =
            -std::log(creditCurve_->survivalProbability(tMax)) / tMax;
    }
    results_.additionalResults["market.recoveryRate"] = recoveryRate_.empty() ? 0.0 : recoveryRate_->value();
    results_.additionalResults["market.equitySpot"] = model_->equity()->equitySpot()->value();
    results_.additionalResults["market.equityForward(tMax)"] = eqFwd;
    results_.additionalResults["market.equityVolaility(tMax)"] =
        std::sqrt(model_->totalBlackVariance() / model_->stepTimes().back());

    results_.additionalResults["model.fdGridSize"] = grid.size();
    results_.additionalResults["model.eta"] = model_->eta();
    results_.additionalResults["model.p"] = model_->p();
    results_.additionalResults["model.calibrationTimes"] = model_->stepTimes();
    results_.additionalResults["model.h0"] = model_->h0();
    results_.additionalResults["model.sigma"] = model_->sigma();

    if (!conversionIndicator.empty()) {
        MonotonicCubicNaturalSpline interpolationConversionIndicator(
            mesher_->locations().begin(), mesher_->locations().end(), conversionIndicator[0].begin());
        results_.additionalResults["conversionIndicator"] = interpolationConversionIndicator(logSpot);
    }
}

} // namespace QuantExt
