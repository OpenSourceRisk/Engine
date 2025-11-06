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

#ifndef qle_crossccyfixfloatswap_i
#define qle_crossccyfixfloatswap_i

%include instruments.i
%include scheduler.i
%include indexes.i
%include swap.i
%include qle_ccyswap.i

%{
using QuantExt::CrossCcyFixFloatSwap;
%}

%shared_ptr(CrossCcyFixFloatSwap)
class CrossCcyFixFloatSwap : public CrossCcySwap {
  public:
    enum Type { Receiver = -1, Payer = 1 };
    CrossCcyFixFloatSwap(CrossCcyFixFloatSwap::Type type,
                         QuantLib::Real fixedNominal,
                         const QuantLib::Currency& fixedCurrency,
                         const QuantLib::Schedule& fixedSchedule,
                         QuantLib::Rate fixedRate,
                         const QuantLib::DayCounter& fixedDayCount,
                         QuantLib::BusinessDayConvention fixedPaymentBdc,
                         QuantLib::Natural fixedPaymentLag,
                         const QuantLib::Calendar& fixedPaymentCalendar,
                         QuantLib::Real floatNominal,
                         const QuantLib::Currency& floatCurrency,
                         const QuantLib::Schedule& floatSchedule,
                         const ext::shared_ptr<IborIndex>& floatIndex,
                         QuantLib::Spread floatSpread,
                         QuantLib::BusinessDayConvention floatPaymentBdc,
                         QuantLib::Natural floatPaymentLag,
                         const QuantLib::Calendar& floatPaymentCalendar);
    CrossCcyFixFloatSwap::Type type() const;
    QuantLib::Real fixedNominal() const;
    const QuantLib::Currency& fixedCurrency() const;
    const QuantLib::Schedule& fixedSchedule() const;
    QuantLib::Rate fixedRate() const;
    const QuantLib::DayCounter& fixedDayCount() const;
    QuantLib::BusinessDayConvention fixedPaymentBdc();
    QuantLib::Natural fixedPaymentLag() const;
    const QuantLib::Calendar& fixedPaymentCalendar() const;
    QuantLib::Real floatNominal() const;
    const QuantLib::Currency& floatCurrency() const;
    const QuantLib::Schedule& floatSchedule() const;
    QuantLib::Rate floatSpread() const;
    QuantLib::BusinessDayConvention floatPaymentBdc();
    QuantLib::Natural floatPaymentLag() const;
    const QuantLib::Calendar& floatPaymentCalendar() const;
    QuantLib::Rate fairFixedRate() const;
    QuantLib::Spread fairSpread() const;
};

#endif
