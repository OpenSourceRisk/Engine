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

/*! \file qle/instruments/bondoption.hpp
\brief bond option class
\ingroup instruments
*/

#pragma once

#include <ql/instruments/bond.hpp>
#include <ql/instruments/callabilityschedule.hpp>
#include <ql/pricingengine.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Bond option class
class BondOption : public QuantLib::Instrument {
public:
    BondOption(const QuantLib::ext::shared_ptr<QuantLib::Bond>& underlying, const CallabilitySchedule& putCallSchedule,
               const bool knocksOutOnDefault = false)
        : underlying_(underlying), putCallSchedule_(putCallSchedule), knocksOutOnDefault_(knocksOutOnDefault) {}

    bool isExpired() const override;
    void deepUpdate() override {
        underlying_->update();
        this->update();
    }

    const CallabilitySchedule& callability() const { return putCallSchedule_; }

    class arguments;
    class results;
    class engine;

private:
    void setupArguments(PricingEngine::arguments*) const override;
    const QuantLib::ext::shared_ptr<QuantLib::Bond> underlying_;
    const CallabilitySchedule putCallSchedule_;
    const bool knocksOutOnDefault_;
};

class BondOption::arguments : public PricingEngine::arguments {
public:
    QuantLib::ext::shared_ptr<QuantLib::Bond> underlying;
    CallabilitySchedule putCallSchedule;
    bool knocksOutOnDefault;
    void validate() const override;
};

class BondOption::results : public QuantLib::Bond::results {};

class BondOption::engine : public GenericEngine<BondOption::arguments, BondOption::results> {};

} // namespace QuantExt
