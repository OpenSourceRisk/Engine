/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef qle_equityforward_i
#define qle_equityforward_i

%include instruments.i
%include termstructures.i
%include cashflows.i
%include timebasket.i
%include indexes.i

%include qle_termstructures.i

%{
using QuantExt::EquityForward;
using QuantExt::DiscountingEquityForwardEngine;
%}

%shared_ptr(EquityForward)
class EquityForward : public Instrument {
  public:
    EquityForward(const std::string& name,
                  const QuantLib::Currency& currency,
                  const QuantLib::Position::Type& longShort,
                  const QuantLib::Real& quantity,
                  const QuantLib::Date& maturityDate,
                  const QuantLib::Real& strike);
    bool isExpired() const;
    const std::string& name();
    QuantLib::Currency currency();
    QuantLib::Position::Type longShort();
    QuantLib::Real quantity();
    QuantLib::Date maturityDate();
    QuantLib::Real strike();
};

%shared_ptr(DiscountingEquityForwardEngine)
class DiscountingEquityForwardEngine : public PricingEngine {
  public:
    DiscountingEquityForwardEngine(const QuantLib::Handle<QuantExt::EquityIndex2>& equityIndex,
                                   const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                   boost::optional<bool> includeSettlementDateFlows = boost::none,
                                   const QuantLib::Date& settlementDate = QuantLib::Date(),
                                   const QuantLib::Date& npvDate = QuantLib::Date());
    void calculate();
    const Handle<EquityIndex2>& equityIndex() const;
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve();
};

#endif
