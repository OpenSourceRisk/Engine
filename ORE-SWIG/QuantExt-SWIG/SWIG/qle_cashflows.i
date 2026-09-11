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
%include <std_map.i>
%include cashflows.i
%include qle_indexes.i

%template(FXLinkedFixings) std::map<Date, Real>;

%{
#include <qle/cashflows/cashflows.hpp>
#include <qle/cashflows/couponpricer.hpp>
#include <qle/cashflows/fixedratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/scaledcoupon.hpp>
#include <qle/cashflows/typedcashflow.hpp>
%}

%rename(QLESetCouponPricer) QuantExt::setCouponPricer;
%rename(QLESetCouponPricers) QuantExt::setCouponPricers;

namespace QuantExt {

void setCouponPricer(
    const QuantLib::Leg& leg,
    const ext::shared_ptr<QuantLib::FloatingRateCouponPricer>& pricer);

void setCouponPricers(
    const QuantLib::Leg& leg,
    const std::vector<
        ext::shared_ptr<QuantLib::FloatingRateCouponPricer> >& pricers);

%nodefaultctor CashFlows;
%rename(QLECashFlows) CashFlows;
class CashFlows {
  public:
    static Real spreadNpv(
        const QuantLib::Leg& leg,
        const QuantLib::YieldTermStructure& discountCurve,
        bool includeSettlementDateFlows,
        QuantLib::Date settlementDate = QuantLib::Date(),
        QuantLib::Date npvDate = QuantLib::Date());
    static Real sumCashflows(
        const QuantLib::Leg& leg,
        const QuantLib::Date& startDate,
        const QuantLib::Date& endDate);
    static std::vector<QuantLib::Rate> couponRates(const QuantLib::Leg& leg);
    static std::vector<QuantLib::Rate> couponDcfRates(const QuantLib::Leg& leg);
};

} // namespace QuantExt

%shared_ptr(QuantExt::ScaledCashFlow)
namespace QuantExt {
class ScaledCashFlow : public CashFlow {
  public:
    ScaledCashFlow(
        Real multiplier,
        ext::shared_ptr<CashFlow> underlying);
    QuantLib::Date date() const;
    Real amount() const;
    Real multiplier() const;
    const ext::shared_ptr<CashFlow>& underlyingCashFlow() const;
};
} // namespace QuantExt

%shared_ptr(QuantExt::ScaledCoupon)
namespace QuantExt {
class ScaledCoupon : public Coupon {
  public:
    ScaledCoupon(
        Real multiplier,
        ext::shared_ptr<Coupon> underlyingCoupon);
    void update();
    Rate amount() const;
    Real accruedAmount(const QuantLib::Date& d) const;
    Rate nominal() const;
    Rate rate() const;
    QuantLib::DayCounter dayCounter() const;
    Real multiplier() const;
    const ext::shared_ptr<Coupon>& underlyingCoupon() const;
};
} // namespace QuantExt

%shared_ptr(QuantExt::TypedCashFlow)
namespace QuantExt {
class TypedCashFlow : public SimpleCashFlow {
  public:
    enum class Type {
        Interest,
        Notional,
        Fee,
        Premium,
        Unspecified
    };
    TypedCashFlow(
        Real amount,
        const QuantLib::Date& date,
        Type type = Type::Unspecified);
    Type type() const;
};

} // namespace QuantExt

%shared_ptr(QuantExt::FXLinked)
%nodefaultctor QuantExt::FXLinked;
namespace QuantExt {
class FXLinked {
  public:
    FXLinked(
        const QuantLib::Date& fixingDate,
        QuantLib::Real foreignAmount,
        ext::shared_ptr<QuantExt::FxIndex> fxIndex,
        const QuantLib::Date& fxResetStart = QuantLib::Date(),
        QuantLib::Real domesticAmount = QuantLib::Null<QuantLib::Real>());
    virtual ~FXLinked();
    QuantLib::Date fxFixingDate() const;
    QuantLib::Real foreignAmount() const;
    const ext::shared_ptr<QuantExt::FxIndex>& fxIndex() const;
    QuantLib::Real fxRate() const;
    virtual ext::shared_ptr<QuantExt::FXLinked> clone(
        ext::shared_ptr<QuantExt::FxIndex> fxIndex) = 0;
};
} // namespace QuantExt

