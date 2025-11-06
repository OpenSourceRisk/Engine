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

#ifndef qle_cashflows_i
#define qle_cashflows_i

%include date.i
%include types.i
%include calendars.i
%include daycounters.i
%include indexes.i
%include termstructures.i
%include scheduler.i
%include vectors.i
%include cashflows.i
%include qle_indexes.i

%{
//using QuantExt::FXLinked;
using QuantExt::FXLinkedCashFlow;
using QuantExt::FloatingRateFXLinkedNotionalCoupon;
%}

//%shared_ptr(FXLinked)
//class FXLinked {
// private:
    // FXLinked();
  //  public:
    //    FXLinked(const QuantLib::Date& fixingDate, 
      //           QuantLib::Real foreignAmount,
        //         ext::shared_ptr<FxIndex> fxIndex);
   //     QuantLib::Date fxFixingDate() const;
   //     const ext::shared_ptr<FxIndex>& fxIndex() const;
   //     QuantLib::Real foreignAmount() const;
   //     QuantLib::Real fxRate() const;
   //     ext::shared_ptr<FXLinked> clone(ext::shared_ptr<FxIndex> fxIndex) = 0;
//};

%shared_ptr(FXLinkedCashFlow)
class FXLinkedCashFlow : public CashFlow {
  public:
    FXLinkedCashFlow(const QuantLib::Date& cashFlowDate,
                     const QuantLib::Date& fixingDate,
                     QuantLib::Real foreignAmount,
                     ext::shared_ptr<FxIndex> fxIndex);
    QuantLib::Date date() const;
    QuantLib::Date fxFixingDate() const;
    const ext::shared_ptr<FxIndex> fxIndex() const;
    QuantLib::Real amount() const override;
    QuantLib::Real foreignAmount() const;
    QuantLib::Real fxRate() const;
};

%shared_ptr(FloatingRateFXLinkedNotionalCoupon)
class FloatingRateFXLinkedNotionalCoupon : public FloatingRateCoupon {
  public:
    FloatingRateFXLinkedNotionalCoupon(const QuantLib::Date& fxFixingDate,
                                       QuantLib::Real foreignAmount,
                                       ext::shared_ptr<FxIndex> fxIndex,
                                       const ext::shared_ptr<FloatingRateCoupon> underlying);
    Real nominal() const;
    Rate rate() const;
    Rate indexFixing() const;
    void setPricer(const ext::shared_ptr<FloatingRateCouponPricer>& p);
};

#endif
