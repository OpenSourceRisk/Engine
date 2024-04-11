/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ored/portfolio/optionwrapper.hpp>
#include <ql/option.hpp>
#include <ql/settings.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

OptionWrapper::OptionWrapper(const QuantLib::ext::shared_ptr<Instrument>& inst, const bool isLongOption,
                             const std::vector<Date>& exerciseDate, const bool isPhysicalDelivery,
                             const std::vector<QuantLib::ext::shared_ptr<Instrument>>& undInst, const Real multiplier,
                             const Real undMultiplier,
                             const std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>& additionalInstruments,
                             const std::vector<Real>& additionalMultipliers)
    : InstrumentWrapper(inst, multiplier, additionalInstruments, additionalMultipliers), isLong_(isLongOption),
      isPhysicalDelivery_(isPhysicalDelivery), contractExerciseDates_(exerciseDate),
      effectiveExerciseDates_(exerciseDate), underlyingInstruments_(undInst),
      activeUnderlyingInstrument_(undInst.at(0)), undMultiplier_(undMultiplier), exercised_(false), exercisable_(true),
      exerciseDate_(Date()) {
    QL_REQUIRE(exerciseDate.size() == undInst.size(), "number of exercise dates ("
                                                          << exerciseDate.size()
                                                          << ") must be equal to underlying instrument vector size ("
                                                          << undInst.size() << ")");
}

void OptionWrapper::initialise(const vector<Date>& dateGrid) {
    // set "effective" exercise dates which get used to determine exercise during cube valuation
    // this is necessary since there is no guarantee that actual exercise dates are included in
    // the valuation date grid
    Date today = Settings::instance().evaluationDate();
    for (Size i = 0; i < contractExerciseDates_.size(); ++i) {
        effectiveExerciseDates_[i] = Null<Date>();
        if (contractExerciseDates_[i] > today && contractExerciseDates_[i] <= dateGrid.back()) {
            // Find the effective exercise date in our grid. We simulate the exercise just after the actual
            // exercise.This ensures that the QL instrument's NPV is a proper continuation value, i.e.
            // does not contain the possibility of exercising in to the underlying on the current exercise
            // date and can  therefore it be used as such in the exercise decision made in the exercise()
            // method of derived classes.
            auto it = std::lower_bound(dateGrid.begin(), dateGrid.end(), contractExerciseDates_[i]);
            effectiveExerciseDates_[i] = *it;
        }
    }
}

void OptionWrapper::reset() {
    exercised_ = false;
    exerciseDate_ = Date();
}

Real OptionWrapper::NPV() const {
    Real addNPV = additionalInstrumentsNPV();

    Date today = Settings::instance().evaluationDate();
    if (!exercised_) {
        for (Size i = 0; i < effectiveExerciseDates_.size(); ++i) {
            if (today == effectiveExerciseDates_[i]) {
                if (exercise()) {
                    exercised_ = true;
                    exerciseDate_ = today;
                }
            }
        }
    }
    if (exercised_) {
        // if exercised, return underlying npv for physical settlement and also for
        // cash settlement if we are still on the exercise date (since the cash
        // settlement takes place strictly after the exercise date usually)
        // FIXME: we assume that the settlement date lies strictly after the exercise
        // date, but before or on the next simulation date. Check this explicitly
        // by introducing the cash settlement date into the option wrapper (note
        // that we will probably need an effective cash settlement date then to
        // maintain the relative position to the effective exercise date).
        Real npv = (isPhysicalDelivery_ || today == exerciseDate_)
                       ? multiplier2() * getTimedNPV(activeUnderlyingInstrument_) * undMultiplier_
                       : 0.0;
        return npv + addNPV;
    } else {
        // if not exercised we just return the original option's NPV
        Real npv = multiplier2() * getTimedNPV(instrument_) * multiplier_;
        return npv + addNPV;
    }
}

const std::map<std::string, boost::any>& OptionWrapper::additionalResults() const {
    static std::map<std::string, boost::any> emptyMap;
    NPV();
    if (exercised_) {
        if (activeUnderlyingInstrument_ != nullptr)
            return activeUnderlyingInstrument_->additionalResults();
        else
            return emptyMap;
    } else {
        return instrument_->additionalResults();
    }
}

bool EuropeanOptionWrapper::exercise() const {
    if (!exercisable_)
        return false;

    // for European Exercise, we only require that underlying has positive PV
    bool res = getTimedNPV(activeUnderlyingInstrument_) * undMultiplier_ > 0.0;
    return res;
}

bool AmericanOptionWrapper::exercise() const {
    if (!exercisable_)
        return false;

    if (Settings::instance().evaluationDate() == effectiveExerciseDates_.back()) {
        bool res = getTimedNPV(activeUnderlyingInstrument_) * undMultiplier_ > 0.0;
        return res;
    } else {
        bool res = getTimedNPV(activeUnderlyingInstrument_) * undMultiplier_ > getTimedNPV(instrument_) * multiplier_;
        return res;
    }
}

bool BermudanOptionWrapper::exercise() const {
    if (!exercisable_)
        return false;

    // set active underlying instrument
    Date today = Settings::instance().evaluationDate();
    for (Size i = 0; i < effectiveExerciseDates_.size(); ++i) {
        if (today == effectiveExerciseDates_[i]) {
            activeUnderlyingInstrument_ = underlyingInstruments_[i];
            break;
        }
    }
    bool exercise = getTimedNPV(activeUnderlyingInstrument_) * undMultiplier_ > getTimedNPV(instrument_) * multiplier_;
    return exercise;
}
} // namespace data
} // namespace ore
