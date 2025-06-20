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

#ifndef qle_ratehelpers_i
#define qle_ratehelpers_i

%include ratehelpers.i
%include swap.i
%include qle_instruments.i
%include qle_tenorbasisswap.i
%include qle_oiccbasisswap.i

%{
using QuantExt::CrossCcyBasisSwapHelper;
using QuantExt::CrossCcyBasisMtMResetSwapHelper;
using QuantExt::TenorBasisSwapHelper;
using QuantExt::SubPeriodsSwapHelper;
using QuantExt::SubPeriodsCoupon1;
using QuantExt::OICCBSHelper;
using QuantExt::BasisTwoSwapHelper;
using QuantExt::ImmFraRateHelper;
using QuantExt::CrossCcyFixFloatSwapHelper;
%}

%shared_ptr(CrossCcyBasisMtMResetSwapHelper)
class CrossCcyBasisMtMResetSwapHelper : public RateHelper {
  public:
    CrossCcyBasisMtMResetSwapHelper(const Handle<Quote>& spreadQuote,
                                    const Handle<Quote>& spotFX,
                                    Natural settlementDays,
                                    const Calendar& settlementCalendar,
                                    const Period& swapTenor,
                                    BusinessDayConvention rollConvention,
                                    const ext::shared_ptr<IborIndex>& foreignCcyIndex,
                                    const ext::shared_ptr<IborIndex>& domesticCcyIndex,
                                    const Handle<YieldTermStructure>& foreignCcyDiscountCurve,
                                    const Handle<YieldTermStructure>& domesticCcyDiscountCurve,
                                    const bool foreignIndexGiven,
                                    const bool domesticIndexGiven,
                                    const bool foreignDiscountCurveGiven,
                                    const bool domesticDiscountCurveGiven,
                                    const Handle<YieldTermStructure>& foreignCcyFxFwdRateCurve
                                        = Handle<YieldTermStructure>(),
                                    const Handle<YieldTermStructure>& domesticCcyFxFwdRateCurve
                                        = Handle<YieldTermStructure>(),
                                    bool eom = false,
                                    bool spreadOnForeignCcy = true,
                                    boost::optional<QuantLib::Period> foreignTenor = boost::none,
                                    boost::optional<QuantLib::Period> domesticTenor = boost::none);
    ext::shared_ptr<CrossCcyBasisMtMResetSwap> swap();
};

%shared_ptr(CrossCcyBasisSwapHelper)
class CrossCcyBasisSwapHelper : public RateHelper {
  public:
    CrossCcyBasisSwapHelper(const Handle<Quote>& spreadQuote,
                            const Handle<Quote>& spotFX,
                            Natural settlementDays,
                            const Calendar& settlementCalendar,
                            const Period& swapTenor,
                            BusinessDayConvention rollConvention,
                            const ext::shared_ptr<IborIndex>& flatIndex,
                            const ext::shared_ptr<IborIndex>& spreadIndex,
                            const Handle<YieldTermStructure>& flatDiscountCurve,
                            const Handle<YieldTermStructure>& spreadDiscountCurve,
                            const bool flatIndexGiven, const bool spreadIndexGiven,
                            const bool flatDiscountCurveGiven,
                            const bool spreadDiscountCurveGiven,
                            bool eom = false,
                            bool flatIsDomestic = true);
    ext::shared_ptr<CrossCcyBasisSwap> swap();
};

%shared_ptr(TenorBasisSwapHelper)
class TenorBasisSwapHelper : public RateHelper {
  public:
    TenorBasisSwapHelper(QuantLib::Handle<QuantLib::Quote> spread, const QuantLib::Period& swapTenor,
                         const ext::shared_ptr<IborIndex>& payIndex,
                         const ext::shared_ptr<IborIndex>& receiveIndex,
                         const QuantLib::Handle<QuantLib::YieldTermStructure>& discountingCurve, const bool payIndexGiven,
                         const bool receiveIndexGiven, const bool discountingGiven, const bool spreadOnRec = true,
                         const bool includeSpread = false, const QuantLib::Period& payFrequency = QuantLib::Period(),
                         const QuantLib::Period& recFrequency = QuantLib::Period(), const bool telescopicValueDates = false,
                         const QuantExt::SubPeriodsCoupon1::Type type = QuantExt::SubPeriodsCoupon1::Compounding);
    ext::shared_ptr<TenorBasisSwap> swap();
};

