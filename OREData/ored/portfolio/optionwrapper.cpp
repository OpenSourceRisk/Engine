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

OptionWrapper::OptionWrapper(const boost::shared_ptr<Instrument>& inst, const bool isLongOption,
                             const std::vector<Date>& exerciseDate, const bool isPhysicalDelivery,
                             const std::vector<boost::shared_ptr<Instrument>>& undInst, const Real multiplier,
                             const Real undMultiplier)
    : InstrumentWrapper(inst, multiplier), isLong_(isLongOption), isPhysicalDelivery_(isPhysicalDelivery),
      contractExerciseDates_(exerciseDate), effectiveExerciseDates_(exerciseDate), underlyingInstruments_(undInst),
      activeUnderlyingInstrument_(undInst.at(0)), undMultiplier_(undMultiplier), exercised_(false),
      permanentlyExercised_(false) {}

void OptionWrapper::initialise(const vector<Date>& dateGrid) {

    // set "effective" exercise dates which get used to determine exercise during cube valuation
    // this is necessary since there is no guarantee that actual exercise dates are included in
    // the valuation date grid.

    if (contractExerciseDates_.back() < dateGrid.front()) {
        // Here we exercise before the first date.
        if (exercise()) {
            permanentlyExercised_ = true;
            exercised_ = true;
        }
    } else {
        for (Size i = 0; i < contractExerciseDates_.size(); ++i) {
            if (contractExerciseDates_[i] >= dateGrid.front() && contractExerciseDates_[i] <= dateGrid.back()) {

                // Find the effective exercise date in our grid.
                auto it = std::lower_bound(dateGrid.begin(), dateGrid.end(), contractExerciseDates_[i]);
                if (*it == contractExerciseDates_[i])
                    effectiveExerciseDates_[i] = contractExerciseDates_[i]; // we are pricing on the exercise date
                else
                    effectiveExerciseDates_[i] = *(it - 1); // we simulate the exercise just before the actual exercise
            }
        }
    }
}

void OptionWrapper::reset() {
    if (!permanentlyExercised_)
        exercised_ = false;
}

Real OptionWrapper::NPV() const {

    if (exercised_) {
        if (isPhysicalDelivery_) {
            return (isLong_ ? 1.0 : -1.0) * activeUnderlyingInstrument_->NPV() * undMultiplier_;
        } else {
            return 0.0;
        }
    } else {
        // check to see if we are exercising now.
        for (Size i = 0; i < effectiveExerciseDates_.size(); ++i) {
            if (Settings::instance().evaluationDate() == effectiveExerciseDates_[i]) {
                if (exercise()) {
                    exercised_ = true;
                }
            }
        }

        // Today, our NPV is still the option NPV
        // This will get called if the option is expired, but not exercised.
        return (isLong_ ? 1.0 : -1.0) * instrument_->NPV() * multiplier_;
    }
}

bool EuropeanOptionWrapper::exercise() const {
    // for European Exercise, we only require that underlying has positive PV
    return activeUnderlyingInstrument_->NPV() * undMultiplier_ > 0.0;
}

bool AmericanOptionWrapper::exercise() const {
    if (Settings::instance().evaluationDate() == effectiveExerciseDates_.back())
        return activeUnderlyingInstrument_->NPV() * undMultiplier_ > 0.0;
    else
        return activeUnderlyingInstrument_->NPV() * undMultiplier_ > instrument_->NPV() * multiplier_;
}

bool BermudanOptionWrapper::exercise() const {
    // set active underlying instrument
    Date today = Settings::instance().evaluationDate();
    for (Size i = 0; i < effectiveExerciseDates_.size(); ++i) {
        if (today == effectiveExerciseDates_[i]) {
            activeUnderlyingInstrument_ = underlyingInstruments_[i];
            break;
        }
    }

    if (today == effectiveExerciseDates_.back())
        return activeUnderlyingInstrument_->NPV() * undMultiplier_ > 0.0;
    else
        return activeUnderlyingInstrument_->NPV() * undMultiplier_ > instrument_->NPV() * multiplier_;
}
} // namespace data
} // namespace ore
