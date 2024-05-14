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
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.*/

#include <qle/pricingengines/fdconvertiblebondevents.hpp>

#include <ql/cashflows/coupon.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/time/calendars/jointcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/timegrid.hpp>

namespace QuantExt {

namespace {
std::string getDateStr(const Date& d) {
    std::ostringstream os;
    os << QuantLib::io::iso_date(d);
    return os.str();
}
} // namespace

FdConvertibleBondEvents::FdConvertibleBondEvents(const Date& today, const DayCounter& dc, const Real N0,
                                                 const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equity,
                                                 const QuantLib::ext::shared_ptr<FxIndex>& fxConversion)
    : today_(today), dc_(dc), N0_(N0), equity_(equity), fxConversion_(fxConversion) {}

Real FdConvertibleBondEvents::time(const Date& d) const { return dc_.yearFraction(today_, d); }

void FdConvertibleBondEvents::registerBondCashflow(const QuantLib::ext::shared_ptr<CashFlow>& c) {
    if (c->date() > today_) {
        registeredBondCashflows_.push_back(c);
        times_.insert(time(c->date()));
    }
}

void FdConvertibleBondEvents::registerCall(const ConvertibleBond2::CallabilityData& c) {
    registeredCallData_.push_back(c);
    if (c.exerciseDate > today_) {
        times_.insert(time(c.exerciseDate));
    }
}

void FdConvertibleBondEvents::registerMakeWhole(const ConvertibleBond2::MakeWholeData& d) {
    registeredMakeWholeData_ = d;
}

void FdConvertibleBondEvents::registerPut(const ConvertibleBond2::CallabilityData& c) {
    registeredPutData_.push_back(c);
    if (c.exerciseDate > today_) {
        times_.insert(time(c.exerciseDate));
    }
}

void FdConvertibleBondEvents::registerConversionRatio(const ConvertibleBond2::ConversionRatioData& c) {
    registeredConversionRatioData_.push_back(c);
    if (c.fromDate > today_) {
        times_.insert(time(c.fromDate));
    }
}

void FdConvertibleBondEvents::registerConversion(const ConvertibleBond2::ConversionData& c) {
    registeredConversionData_.push_back(c);
    if (c.exerciseDate > today_) {
        times_.insert(time(c.exerciseDate));
    }
}

void FdConvertibleBondEvents::registerMandatoryConversion(const ConvertibleBond2::MandatoryConversionData& c) {
    registeredMandatoryConversionData_.push_back(c);
    if (c.exerciseDate > today_) {
        times_.insert(time(c.exerciseDate));
    }
}

void FdConvertibleBondEvents::registerConversionReset(const ConvertibleBond2::ConversionResetData& c) {
    registeredConversionResetData_.push_back(c);
    if (c.resetDate > today_) {
        times_.insert(time(c.resetDate));
    }
}

void FdConvertibleBondEvents::registerDividendProtection(const ConvertibleBond2::DividendProtectionData& c) {
    registeredDividendProtectionData_.push_back(c);
    if (c.protectionDate > today_) {
        times_.insert(time(c.protectionDate));
    }
}

const std::set<Real>& FdConvertibleBondEvents::times() const { return times_; }

Date FdConvertibleBondEvents::nextExerciseDate(const Date& d,
                                               const std::vector<ConvertibleBond2::CallabilityData>& data) const {
    Date result = Date::maxDate();
    for (auto const& x : data) {
        if (x.exerciseDate > d)
            result = std::min(result, x.exerciseDate);
    }
    if (result == Date::maxDate())
        return Null<Date>();
    return result;
}

Date FdConvertibleBondEvents::nextConversionDate(const Date& d) const {
    Date result = Date::maxDate();
    for (auto const& x : registeredConversionData_) {
        if (x.exerciseDate > d)
            result = std::min(result, x.exerciseDate);
    }
    if (result == Date::maxDate())
        return Null<Date>();
    return result;
}

void FdConvertibleBondEvents::processBondCashflows() {
    lastRedemptionDate_ = Date::minDate();
    for (auto const& c : registeredBondCashflows_) {
        if (QuantLib::ext::dynamic_pointer_cast<Coupon>(c) == nullptr)
            lastRedemptionDate_ = std::max(lastRedemptionDate_, c->date());
    }
    for (auto const& d : registeredBondCashflows_) {
        bool isRedemption = QuantLib::ext::dynamic_pointer_cast<Coupon>(d) == nullptr;
        Size index = grid_.index(time(d->date()));
        hasBondCashflow_[index] = true;
        associatedDate_[index] = d->date();
        if (isRedemption && d->date() == lastRedemptionDate_)
            bondFinalRedemption_[index] += d->amount();
        else
            bondCashflow_[index] += d->amount();
    }
}

void FdConvertibleBondEvents::processExerciseData(const std::vector<ConvertibleBond2::CallabilityData>& sourceData,
                                                  std::vector<bool>& targetFlags, std::vector<CallData>& targetData) {
    for (auto const& c : sourceData) {
        if (c.exerciseDate <= today_ && c.exerciseType == ConvertibleBond2::CallabilityData::ExerciseType::OnThisDate)
            continue;
        Size indexStart = grid_.index(time(std::max(c.exerciseDate, today_)));
        Size indexEnd;
        associatedDate_[indexStart] = std::max(c.exerciseDate, today_);
        if (c.exerciseType == ConvertibleBond2::CallabilityData::ExerciseType::OnThisDate) {
            indexEnd = indexStart;
        } else if (c.exerciseType == ConvertibleBond2::CallabilityData::ExerciseType::FromThisDateOn) {
            Date nextDate = nextExerciseDate(c.exerciseDate, sourceData);
            QL_REQUIRE(nextDate != Null<Date>(),
                       "FdConvertibleBondEvents::processExerciseData(): internal error: did not find a next exercise "
                       "date after "
                           << c.exerciseDate
                           << ", the last exercise date should not have exercise type FromThisDateOn");
            if (nextDate <= today_)
                continue;
            indexEnd = grid_.index(time(nextDate)) - 1;
        } else {
            QL_FAIL("FdConvertibleBondEvents: internal error, exercise type not "
                    "recognized");
        }
        for (Size i = indexStart; i <= indexEnd; ++i) {
            targetFlags[i] = true;
            targetData[i] = CallData{c.price,  c.priceType,        c.includeAccrual,
                                     c.isSoft, c.softTriggerRatio, std::function<Real(Real, Real)>()};
        }
    }
}

void FdConvertibleBondEvents::processMakeWholeData() {
    if (registeredMakeWholeData_.crIncreaseData) {

        // init and checks

        auto const& stockPrices = (*registeredMakeWholeData_.crIncreaseData).stockPrices;
        auto const& effDates = (*registeredMakeWholeData_.crIncreaseData).effectiveDates;
        auto const& crInc = (*registeredMakeWholeData_.crIncreaseData).crIncrease;
        QL_REQUIRE(stockPrices.size() >= 2,
                   "FdConvertibleBondEvents::processMakeWholeData(): at least two stock prices required (cr increase)");
        QL_REQUIRE(
            effDates.size() >= 2,
            "FdConvertibleBondEvents::processMakeWholeData(): at least two effective dates required (cr increase)");
        QL_REQUIRE(effDates.size() == crInc.size(), "FdConvertibleBondEvents::processMakeWholeData(): effective dates ("
                                                        << effDates.size() << ") must match cr increase rows ("
                                                        << crInc.size() << ") (cr increase)");
        for (auto const& c : crInc) {
            QL_REQUIRE(c.size() == stockPrices.size(),
                       "FdConvertibleBondEvents::processMakeWholeData(): stock prices size ("
                           << stockPrices.size() << ") must match cr increase coluns (" << c.size() << ")");
        }

        // build interpolation

        mw_cr_inc_x_ = Array(stockPrices.begin(), stockPrices.end());
        mw_cr_inc_y_ = Array(effDates.size());
        mw_cr_inc_z_ = Matrix(mw_cr_inc_y_.size(), mw_cr_inc_x_.size());

        for (Size i = 0; i < effDates.size(); ++i) {
            mw_cr_inc_y_[i] = time(effDates[i]);
            for (Size j = 0; j < stockPrices.size(); ++j) {
                mw_cr_inc_z_(i, j) = crInc[i][j];
            }
        }

        auto interpolation = QuantLib::ext::make_shared<BilinearInterpolation>(
            mw_cr_inc_x_.begin(), mw_cr_inc_x_.end(), mw_cr_inc_y_.begin(), mw_cr_inc_y_.end(), mw_cr_inc_z_);

        // init cap (infty if not given)

        Real cap = QL_MAX_REAL;
        if ((*registeredMakeWholeData_.crIncreaseData).cap != Null<Real>())
            cap = (*registeredMakeWholeData_.crIncreaseData).cap;

        // set effetive mw cr functions on the call data

        for (Size i = 0; i < grid_.size(); ++i) {
            if (hasCall_[i]) {
                Real t = grid_[i];
                callData_[i].mwCr = [interpolation, t, cap](const Real S, const Real cr) {
                    if ((S < interpolation->xMin() && !close_enough(S, interpolation->xMin())) ||
                        (S > interpolation->xMax() && !close_enough(S, interpolation->xMax())) ||
                        (t < interpolation->yMin() && !close_enough(t, interpolation->yMin())) ||
                        (t > interpolation->yMax() && !close_enough(t, interpolation->yMax()))) {
                        return cr;
                    } else {
                        Real tmp = interpolation->operator()(S, t);
                        // apply cap, but if initial cr was already greater than result, keep that
                        return std::max(cr, std::min(cr + tmp, cap));
                    }
                };
            }
        }
    }
}

void FdConvertibleBondEvents::processConversionAndDivProtData() {

    // set the initial conversion ratio

    std::set<std::pair<Date, Real>> crSchedule;
    for (const auto& d : registeredConversionRatioData_) {
        crSchedule.insert(std::make_pair(d.fromDate, d.conversionRatio));
    }
    if (!crSchedule.empty()) {
        initialConversionRatio_ = crSchedule.begin()->second;
    }
    std::fill(currentConversionRatio_.begin(), currentConversionRatio_.end(), initialConversionRatio_);
    additionalResults_["historicEvents.initialConversionRatio"] = initialConversionRatio_;

    // collect all relevant conversion events
    // - cd: cr reset event
    // - dd: dp event (with cr adjustment or pass-through)
    // - vd: voluntary conversion (with coco possibly)
    // - newCr: cr changed (or initially set to) specific value

    struct AdjEvent {
        ConvertibleBond2::ConversionResetData* cd = nullptr;
        ConvertibleBond2::DividendProtectionData* dd = nullptr;
        ConvertibleBond2::ConversionData* vd = nullptr;
        Real newCr = Null<Real>(); // set cr to specific value on event date
    };

    std::map<Date, AdjEvent> adjEvents;

    for (auto& d : registeredConversionResetData_) {
        adjEvents[d.resetDate].cd = &d;
    }

    for (auto& d : registeredDividendProtectionData_) {
        adjEvents[d.protectionDate].dd = &d;
    }

    for (auto& d : registeredConversionRatioData_) {
        adjEvents[d.fromDate].newCr = d.conversionRatio;
    }

    for (auto& d : registeredConversionData_) {
        adjEvents[d.exerciseDate].vd = &d;
    }

    // step through the events and process them (historical and future events)

    Real currentCr = initialConversionRatio_;
    Size lastDividendProtectionTimeIndex = Null<Size>();
    bool haveStochasticCr = false;

    for (auto const& event : adjEvents) {

        if (event.second.cd != nullptr) {
            const ConvertibleBond2::ConversionResetData& c = *event.second.cd;
            if (c.resetDate <= today_) {

                // historical conversion ratio reset event

                Real S = equity_->fixing(equity_->fixingCalendar().adjust(c.resetDate, Preceding));
                Real fx = 1.0;
                if (fxConversion_ != nullptr) {
                    fx = fxConversion_->fixing(JointCalendar(equity_->fixingCalendar(), fxConversion_->fixingCalendar())
                                                   .adjust(c.resetDate, Preceding));
                }
                Real cr = c.referenceType == ConvertibleBond2::ConversionResetData::ReferenceType::CurrentCP
                              ? currentCr
                              : initialConversionRatio_;
                Real referenceCP = N0_ / cr;
                additionalResults_["historicEvents.crReset_" + getDateStr(event.first) + "_S"] = S;
                additionalResults_["historicEvents.crReset_" + getDateStr(event.first) + "_threshold"] = c.threshold;
                additionalResults_["historicEvents.crReset_" + getDateStr(event.first) + "_referenceCP"] = referenceCP;
                additionalResults_["historicEvents.crReset_" + getDateStr(event.first) + "_gearing"] = c.gearing;
                additionalResults_["historicEvents.crReset_" + getDateStr(event.first) + "_floor"] = c.floor;
                additionalResults_["historicEvents.crReset_" + getDateStr(event.first) + "_globalFloor"] =
                    c.globalFloor * fx;
                additionalResults_["historicEvents.crReset_" + getDateStr(event.first) + "_fxConversion"] = fx;
                additionalResults_["historicEvents.crReset_" + getDateStr(event.first) + "_currentCr"] = currentCr;
                if (!close_enough(cr, 0.0) && S < c.threshold * referenceCP) {
                    Real adjustedConversionRatio = QL_MAX_REAL;
                    if (!close_enough(c.gearing, 0.0)) {
                        adjustedConversionRatio = std::min(adjustedConversionRatio, N0_ / (c.gearing * S));
                    }
                    if (!close_enough(c.floor, 0.0)) {
                        adjustedConversionRatio = std::min(adjustedConversionRatio, N0_ / (c.floor * referenceCP));
                    }
                    if (!close_enough(c.globalFloor * fx, 0.0)) {
                        adjustedConversionRatio = std::min(adjustedConversionRatio, N0_ / (c.globalFloor * fx));
                    }
                    adjustedConversionRatio = std::max(
                        currentCr, adjustedConversionRatio != QL_MAX_REAL ? adjustedConversionRatio : -QL_MAX_REAL);
                    currentCr = std::max(currentCr, adjustedConversionRatio);
                }
                additionalResults_["historicEvents.crReset_" + getDateStr(event.first) + "_adjustedCr"] = currentCr;
            } else {

                // future conversion ratio reset event

                Size index = grid_.index(time(event.first));
                associatedDate_[index] = event.first;
                hasConversionReset_[index] = true;
                const ConvertibleBond2::ConversionResetData& c = *event.second.cd;
                conversionResetData_[index].resetActive = true;
                conversionResetData_[index].reference = c.referenceType;
                conversionResetData_[index].gearing = c.gearing;
                conversionResetData_[index].floor = c.floor;
                conversionResetData_[index].globalFloor = c.globalFloor * currentFxConversion_[index];
                conversionResetData_[index].threshold = c.threshold;
                for (Size i = index + 1; i < grid_.size(); ++i) {
                    stochasticConversionRatio_[i] = true;
                }
                haveStochasticCr = true;
            }
        }

        if (event.second.dd != nullptr) {
            const ConvertibleBond2::DividendProtectionData& c = *event.second.dd;
            if (c.protectionDate <= today_) {
                if (c.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly ||
                    c.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpDown ||
                    c.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly2 ||
                    c.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpDown2) {

                    // historic dividend protection event with conversion ratio adjustment

                    Real fx = 1.0;
                    if (fxConversion_ != nullptr) {
                        fx = fxConversion_->fixing(
                            JointCalendar(equity_->fixingCalendar(), fxConversion_->fixingCalendar())
                                .adjust(c.protectionDate, Preceding));
                    }
                    Real S = equity_->fixing(equity_->fixingCalendar().adjust(c.protectionDate, Preceding));
                    Real D = equity_->dividendsBetweenDates(c.startDate, c.protectionDate);
                    Real H = c.threshold * fx;

                    additionalResults_["historicEvents.crReset_DP_" + getDateStr(event.first) + "_div_" +
                                       getDateStr(c.startDate) + "_" + getDateStr(c.protectionDate)] = D;
                    additionalResults_["historicEvents.crReset_DP_" + getDateStr(event.first) + "_S"] = S;
                    additionalResults_["historicEvents.crReset_DP_" + getDateStr(event.first) + "_threshold"] = H;
                    additionalResults_["historicEvents.crReset_DP_" + getDateStr(event.first) + "_fxConversion"] = fx;
                    additionalResults_["historicEvents.crReset_DP_" + getDateStr(event.first) + "_currentCr"] =
                        currentCr;

                    if (c.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly ||
                        c.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpDown) {
                        bool absolute =
                            c.dividendType == ConvertibleBond2::DividendProtectionData::DividendType::Absolute;
                        Real d = absolute ? D : D / S;
                        Real C =
                            c.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly
                                ? std::max(d - H, 0.0)
                                : d - H;
                        currentCr *= absolute ? S / std::max(S - C, 1E-4) : (1.0 + C);
                    } else {
                        Real f = std::max(S - H, 0.0) / std::max(S - D, 1E-4);
                        if (c.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly2)
                            f = std::max(f, 1.0);
                        currentCr *= f;
                    }

                    additionalResults_["historicEvents.crReset_DP_" + getDateStr(event.first) + "_adjustedCr"] =
                        currentCr;
                }
            } else {

                if (c.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly ||
                    c.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpDown ||
                    c.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpOnly2 ||
                    c.adjustmentStyle == ConvertibleBond2::DividendProtectionData::AdjustmentStyle::CrUpDown2) {

                    // future dividend protection event with conversion ratio adjustment

                    Size index = grid_.index(time(event.first));
                    associatedDate_[index] = event.first;
                    hasConversionReset_[index] = true;
                    const ConvertibleBond2::DividendProtectionData& c = *event.second.dd;
                    conversionResetData_[index].divProtActive = true;
                    conversionResetData_[index].adjustmentStyle = c.adjustmentStyle;
                    conversionResetData_[index].dividendType = c.dividendType;
                    conversionResetData_[index].divThreshold = c.threshold * currentFxConversion_[index];
                    if (lastDividendProtectionTimeIndex == Null<Size>()) {
                        conversionResetData_[index].lastDividendProtectionTimeIndex = 0;
                        conversionResetData_[index].accruedHistoricalDividends =
                            equity_->dividendsBetweenDates(c.startDate, today_);
                        additionalResults_["historicEvents.accruedDividends_" + getDateStr(c.startDate) + "_" +
                                           getDateStr(today_)] = conversionResetData_[index].accruedHistoricalDividends;
                    } else {
                        conversionResetData_[index].lastDividendProtectionTimeIndex = lastDividendProtectionTimeIndex;
                        conversionResetData_[index].accruedHistoricalDividends = 0.0;
                    }
                    lastDividendProtectionTimeIndex = index;
                    for (Size i = index + 1; i < grid_.size(); ++i) {
                        stochasticConversionRatio_[i] = true;
                    }
                    haveStochasticCr = true;

                } else {

                    // future dividend pass through event
                    Size index = grid_.index(time(c.protectionDate));
                    associatedDate_[index] = c.protectionDate;
                    hasDividendPassThrough_[index] = true;
                    dividendPassThroughData_[index].adjustmentStyle = c.adjustmentStyle;
                    dividendPassThroughData_[index].dividendType = c.dividendType;
                    dividendPassThroughData_[index].divThreshold = c.threshold;
                    if (lastDividendProtectionTimeIndex == Null<Size>()) {
                        dividendPassThroughData_[index].lastDividendProtectionTimeIndex = 0;
                        dividendPassThroughData_[index].accruedHistoricalDividends =
                            equity_->dividendsBetweenDates(c.startDate, today_);
                        additionalResults_["historicEvents.accruedDividends_" + getDateStr(c.startDate) + "_" +
                                           getDateStr(today_)] =
                            dividendPassThroughData_[index].accruedHistoricalDividends;
                    } else {
                        dividendPassThroughData_[index].lastDividendProtectionTimeIndex =
                            lastDividendProtectionTimeIndex;
                        dividendPassThroughData_[index].accruedHistoricalDividends = 0.0;
                    }
                    lastDividendProtectionTimeIndex = index;
                }
            }
        }

        if (event.second.vd != nullptr) {

            // voluntary conversion data (possibly with coco)

            const ConvertibleBond2::ConversionData& vd = *event.second.vd;

            Date nextConvDate = nextConversionDate(vd.exerciseDate);
            if (nextConvDate > today_) {
                bool conversionIsProhibited = false;
                Size indexStart = grid_.index(time(std::max(vd.exerciseDate, today_)));
                Size indexEnd;
                associatedDate_[indexStart] = std::max(vd.exerciseDate, today_);
                if (vd.exerciseType == ConvertibleBond2::ConversionData::ExerciseType::OnThisDate) {
                    indexEnd = indexStart;
                } else if (vd.exerciseType == ConvertibleBond2::ConversionData::ExerciseType::FromThisDateOn) {
                    QL_REQUIRE(nextConvDate != Null<Date>(),
                               "FdConvertibleBondEvents::processConversionData(): internal error: did not find a next "
                               "conversion "
                               "date after "
                                   << vd.exerciseDate
                                   << ", the last conversione date should not have exercise type FromThisDateOn");
                    indexEnd = grid_.index(time(nextConvDate));
                    // check whether conversion is prohibited for the future due to past coco condition check
                    if (vd.cocoType == ConvertibleBond2::ConversionData::CocoType::StartOfPeriod &&
                        vd.exerciseDate <= today_) {
                        Real S = equity_->fixing(equity_->fixingCalendar().adjust(vd.exerciseDate, Preceding));
                        conversionIsProhibited = S * currentCr <= vd.cocoBarrier;
                        additionalResults_["historicEvents.coco_" + getDateStr(vd.exerciseDate) + "_S"] = S;
                        additionalResults_["historicEvents.coco_" + getDateStr(vd.exerciseDate) + "_currentCr"] =
                            currentCr;
                        additionalResults_["historicEvents.coco_" + getDateStr(vd.exerciseDate) + "_cocoBarrier"] =
                            vd.cocoBarrier;
                        additionalResults_["historicEvents.coco_" + getDateStr(vd.exerciseDate) + "_triggered"] =
                            !conversionIsProhibited;
                    }
                } else {
                    QL_FAIL("FdConvertibleBondEvents: internal error, exercise type not recognized");
                }
                // update the grid info
                for (Size i = indexStart; i <= indexEnd; ++i) {
                    // if we have set conversion information already and the current is the last conversion
                    // date, we do not modify the exisiting information, because then the current date is the
                    // end date of the last american period.
                    if (nextConvDate == Null<Date>() && hasConversionInfoSet_[i])
                        continue;
                    // otherwise mark the index as set
                    hasConversionInfoSet_[i] = true;
                    // if the conversion is not allowed, do not set the hasConversion flag
                    if (conversionIsProhibited)
                        continue;
                    // if the conversion is allowed, set the conversion flag ...
                    hasConversion_[i] = true;
                    // ... and the conversion data
                    conversionData_[i] = ConversionData{vd.cocoBarrier};
                    // a start of period coco check should only be done, if the date is > today, otherwise it's
                    // done already
                    if (vd.cocoType == ConvertibleBond2::ConversionData::CocoType::Spot) {
                        hasContingentConversion_[i] = true;
                    } else if (vd.exerciseDate > today_ &&
                               vd.cocoType == ConvertibleBond2::ConversionData::CocoType::StartOfPeriod) {
                        hasContingentConversion_[i] = true;
                        hasNoConversionPlane_[i] = i > indexStart;
                    }
                }
            }
        }

        if (event.second.newCr != Null<Real>()) {
            // update current cr
            currentCr = event.second.newCr;
            if (event.first >= today_) {
                associatedDate_[grid_.index(time(event.first))] = event.first;
                if (haveStochasticCr) {
                    // we know event.first >= today if haveStochasticCr is true
                    Size index = grid_.index(time(event.first));
                    hasConversionReset_[index] = true;
                    conversionResetData_[index].resetToSpecificValue = true;
                    conversionResetData_[index].newCr = currentCr;
                }
            }
        }

        // update current cr on grid

        for (Size i = grid_.index(time(std::max(event.first, today_))); i < grid_.size(); ++i) {
            currentConversionRatio_[i] = currentCr;
        }

    } // for adjEvents

} // processConversionData()

void FdConvertibleBondEvents::processMandatoryConversionData() {
    for (auto const& d : registeredMandatoryConversionData_) {
        if (d.exerciseDate <= today_)
            continue;
        Size index = grid_.index(time(d.exerciseDate));
        associatedDate_[index] = d.exerciseDate;
        hasMandatoryConversion_[index] = true;
        mandatoryConversionData_[index] = {d.pepsUpperBarrier * currentFxConversion_[index],
                                           d.pepsLowerBarrier * currentFxConversion_[index], d.pepsUpperConversionRatio,
                                           d.pepsLowerConversionRatio};
    }
}

void FdConvertibleBondEvents::finalise(const TimeGrid& grid) {
    QL_REQUIRE(!finalised_, "FdConvertibleBondEvents: internal error, events already finalised");
    finalised_ = true;
    grid_ = grid;

    hasBondCashflow_.resize(grid.size(), false);
    hasCall_.resize(grid.size(), false);
    hasPut_.resize(grid.size(), false);
    hasConversion_.resize(grid.size(), false);
    hasMandatoryConversion_.resize(grid.size(), false);
    hasContingentConversion_.resize(grid.size(), false);
    hasConversionInfoSet_.resize(grid.size(), false);
    hasNoConversionPlane_.resize(grid.size(), false);
    hasConversionReset_.resize(grid.size(), false);
    hasDividendPassThrough_.resize(grid.size(), false);

    bondCashflow_.resize(grid.size(), 0.0);
    bondFinalRedemption_.resize(grid.size(), 0.0);
    callData_.resize(grid.size());
    putData_.resize(grid.size());
    conversionData_.resize(grid.size());
    conversionResetData_.resize(grid.size());
    dividendPassThroughData_.resize(grid.size());
    mandatoryConversionData_.resize(grid.size());

    stochasticConversionRatio_.resize(grid.size(), false);
    currentConversionRatio_.resize(grid.size(), 0.0);
    currentFxConversion_.resize(grid.size(), 1.0);
    associatedDate_.resize(grid.size(), Null<Date>());

    initialConversionRatio_ = 0.0;

    // fill fx conversion rate on grid

    if (fxConversion_ != nullptr) {
        auto source = fxConversion_->sourceCurve();
        auto target = fxConversion_->targetCurve();
        Real spot = fxConversion_->fixing(today_);
        for (Size i = 0; i < grid_.size(); ++i) {
            Real fx = spot * source->discount(grid_[i]) / target->discount(grid_[i]);
            currentFxConversion_[i] = fx;
        }
    }

    // process data

    processBondCashflows();
    processExerciseData(registeredCallData_, hasCall_, callData_);
    processExerciseData(registeredPutData_, hasPut_, putData_);
    processMakeWholeData();
    processConversionAndDivProtData();
    processMandatoryConversionData();

    // checks

    Size lastRedemptionIndex = grid_.index(time(lastRedemptionDate_));
    for (Size k = lastRedemptionIndex + 1; k < grid_.size(); ++k) {
        QL_REQUIRE(!hasConversion(k) && !hasMandatoryConversion(k),
                   "FdConvertibleBondEvents: conversion right after last bond redemption flow not allowed");
    }
}

bool FdConvertibleBondEvents::hasBondCashflow(const Size i) const { return hasBondCashflow_.at(i); }
bool FdConvertibleBondEvents::hasCall(const Size i) const { return hasCall_.at(i); }
bool FdConvertibleBondEvents::hasPut(const Size i) const { return hasPut_.at(i); }
bool FdConvertibleBondEvents::hasConversion(const Size i) const { return hasConversion_.at(i); }
bool FdConvertibleBondEvents::hasMandatoryConversion(const Size i) const { return hasMandatoryConversion_.at(i); }
bool FdConvertibleBondEvents::hasContingentConversion(const Size i) const { return hasContingentConversion_.at(i); }
bool FdConvertibleBondEvents::hasNoConversionPlane(const Size i) const { return hasNoConversionPlane_.at(i); }
bool FdConvertibleBondEvents::hasConversionReset(const Size i) const { return hasConversionReset_.at(i); }
bool FdConvertibleBondEvents::hasDividendPassThrough(const Size i) const { return hasDividendPassThrough_.at(i); }

Real FdConvertibleBondEvents::getBondCashflow(const Size i) const { return bondCashflow_.at(i); }
Real FdConvertibleBondEvents::getBondFinalRedemption(const Size i) const { return bondFinalRedemption_.at(i); }

const FdConvertibleBondEvents::CallData& FdConvertibleBondEvents::getCallData(const Size i) const {
    return callData_.at(i);
}

const FdConvertibleBondEvents::CallData& FdConvertibleBondEvents::getPutData(const Size i) const {
    return putData_.at(i);
}

const FdConvertibleBondEvents::ConversionData& FdConvertibleBondEvents::getConversionData(const Size i) const {
    return conversionData_.at(i);
}

const FdConvertibleBondEvents::MandatoryConversionData&
FdConvertibleBondEvents::getMandatoryConversionData(const Size i) const {
    return mandatoryConversionData_.at(i);
}

const FdConvertibleBondEvents::ConversionResetData&
FdConvertibleBondEvents::getConversionResetData(const Size i) const {
    return conversionResetData_.at(i);
}

const FdConvertibleBondEvents::DividendPassThroughData&
FdConvertibleBondEvents::getDividendPassThroughData(const Size i) const {
    return dividendPassThroughData_.at(i);
}

bool FdConvertibleBondEvents::hasStochasticConversionRatio(const Size i) const {
    return stochasticConversionRatio_.at(i);
}

Real FdConvertibleBondEvents::getCurrentConversionRatio(const Size i) const { return currentConversionRatio_.at(i); }
Real FdConvertibleBondEvents::getCurrentFxConversion(const Size i) const { return currentFxConversion_.at(i); }
Real FdConvertibleBondEvents::getInitialConversionRatio() const { return initialConversionRatio_; }
Date FdConvertibleBondEvents::getAssociatedDate(const Size i) const { return associatedDate_.at(i); }

} // namespace QuantExt
