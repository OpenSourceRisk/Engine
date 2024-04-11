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

#include <ored/portfolio/compositeinstrumentwrapper.hpp>

namespace ore {
namespace data {

CompositeInstrumentWrapper::CompositeInstrumentWrapper(
    const std::vector<QuantLib::ext::shared_ptr<InstrumentWrapper>>& wrappers, const std::vector<Handle<Quote>>& fxRates,
    const Date& valuationDate)
    : InstrumentWrapper(), wrappers_(wrappers), fxRates_(fxRates), valuationDate_(valuationDate) {

    QL_REQUIRE(wrappers.size() > 0, "no instrument wrappers provided");
    QL_REQUIRE(fxRates_.empty() || fxRates_.size() == wrappers_.size(), "unexpected number of fxRates provided");

    for (const auto& w : wrappers_) {
        auto tmp = w->additionalInstruments();
        auto tmp2 = w->additionalMultipliers();
        additionalInstruments_.insert(additionalInstruments_.end(), tmp.begin(), tmp.end());
        additionalMultipliers_.insert(additionalMultipliers_.end(), tmp2.begin(), tmp2.end());
    }
}

void CompositeInstrumentWrapper::initialise(const std::vector<QuantLib::Date>& dates) {
    for (auto& w : wrappers_)
        w->initialise(dates);
};

void CompositeInstrumentWrapper::reset() {
    for (auto& w : wrappers_) {
        w->reset();
    }
}

QuantLib::Real CompositeInstrumentWrapper::NPV() const {
    Date today = Settings::instance().evaluationDate();
    if (valuationDate_ != Date()) {
        QL_REQUIRE(today == valuationDate_, "today must be the expected valuation date for this trade");
    }
    Real npv = 0;
    for (Size i = 0; i < wrappers_.size(); i++) {
        npv += wrappers_[i]->NPV() * (fxRates_.empty() ? 1.0 : fxRates_[i]->value());
    }
    for (auto& w : wrappers_) {
        numberOfPricings_ += w->getNumberOfPricings();
        cumulativePricingTime_ += w->getCumulativePricingTime();
        w->resetPricingStats();
    }
    return npv;
}

const std::map<std::string, boost::any>& CompositeInstrumentWrapper::additionalResults() const {
    additionalResults_.clear();
    for (auto const& w : wrappers_) {
        additionalResults_.insert(w->additionalResults().begin(), w->additionalResults().end());
    }
    return additionalResults_;
}

void CompositeInstrumentWrapper::updateQlInstruments() {
    for (auto& w : wrappers_) {
        w->updateQlInstruments();
    }
}

bool CompositeInstrumentWrapper::isOption() {
    for (const auto& w : wrappers_) {
        if (w->isOption()) {
            return true;
        }
    }
    return false;
}

} // namespace data
} // namespace ore
