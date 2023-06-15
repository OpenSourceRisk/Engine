/*
 Copyright (C) 2020 Quaternion Risk Managment Ltd
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

/*! \file qle/instruments/ascot.hpp
    \brief Ascot class
*/

#pragma once

#include <qle/instruments/convertiblebond2.hpp>

#include <ql/instruments/bond.hpp>
#include <ql/instruments/callabilityschedule.hpp>
#include <ql/instruments/dividendschedule.hpp>
#include <ql/instruments/oneassetoption.hpp>
#include <ql/quote.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>

namespace QuantLib {
class IborIndex;
class PricingEngine;
} // namespace QuantLib

namespace QuantExt {

using namespace QuantLib;

//! ascot
class Ascot : public Instrument {
public:
    class arguments;
    class engine;

    Ascot(Option::Type callPut, const ext::shared_ptr<Exercise>& exercise, const Real& bondQuantity,
          const ext::shared_ptr<ConvertibleBond2>& bond, const Leg& fundingLeg);

    const Option::Type callPut() { return callPut_; }
    ext::shared_ptr<Exercise> exercise() { return exercise_; }
    const Real bondQuantity() { return bondQuantity_; }
    const ext::shared_ptr<ConvertibleBond2>& underlyingBond() const { return bond_; }

    const Leg& fundingLeg() const { return fundingLeg_; }

    bool isExpired() const override { return bond_->isExpired(); }

protected:
    void setupArguments(PricingEngine::arguments*) const override;

    Option::Type callPut_;
    ext::shared_ptr<Exercise> exercise_;
    Real bondQuantity_;
    ext::shared_ptr<ConvertibleBond2> bond_;
    Leg fundingLeg_;
};

class Ascot::arguments : public virtual PricingEngine::arguments {
public:
    arguments() {}

    Option::Type callPut;
    ext::shared_ptr<Exercise> exercise;
    Real bondQuantity;
    ext::shared_ptr<ConvertibleBond2> bond;
    Leg fundingLeg;

    void setupArguments(PricingEngine::arguments*) const;

    void validate() const override;
};

class Ascot::engine : public GenericEngine<Ascot::arguments, Ascot::results> {};

} // namespace QuantExt