%shared_ptr(QuantExt::FXLinkedCashFlow)
namespace QuantExt {
class FXLinkedCashFlow : public CashFlow {
  public:
    FXLinkedCashFlow(const QuantLib::Date& cashFlowDate,
                     const QuantLib::Date& fixingDate,
                     QuantLib::Real foreignAmount,
                     ext::shared_ptr<QuantExt::FxIndex> fxIndex);
    QuantLib::Date date() const;
    QuantLib::Date fxFixingDate() const;
    const ext::shared_ptr<QuantExt::FxIndex> fxIndex() const;
    QuantLib::Real amount() const override;
    QuantLib::Real foreignAmount() const;
    QuantLib::Real fxRate() const;
};
} // namespace QuantExt

%shared_ptr(QuantExt::FixedRateFXLinkedNotionalCoupon)
namespace QuantExt {
class FixedRateFXLinkedNotionalCoupon : public FixedRateCoupon {
  public:
    FixedRateFXLinkedNotionalCoupon(
        const QuantLib::Date& fxFixingDate,
        QuantLib::Real foreignAmount,
        ext::shared_ptr<QuantExt::FxIndex> fxIndex,
        const ext::shared_ptr<FixedRateCoupon>& underlying,
        const QuantLib::Date& fxResetStart = QuantLib::Date(),
        QuantLib::Real domesticAmount = QuantLib::Null<QuantLib::Real>());
    QuantLib::Rate nominal() const;
    QuantLib::Rate rate() const;
    ext::shared_ptr<QuantExt::FXLinked> clone(
        ext::shared_ptr<QuantExt::FxIndex> fxIndex);
    QuantLib::Date fxFixingDate() const;
    QuantLib::Real foreignAmount() const;
    const ext::shared_ptr<QuantExt::FxIndex>& fxIndex() const;
    QuantLib::Real fxRate() const;
    ext::shared_ptr<FixedRateCoupon> underlying() const;
};
} // namespace QuantExt

%shared_ptr(QuantExt::AverageFXLinked)
%nodefaultctor QuantExt::AverageFXLinked;
namespace QuantExt {
class AverageFXLinked {
  public:
    AverageFXLinked(
        const std::vector<QuantLib::Date>& fixingDates,
        QuantLib::Real foreignAmount,
        ext::shared_ptr<QuantExt::FxIndex> fxIndex,
        bool inverted = false);
    const std::vector<QuantLib::Date>& fxFixingDates() const;
    QuantLib::Real foreignAmount() const;
    const ext::shared_ptr<QuantExt::FxIndex>& fxIndex() const;
    QuantLib::Real fxRate() const;
    virtual ext::shared_ptr<QuantExt::AverageFXLinked> clone(
        ext::shared_ptr<QuantExt::FxIndex> fxIndex) = 0;
};
} // namespace QuantExt

%shared_ptr(QuantExt::AverageFXLinkedCashFlow)
namespace QuantExt {
class AverageFXLinkedCashFlow : public CashFlow, public AverageFXLinked {
  public:
    AverageFXLinkedCashFlow(
        const QuantLib::Date& cashFlowDate,
        const std::vector<QuantLib::Date>& fixingDates,
        QuantLib::Real foreignAmount,
        ext::shared_ptr<QuantExt::FxIndex> fxIndex,
        bool inverted = false);
    QuantLib::Date date() const override;
    QuantLib::Real amount() const override;
    ext::shared_ptr<QuantExt::AverageFXLinked> clone(
        ext::shared_ptr<QuantExt::FxIndex> fxIndex) override;
    std::map<Date, Real> fixings() const;
};
} // namespace QuantExt

%shared_ptr(QuantExt::FXLinkedTypedCashFlow)
namespace QuantExt {
class FXLinkedTypedCashFlow : public FXLinkedCashFlow {
  public:
    FXLinkedTypedCashFlow(
        const QuantLib::Date& cashFlowDate,
        const QuantLib::Date& fixingDate,
        QuantLib::Real foreignAmount,
        ext::shared_ptr<QuantExt::FxIndex> fxIndex,
        QuantExt::TypedCashFlow::Type type =
            QuantExt::TypedCashFlow::Type::Unspecified);
    QuantExt::TypedCashFlow::Type type() const;
};
} // namespace QuantExt

%shared_ptr(QuantExt::FloatingRateFXLinkedNotionalCoupon)
namespace QuantExt {
class FloatingRateFXLinkedNotionalCoupon : public FloatingRateCoupon {
  public:
    FloatingRateFXLinkedNotionalCoupon(const QuantLib::Date& fxFixingDate,
                                       QuantLib::Real foreignAmount,
                                       ext::shared_ptr<QuantExt::FxIndex> fxIndex,
                                       const ext::shared_ptr<FloatingRateCoupon> underlying);
    Real nominal() const;
    Rate rate() const;
    Rate indexFixing() const;
    void setPricer(const ext::shared_ptr<FloatingRateCouponPricer>& p);
};
} // namespace QuantExt

#endif