%shared_ptr(SubPeriodsSwapHelper)
class SubPeriodsSwapHelper : public RateHelper {
  public:
    SubPeriodsSwapHelper(QuantLib::Handle<QuantLib::Quote> spread,
                         const QuantLib::Period& swapTenor,
                         const QuantLib::Period& fixedTenor,
                         const QuantLib::Calendar& fixedCalendar,
                         const QuantLib::DayCounter& fixedDayCount,
                         QuantLib::BusinessDayConvention fixedConvention,
                         const QuantLib::Period& floatPayTenor,
                         const ext::shared_ptr<IborIndex>& iborIndex,
                         const QuantLib::DayCounter& floatDayCount,
                         const QuantLib::Handle<QuantLib::YieldTermStructure>& discountingCurve =
                             QuantLib::Handle<QuantLib::YieldTermStructure>(),
                         QuantExt::SubPeriodsCoupon1::Type type = QuantExt::SubPeriodsCoupon1::Compounding);
    ext::shared_ptr<SubPeriodsSwap> swap();
};

%shared_ptr(BasisTwoSwapHelper)
class BasisTwoSwapHelper : public RateHelper {
  public:
    BasisTwoSwapHelper(const QuantLib::Handle<QuantLib::Quote>& spread,
                       const QuantLib::Period& swapTenor,
                       const QuantLib::Calendar& calendar,
                       QuantLib::Frequency longFixedFrequency,
                       QuantLib::BusinessDayConvention longFixedConvention,
                       const QuantLib::DayCounter& longFixedDayCount,
                       const ext::shared_ptr<IborIndex>& longIndex,
                       bool longIndexGiven,
                       QuantLib::Frequency shortFixedFrequency,
                       QuantLib::BusinessDayConvention shortFixedConvention,
                       const QuantLib::DayCounter& shortFixedDayCount,
                       const ext::shared_ptr<IborIndex>& shortIndex,
                       bool longMinusShort, bool shortIndexGiven,
                       const QuantLib::Handle<QuantLib::YieldTermStructure>& discountingCurve
                             = QuantLib::Handle<QuantLib::YieldTermStructure>(),
                       bool discountCurveGiven = false);
    ext::shared_ptr<VanillaSwap> longSwap();
    ext::shared_ptr<VanillaSwap> shortSwap();
};

%shared_ptr(OICCBSHelper)
class OICCBSHelper : public RateHelper {
  public:
    OICCBSHelper(QuantLib::Natural settlementDays,
                    const QuantLib::Period& term,
                    const ext::shared_ptr<OvernightIndex>& payIndex,
                    const QuantLib::Period& payTenor,
                    const ext::shared_ptr<OvernightIndex>& recIndex,
                    const QuantLib::Period& recTenor,
                    const QuantLib::Handle<QuantLib::Quote>& spreadQuote,
                    const QuantLib::Handle<QuantLib::YieldTermStructure>& fixedDiscountCurve,
                    bool spreadQuoteOnPayLeg,
                    bool fixedDiscountOnPayLeg);
    ext::shared_ptr<OvernightIndexedCrossCcyBasisSwap> swap();
};

%shared_ptr(ImmFraRateHelper)
class ImmFraRateHelper : public RateHelper {
  public:
    ImmFraRateHelper(const QuantLib::Handle<QuantLib::Quote>& rate,
                     const QuantLib::Size imm1,
                     const QuantLib::Size imm2,
                     const ext::shared_ptr<IborIndex>& iborIndex,
                     QuantLib::Pillar::Choice pillar = QuantLib::Pillar::LastRelevantDate,
                     QuantLib::Date customPillarDate = QuantLib::Date());
};

%shared_ptr(CrossCcyFixFloatSwapHelper)
class CrossCcyFixFloatSwapHelper : public RateHelper {
  public:
      CrossCcyFixFloatSwapHelper(const QuantLib::Handle<QuantLib::Quote>& rate,
                                 const QuantLib::Handle<QuantLib::Quote>& spotFx,
                                 QuantLib::Natural settlementDays,
                                 const QuantLib::Calendar& paymentCalendar,
                                 QuantLib::BusinessDayConvention paymentConvention,
                                 const QuantLib::Period& tenor,
                                 const QuantLib::Currency& fixedCurrency,
                                 QuantLib::Frequency fixedFrequency,
                                 QuantLib::BusinessDayConvention fixedConvention,
                                 const QuantLib::DayCounter& fixedDayCount,
                                 const ext::shared_ptr<IborIndex>& index,
                                 const QuantLib::Handle<QuantLib::YieldTermStructure>& floatDiscount,
                                 const QuantLib::Handle<QuantLib::Quote>& spread = QuantLib::Handle<QuantLib::Quote>(),
                                 bool endOfMonth = false);
    ext::shared_ptr<CrossCcyFixFloatSwap> swap();
};

#endif
