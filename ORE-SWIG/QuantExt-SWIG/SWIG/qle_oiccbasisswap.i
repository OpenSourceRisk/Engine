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

#ifndef qle_oiccbasisswap_i
#define qle_oiccbasisswap_i

%include instruments.i
%include scheduler.i
%include indexes.i

%{
using QuantExt::OvernightIndexedCrossCcyBasisSwap;
using QuantExt::OvernightIndexedCrossCcyBasisSwapEngine;
%}

%shared_ptr(OvernightIndexedCrossCcyBasisSwap)
class OvernightIndexedCrossCcyBasisSwap : public Instrument {
  public:
    OvernightIndexedCrossCcyBasisSwap(QuantLib::Real payNominal,
                                      QuantLib::Currency payCurrency,
                                      const QuantLib::Schedule& paySchedule,
                                      const ext::shared_ptr<OvernightIndex>& payIndex,
                                      QuantLib::Real paySpread,
                                      QuantLib::Real recNominal,
                                      QuantLib::Currency recCurrency,
                                      const QuantLib::Schedule& recSchedule,
                                      const ext::shared_ptr<OvernightIndex>& recIndex,
                                      QuantLib::Real recSpread);
    /*Name Inspectors*/
    /*Pay Leg*/
    QuantLib::Real payNominal() const;
    QuantLib::Currency payCurrency();
    const QuantLib::Schedule& paySchedule();
    QuantLib::Real paySpread();
    /*Receiver Leg*/
    QuantLib::Real recNominal() const;
    QuantLib::Currency recCurrency();
    const QuantLib::Schedule& recSchedule();
    QuantLib::Real recSpread();
    /*Other*/
    const QuantLib::Leg& payLeg();
    const QuantLib::Leg& recLeg();
    /*Name Results*/
    QuantLib::Real payLegBPS();
    QuantLib::Real payLegNPV();
    QuantLib::Real fairPayLegSpread() const;
    QuantLib::Real recLegBPS() const;
    QuantLib::Real recLegNPV() const;
    QuantLib::Spread fairRecLegSpread() const;
};

%shared_ptr(OvernightIndexedCrossCcyBasisSwapEngine)
class OvernightIndexedCrossCcyBasisSwapEngine : public PricingEngine {
  public:
    OvernightIndexedCrossCcyBasisSwapEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& ts1,
                                            const QuantLib::Currency& ccy1,
                                            const QuantLib::Handle<QuantLib::YieldTermStructure>& ts2,
                                            const QuantLib::Currency& ccy2,
                                            const QuantLib::Handle<QuantLib::Quote>& fx);
    QuantLib::Handle<QuantLib::YieldTermStructure> ts1();
    QuantLib::Handle<QuantLib::YieldTermStructure> ts2();
    QuantLib::Currency ccy1();
    QuantLib::Currency ccy2();
    QuantLib::Handle<QuantLib::Quote> fx();
};

#endif
