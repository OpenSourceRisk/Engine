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

#ifndef qle_averageoisratehelper_i
#define qle_averageoisratehelper_i

%include instruments.i
%include termstructures.i
%include cashflows.i
%include timebasket.i
%include indexes.i
%include qle_averageois.i

%{
using QuantExt::AverageOISRateHelper;
%}

%shared_ptr(AverageOISRateHelper)
class AverageOISRateHelper : public RateHelper {
public:
    AverageOISRateHelper(const QuantLib::Handle<QuantLib::Quote>& fixedRate,
                         const QuantLib::Period& spotLagTenor,
                         const QuantLib::Period& swapTenor,
                         const QuantLib::Period& fixedTenor,
                         const QuantLib::DayCounter& fixedDayCounter,
                         const QuantLib::Calendar& fixedCalendar,
                         QuantLib::BusinessDayConvention fixedConvention,
                         QuantLib::BusinessDayConvention fixedPaymentAdjustment,
                         ext::shared_ptr<OvernightIndex>& overnightIndex,
                         const QuantLib::Period& onTenor,
                         const QuantLib::Handle<QuantLib::Quote>& onSpread,
                         QuantLib::Natural rateCutoff,
                         const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve = QuantLib::Handle<QuantLib::YieldTermStructure>());
    QuantLib::Real onSpread() const;
    ext::shared_ptr<AverageOIS> averageOIS() const;
};

#endif


