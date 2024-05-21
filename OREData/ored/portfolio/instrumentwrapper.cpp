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

#include <ored/portfolio/instrumentwrapper.hpp>

namespace ore {
namespace data {

InstrumentWrapper::InstrumentWrapper() : instrument_(nullptr), multiplier_(1.0) {}

InstrumentWrapper::InstrumentWrapper(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& inst, const Real multiplier,
                                     const std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>& additionalInstruments,
                                     const std::vector<Real>& additionalMultipliers)
    : instrument_(inst), multiplier_(multiplier), additionalInstruments_(additionalInstruments),
      additionalMultipliers_(additionalMultipliers) {
    QL_REQUIRE(additionalInstruments_.size() == additionalMultipliers_.size(),
               "vector size mismatch, instruments (" << additionalInstruments_.size() << ") vs multipliers ("
                                                     << additionalMultipliers_.size() << ")");
}

QuantLib::Real InstrumentWrapper::additionalInstrumentsNPV() const {
    Real npv = 0.0;
    for (QuantLib::Size i = 0; i < additionalInstruments_.size(); ++i)
        npv += additionalInstruments_[i]->NPV() * additionalMultipliers_[i];
    return npv;
}

void InstrumentWrapper::updateQlInstruments() {
    // the instrument might contain nested lazy objects which we also want to be updated
    instrument_->deepUpdate();
    for (QuantLib::Size i = 0; i < additionalInstruments_.size(); ++i)
        additionalInstruments_[i]->deepUpdate();
}

bool InstrumentWrapper::isOption() { return false; }

QuantLib::ext::shared_ptr<QuantLib::Instrument> InstrumentWrapper::qlInstrument(const bool calculate) const {
    if (calculate && instrument_ != nullptr) {
        getTimedNPV(instrument_);
    }
    return instrument_;
}

Real InstrumentWrapper::multiplier() const { return multiplier_; }

Real InstrumentWrapper::multiplier2() const { return 1.0; }

const std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>& InstrumentWrapper::additionalInstruments() const {
    return additionalInstruments_;
}

const std::vector<Real>& InstrumentWrapper::additionalMultipliers() const { return additionalMultipliers_; }

boost::timer::nanosecond_type InstrumentWrapper::getCumulativePricingTime() const { return cumulativePricingTime_; }

std::size_t InstrumentWrapper::getNumberOfPricings() const { return numberOfPricings_; }

void InstrumentWrapper::resetPricingStats() const {
    numberOfPricings_ = 0;
    cumulativePricingTime_ = 0;
}

Real InstrumentWrapper::getTimedNPV(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& instr) const {
    if (instr == nullptr)
        return 0.0;
    if (instr->isCalculated() || instr->isExpired())
        return instr->NPV();
    boost::timer::cpu_timer timer_;
    Real tmp = instr->NPV();
    cumulativePricingTime_ += timer_.elapsed().wall;
    ++numberOfPricings_;
    return tmp;
}

VanillaInstrument::VanillaInstrument(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& inst, const Real multiplier,
                                     const std::vector<QuantLib::ext::shared_ptr<QuantLib::Instrument>>& additionalInstruments,
                                     const std::vector<Real>& additionalMultipliers)
    : InstrumentWrapper(inst, multiplier, additionalInstruments, additionalMultipliers) {}

QuantLib::Real VanillaInstrument::NPV() const {
    return getTimedNPV(instrument_) * multiplier_ + additionalInstrumentsNPV();
}

const std::map<std::string, boost::any>& VanillaInstrument::additionalResults() const {
    static std::map<std::string, boost::any> empty;
    if (instrument_ == nullptr)
        return empty;
    getTimedNPV(instrument_);
    return instrument_->additionalResults();
}

} // namespace data
} // namespace ore
