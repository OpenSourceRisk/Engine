/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file ple/instruments/outperformanceoption.hpp
    \brief outperformance option
*/

#ifndef quantext_outperformance_option_hpp
#define quantext_outperformance_option_hpp

#include <ql/option.hpp>
#include <ql/event.hpp>
#include <qle/indexes/fxindex.hpp>


namespace QuantLib {
class Quote;
class YieldTermStructure;
} // namespace QuantLib

namespace QuantExt {
using namespace QuantLib;

//! Outperformance option

class OutperformanceOption : public QuantLib::Instrument {
public:
    class arguments;
    class results;
    class engine;
    OutperformanceOption(const QuantLib::ext::shared_ptr<Exercise>& exercise, const Option::Type optionType, 
        const Real strikeReturn, const Real initialValue1, const Real initialValue2, const Real notional, 
        const Real knockInPrice = Null<Real>(), const Real knockOutPrice = Null<Real>(), QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex1 = nullptr,
        QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex2 = nullptr);

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override;
    //@}

    QuantLib::ext::shared_ptr<Exercise> exercise() const { return exercise_; }
    Option::Type optionType() const { return optionType_; }
    Real strikeReturn() const { return strikeReturn_; }
    Real initialValue1() const { return initialValue1_; }
    Real initialValue2() const { return initialValue2_; }
    Real notional() const { return notional_; }
    Real knockInPrice() const { return knockInPrice_; }
    Real knockOutPrice() const { return knockOutPrice_; }
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex1() const { return fxIndex1_; }
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex2() const { return fxIndex2_; }
private:
    void setupExpired() const override;
    void fetchResults(const PricingEngine::results*) const override;
    
    QuantLib::ext::shared_ptr<Exercise> exercise_;
    Option::Type optionType_;
    Real strikeReturn_;
    Real initialValue1_;
    Real initialValue2_;
    Real notional_;
    Real knockInPrice_;
    Real knockOutPrice_;
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex1_;
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex2_;
};

//! %Arguments for Outperformance option calculation
class OutperformanceOption::arguments : public PricingEngine::arguments {
public:
    QuantLib::ext::shared_ptr<Exercise> exercise;
    Option::Type optionType;
    Real strikeReturn;
    Real initialValue1;
    Real initialValue2;
    Real notional;
    Real knockInPrice;
    Real knockOutPrice;
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex1;
    QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex2;
    void validate() const override;
};

//! %Results from Outperformance option calculation
class OutperformanceOption::results : public QuantLib::Instrument::results {
public:
    Real standardDeviation;
    void reset() override {
        standardDeviation = Null<Real>();
        Instrument::results::reset();
    }
};

//! base class for outperformance option engines
class OutperformanceOption::engine : public GenericEngine<OutperformanceOption::arguments, OutperformanceOption::results> {};
} // namespace QuantExt

#endif
