/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file blackmultilegoptionengine.hpp
    \brief Simple Black European swaption engine
*/

#pragma once

#include <qle/instruments/multilegoption.hpp>

#include <ql/instruments/nonstandardswaption.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/pricingengines/genericmodelengine.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>

namespace QuantExt {

class BlackMultiLegOptionEngineBase {
public:
    BlackMultiLegOptionEngineBase(const Handle<YieldTermStructure>& discountCurve,
                                  const Handle<SwaptionVolatilityStructure>& volatility);

    static bool instrumentIsHandled(const MultiLegOption& m, std::vector<std::string>& messages);

protected:
    static bool instrumentIsHandled(const std::vector<Leg>& legs, const std::vector<bool>& payer,
                                    const std::vector<Currency>& currency, const QuantLib::ext::shared_ptr<Exercise>& exercise,
                                    const Settlement::Type& settlementType, const Settlement::Method& settlementMethod,
                                    std::vector<std::string>& messages);

    void calculate() const;

    // inputs set in ctor
    Handle<YieldTermStructure> discountCurve_;
    Handle<SwaptionVolatilityStructure> volatility_;

    // inputs set by derived classes
    mutable std::vector<Leg> legs_;
    mutable std::vector<bool> payer_;
    mutable std::vector<Currency> currency_;
    mutable QuantLib::ext::shared_ptr<Exercise> exercise_;
    mutable Settlement::Type settlementType_;
    mutable Settlement::Method settlementMethod_;

    // outputs
    mutable Real npv_, underlyingNpv_;
    mutable std::map<std::string, boost::any> additionalResults_;
};

class BlackMultiLegOptionEngine : public QuantLib::GenericEngine<MultiLegOption::arguments, MultiLegOption::results>,
                                  public BlackMultiLegOptionEngineBase {
public:
    BlackMultiLegOptionEngine(const Handle<YieldTermStructure>& discountCurve,
                              const Handle<SwaptionVolatilityStructure>& volatility);

    void calculate() const override;
};

class BlackSwaptionFromMultilegOptionEngine : public QuantLib::GenericEngine<Swaption::arguments, Swaption::results>,
                                              public BlackMultiLegOptionEngineBase {
public:
    BlackSwaptionFromMultilegOptionEngine(const Handle<YieldTermStructure>& discountCurve,
                                          const Handle<SwaptionVolatilityStructure>& volatility);

    void calculate() const override;
};

class BlackNonstandardSwaptionFromMultilegOptionEngine
    : public QuantLib::GenericEngine<NonstandardSwaption::arguments, NonstandardSwaption::results>,
      public BlackMultiLegOptionEngineBase {
public:
    BlackNonstandardSwaptionFromMultilegOptionEngine(const Handle<YieldTermStructure>& discountCurve,
                                                     const Handle<SwaptionVolatilityStructure>& volatility);

    void calculate() const override;
};

} // namespace QuantExt
