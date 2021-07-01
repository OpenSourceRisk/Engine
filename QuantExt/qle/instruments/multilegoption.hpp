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

/*! \file multilegoption.hpp
    \brief multi leg option instrument
*/

#pragma once

#include <ql/cashflow.hpp>
#include <ql/currency.hpp>
#include <ql/exercise.hpp>
#include <ql/instrument.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/pricingengine.hpp>
#include <ql/settings.hpp>

using namespace QuantLib;

namespace QuantExt {

class MultiLegOption : public Instrument {
public:
    class arguments;
    class results;
    class engine;

    /*! If an exercise is given, the instrument represents an exercise into underlying option
      otherwise the instrument represents the underlying itself. */
    MultiLegOption(const std::vector<Leg>& legs, const std::vector<bool>& payer, const std::vector<Currency>& currency,
                   const boost::shared_ptr<Exercise>& exercise = boost::shared_ptr<Exercise>(),
                   const Settlement::Type settlementType = Settlement::Physical,
                   Settlement::Method settlementMethod = Settlement::PhysicalOTC);

    bool isExpired() const override { return Settings::instance().evaluationDate() >= maturityDate(); }
    void setupArguments(PricingEngine::arguments*) const override;
    void fetchResults(const PricingEngine::results*) const override;

    const Date& maturityDate() const { return maturity_; }
    Real underlyingNpv() const;

private:
    const std::vector<Leg> legs_;
    const std::vector<bool> payer_;
    const std::vector<Currency> currency_;
    const boost::shared_ptr<Exercise> exercise_;
    const Settlement::Type settlementType_;
    const Settlement::Method settlementMethod_;
    Date maturity_;
    // results
    mutable Real underlyingNpv_;
};

class MultiLegOption::arguments : public virtual PricingEngine::arguments {
public:
    std::vector<Leg> legs;
    std::vector<Real> payer;
    std::vector<Currency> currency;
    boost::shared_ptr<Exercise> exercise;
    Settlement::Type settlementType;
    void validate() const {}
};

class MultiLegOption::results : public Instrument::results {
public:
    Real underlyingNpv;
    void reset();
};

class MultiLegOption::engine : public GenericEngine<MultiLegOption::arguments, MultiLegOption::results> {};

} // namespace QuantExt
