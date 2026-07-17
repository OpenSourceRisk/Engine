/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef qle_cashflows_ext_i
#define qle_cashflows_ext_i

%include common.i
%include cashflows.i
%include volatilities.i
%include inflation.i
%include qle_indexes.i

namespace QuantExt {
    class FutureExpiryCalculator;
    class EquityCouponPricer;
    class EquityMarginCouponPricer;
    class CorrelationTermStructure;
} // namespace QuantExt

%{
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/equitycouponpricer.hpp>
#include <qle/cashflows/equitymargincoupon.hpp>
#include <qle/cashflows/equitymargincouponpricer.hpp>
#include <qle/cashflows/strippedcapflooredcpicoupon.hpp>
#include <qle/cashflows/yoyinflationcoupon.hpp>
#include <qle/cashflows/strippedcapflooredyoyinflationcoupon.hpp>
#include <qle/cashflows/nonstandardcapflooredyoyinflationcoupon.hpp>
#include <qle/cashflows/nonstandardinflationcouponpricer.hpp>
#include <qle/cashflows/nonstandardyoyinflationcoupon.hpp>
using QuantLib::InflationCouponPricer;
%}

namespace QuantExt {

enum class EquityReturnType {
    Price,
    Total,
    Absolute,
    Dividend
};

} // namespace QuantExt

// QuantExt::EquityCoupon – full wrapper replacing previous empty stub
%shared_ptr(QuantExt::EquityCoupon)
namespace QuantExt {
class EquityCoupon : public Coupon {
  public:
    EquityCoupon(const Date& paymentDate, Real nominal,
                 const Date& startDate, const Date& endDate,
                 Natural fixingDays,
                 const ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve,
                 const DayCounter& dayCounter,
                 EquityReturnType returnType,
                 Real dividendFactor = 1.0,
                 bool notionalReset = false,
                 Real initialPrice = Null<Real>(),
                 Real quantity = Null<Real>(),
                 const Date& fixingStartDate = Date(),
                 const Date& fixingEndDate = Date(),
                 const Date& refPeriodStart = Date(),
                 const Date& refPeriodEnd = Date(),
                 const Date& exCouponDate = Date(),
                 const ext::shared_ptr<FxIndex>& fxIndex = nullptr,
                 bool initialPriceIsInTargetCcy = false,
                 Real legInitialNotional = Null<Real>(),
                 const Date& legFixingDate = Date());

    Real amount() const;
    DayCounter dayCounter() const;
    Real accruedAmount(const Date& d) const;
    Rate rate() const;
    Real nominal() const;

    const ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve() const;
    const ext::shared_ptr<FxIndex>& fxIndex() const;
    EquityReturnType returnType() const;
    Real dividendFactor() const;
    Date fixingStartDate() const;
    Date fixingEndDate() const;
    bool notionalReset() const;
    Real inputQuantity() const;
    Real inputInitialPrice() const;
    Real inputNominal() const;
    std::vector<Date> fixingDates() const;
    Real initialPrice() const;
    bool initialPriceIsInTargetCcy() const;
    Real quantity() const;
    Real fxRate() const;
    Real legInitialNotional() const;
    Date legFixingDate() const;

    void setPricer(const ext::shared_ptr<QuantExt::EquityCouponPricer>&);
    ext::shared_ptr<QuantExt::EquityCouponPricer> pricer() const;
};
} // namespace QuantExt

// QuantExt::EquityCouponPricer – full wrapper replacing previous partial stub
%shared_ptr(QuantExt::EquityCouponPricer)
namespace QuantExt {
class EquityCouponPricer : public Observer, public Observable {
  public:
    EquityCouponPricer();
    Rate swapletRate();
    void initialize(const EquityCoupon& coupon);

    void setEquityVolatility(const Handle<BlackVolTermStructure>& equityVol);
    void setFxVolatility(const Handle<BlackVolTermStructure>& fxVol);
    void setCorrelation(const Handle<QuantExt::CorrelationTermStructure>& correlation);
};
} // namespace QuantExt

// QuantExt::EquityMarginCoupon – full wrapper
%shared_ptr(QuantExt::EquityMarginCoupon)
namespace QuantExt {
class EquityMarginCoupon : public Coupon {
  public:
    EquityMarginCoupon(const Date& paymentDate, Real nominal, Rate rate, Real marginFactor,
                       const Date& startDate, const Date& endDate, Natural fixingDays,
                       const ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve,
                       const DayCounter& dayCounter,
                       bool isTotalReturn = false, Real dividendFactor = 1.0,
                       bool notionalReset = false,
                       Real initialPrice = Null<Real>(), Real quantity = Null<Real>(),
                       const Date& fixingStartDate = Date(),
                       const Date& fixingEndDate = Date(),
                       const Date& refPeriodStart = Date(),
                       const Date& refPeriodEnd = Date(),
                       const Date& exCouponDate = Date(), Real multiplier = Null<Real>(),
                       const ext::shared_ptr<FxIndex>& fxIndex = nullptr,
                       bool initialPriceIsInTargetCcy = false);

    Real amount() const;
    DayCounter dayCounter() const;
    Real accruedAmount(const Date& d) const;
    Rate rate() const;
    Real nominal() const;

    const ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve() const;
    const ext::shared_ptr<FxIndex>& fxIndex() const;
    bool isTotalReturn() const;
    Real dividendFactor() const;
    Date fixingStartDate() const;
    Date fixingEndDate() const;
    std::vector<Date> fixingDates() const;
    Real initialPrice() const;
    bool initialPriceIsInTargetCcy() const;
    Real quantity() const;
    Real fxRate() const;
    Real marginFactor() const;
    InterestRate fixedRate() const;
    Real multiplier() const;

    void setPricer(const ext::shared_ptr<QuantExt::EquityMarginCouponPricer>&);
    ext::shared_ptr<QuantExt::EquityMarginCouponPricer> pricer() const;
};
} // namespace QuantExt

// QuantExt::EquityMarginCouponPricer – full wrapper
%shared_ptr(QuantExt::EquityMarginCouponPricer)
namespace QuantExt {
class EquityMarginCouponPricer : public Observer, public Observable {
  public:
    EquityMarginCouponPricer();
    Rate rate() const;
    void initialize(const EquityMarginCoupon& coupon);
};
} // namespace QuantExt

// QuantExt::EquityLeg builder using helper-function-with-kwargs pattern
%{
Leg _EquityLeg(
    const Schedule& schedule,
    const ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve,
    const std::vector<Real>& notionals,
    const DayCounter& paymentDayCounter = DayCounter(),
    const BusinessDayConvention paymentConvention = Following,
    const Calendar& paymentCalendar = Calendar(),
    Integer paymentLag = 0,
    QuantExt::EquityReturnType returnType = QuantExt::EquityReturnType::Price,
    Real dividendFactor = 1.0,
    Real initialPrice = Null<Real>(),
    bool initialPriceIsInTargetCcy = false,
    Natural fixingDays = 0,
    bool notionalReset = false,
    Real quantity = Null<Real>(),
    const ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr,
    const std::vector<Date>& paymentDates = {},
    const Schedule& valuationSchedule = Schedule())
{
    QuantExt::EquityLeg leg(schedule, equityCurve, fxIndex);
    leg.withNotionals(notionals)
       .withPaymentDayCounter(paymentDayCounter)
       .withPaymentAdjustment(paymentConvention)
       .withPaymentCalendar(paymentCalendar)
       .withPaymentLag(paymentLag)
       .withReturnType(returnType)
       .withDividendFactor(dividendFactor)
       .withInitialPrice(initialPrice)
       .withInitialPriceIsInTargetCcy(initialPriceIsInTargetCcy)
       .withFixingDays(fixingDays)
       .withNotionalReset(notionalReset)
       .withQuantity(quantity)
       .withPaymentDates(paymentDates)
       .withValuationSchedule(valuationSchedule);
    return leg;
}
%}
#if !defined(SWIGJAVA) && !defined(SWIGCSHARP)
%feature("kwargs") _EquityLeg;
#endif
%rename(EquityLeg) _EquityLeg;
Leg _EquityLeg(
    const Schedule& schedule,
    const ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve,
    const std::vector<Real>& notionals,
    const DayCounter& paymentDayCounter = DayCounter(),
    const BusinessDayConvention paymentConvention = Following,
    const Calendar& paymentCalendar = Calendar(),
    Integer paymentLag = 0,
    QuantExt::EquityReturnType returnType = QuantExt::EquityReturnType::Price,
    Real dividendFactor = 1.0,
    Real initialPrice = Null<Real>(),
    bool initialPriceIsInTargetCcy = false,
    Natural fixingDays = 0,
    bool notionalReset = false,
    Real quantity = Null<Real>(),
    const ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr,
    const std::vector<Date>& paymentDates = {},
    const Schedule& valuationSchedule = Schedule());

// QuantExt::IndexedCoupon – coupon with an indexed notional multiplier
%{
#include <qle/cashflows/indexedcoupon.hpp>
%}

%shared_ptr(QuantExt::IndexedCoupon)
namespace QuantExt {
class IndexedCoupon : public Coupon {
  public:
    IndexedCoupon(const ext::shared_ptr<Coupon>& c, Real qty,
                  const ext::shared_ptr<Index>& index,
                  const Date& fixingDate);
    IndexedCoupon(const ext::shared_ptr<Coupon>& c, Real qty,
                  Real initialFixing);

    Real amount() const;
    Real nominal() const;
    Rate rate() const;
    DayCounter dayCounter() const;
    Real accruedAmount(const Date& d) const;

    ext::shared_ptr<Coupon> underlying() const;
    Real quantity() const;
    ext::shared_ptr<Index> index() const;
    const Date& fixingDate() const;
    Real initialFixing() const;
    Real multiplier() const;
};
} // namespace QuantExt

// QuantExt::IndexWrappedCashFlow – cashflow with an indexed notional multiplier
%shared_ptr(QuantExt::IndexWrappedCashFlow)
namespace QuantExt {
class IndexWrappedCashFlow : public CashFlow {
  public:
    IndexWrappedCashFlow(const ext::shared_ptr<CashFlow>& c, Real qty,
                         const ext::shared_ptr<Index>& index,
                         const Date& fixingDate);
    IndexWrappedCashFlow(const ext::shared_ptr<CashFlow>& c, Real qty,
                         Real initialFixing);

    Date date() const;
    Real amount() const;

    ext::shared_ptr<CashFlow> underlying() const;
    Real quantity() const;
    ext::shared_ptr<Index> index() const;
    const Date& fixingDate() const;
    Real initialFixing() const;
    Real multiplier() const;
};
} // namespace QuantExt

// Free functions for unpacking indexed coupons/cashflows
namespace QuantExt {
    ext::shared_ptr<CashFlow> unpackIndexedCouponOrCashFlow(
        const ext::shared_ptr<CashFlow>& c);
    ext::shared_ptr<Coupon> unpackIndexedCoupon(
        const ext::shared_ptr<Coupon>& c);
    ext::shared_ptr<CashFlow> unpackIndexWrappedCashFlow(
        const ext::shared_ptr<CashFlow>& c);
    Real getIndexedCouponOrCashFlowMultiplier(
        const ext::shared_ptr<CashFlow>& c);
}

// QuantExt::IndexedCouponLeg builder using helper-function-with-kwargs pattern
%{
Leg _IndexedCouponLeg(
    const Leg& underlyingLeg,
    Real qty,
    const ext::shared_ptr<Index>& index,
    Real initialFixing = Null<Real>(),
    Real initialNotionalFixing = Null<Real>(),
    Size fixingDays = 0,
    const Calendar& fixingCalendar = Calendar(),
    const BusinessDayConvention& fixingConvention = Following,
    bool inArrearsFixing = false)
{
    QuantExt::IndexedCouponLeg leg(underlyingLeg, qty, index);
    if (initialFixing != Null<Real>())
        leg.withInitialFixing(initialFixing);
    if (initialNotionalFixing != Null<Real>())
        leg.withInitialNotionalFixing(initialNotionalFixing);
    leg.withFixingDays(fixingDays)
       .withFixingCalendar(fixingCalendar)
       .withFixingConvention(fixingConvention)
       .inArrearsFixing(inArrearsFixing);
    return leg;
}
%}
#if !defined(SWIGJAVA) && !defined(SWIGCSHARP)
%feature("kwargs") _IndexedCouponLeg;
#endif
%rename(IndexedCouponLeg) _IndexedCouponLeg;
Leg _IndexedCouponLeg(
    const Leg& underlyingLeg,
    Real qty,
    const ext::shared_ptr<Index>& index,
    Real initialFixing = Null<Real>(),
    Real initialNotionalFixing = Null<Real>(),
    Size fixingDays = 0,
    const Calendar& fixingCalendar = Calendar(),
    const BusinessDayConvention& fixingConvention = Following,
    bool inArrearsFixing = false);

// ---------------------------------------------------------------------------
// CPIVolatilitySurface – minimal SWIG declaration (not in QuantLib-SWIG)
// ---------------------------------------------------------------------------
%{
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>
using QuantLib::CPIVolatilitySurface;
%}
%shared_ptr(CPIVolatilitySurface)
class CPIVolatilitySurface : public VolatilityTermStructure {
  private:
    CPIVolatilitySurface();
};
%template(CPIVolatilitySurfaceHandle) Handle<CPIVolatilitySurface>;
%template(RelinkableCPIVolatilitySurfaceHandle) RelinkableHandle<CPIVolatilitySurface>;

// ---------------------------------------------------------------------------
// QuantExt::CPICoupon – extends QuantLib::CPICoupon
// ---------------------------------------------------------------------------
%{
#include <qle/cashflows/cpicoupon.hpp>
#include <qle/cashflows/cpicouponpricer.hpp>
%}

%shared_ptr(QuantExt::CPICoupon)
%rename(QLECPICoupon) QuantExt::CPICoupon;
namespace QuantExt {
class CPICoupon : public ::CPICoupon {
  public:
    CPICoupon(Real baseCPI,
              const Date& paymentDate, Real nominal,
              const Date& startDate, const Date& endDate,
              const ext::shared_ptr<ZeroInflationIndex>& index,
              const Period& observationLag,
              CPI::InterpolationType observationInterpolation,
              const DayCounter& dayCounter,
              Real fixedRate,
              const Date& refPeriodStart = Date(),
              const Date& refPeriodEnd = Date(),
              const Date& exCouponDate = Date(),
              bool subtractInflationNominal = false);

    bool subtractInflationNotional();
    void setPricer(const ext::shared_ptr<QuantLib::CPICouponPricer>& pricer);
};
} // namespace QuantExt

// ---------------------------------------------------------------------------
// QuantExt::CappedFlooredCPICashFlow
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::CappedFlooredCPICashFlow)
namespace QuantExt {
class CappedFlooredCPICashFlow : public CPICashFlow {
  public:
    CappedFlooredCPICashFlow(const ext::shared_ptr<CPICashFlow>& underlying,
                             Date startDate = Date(),
                             Period observationLag = 0 * Days,
                             Rate cap = Null<Rate>(),
                             Rate floor = Null<Rate>());

    void setPricer(const ext::shared_ptr<QuantExt::InflationCashFlowPricer>& pricer);
    bool isCapped() const;
    bool isFloored() const;
    ext::shared_ptr<CPICashFlow> underlying() const;
};
} // namespace QuantExt

// ---------------------------------------------------------------------------
// QuantExt::CappedFlooredCPICoupon
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::CappedFlooredCPICoupon)
namespace QuantExt {
class CappedFlooredCPICoupon : public CPICoupon {
  public:
    CappedFlooredCPICoupon(const ext::shared_ptr<CPICoupon>& underlying,
                           Date startDate = Date(),
                           Rate cap = Null<Rate>(),
                           Rate floor = Null<Rate>());

    ext::shared_ptr<CPICoupon> underlying() const;
    bool isCapped() const;
    bool isFloored() const;
    void setPricer(const ext::shared_ptr<QuantLib::CPICouponPricer>& pricer);
};
} // namespace QuantExt

// ---------------------------------------------------------------------------
// QuantExt stripped capped/floored CPI coupon, cashflow and leg
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::StrippedCappedFlooredCPICoupon)
%nodefaultctor QuantExt::StrippedCappedFlooredCPICoupon;
namespace QuantExt {
class StrippedCappedFlooredCPICoupon : public QuantExt::CPICoupon {
  public:
    Rate rate() const;
    Rate cap() const;
    Rate floor() const;
    Rate effectiveCap() const;
    Rate effectiveFloor() const;
    ext::shared_ptr<QuantExt::CappedFlooredCPICoupon> underlying();
    bool isCap() const;
    bool isFloor() const;
    bool isCollar() const;
};
} // namespace QuantExt

%inline %{
ext::shared_ptr<QuantExt::StrippedCappedFlooredCPICoupon>
as_stripped_capped_floored_cpi_coupon(const ext::shared_ptr<QuantLib::CashFlow>& cashFlow) {
    return ext::dynamic_pointer_cast<QuantExt::StrippedCappedFlooredCPICoupon>(cashFlow);
}
%}

%{
ext::shared_ptr<QuantExt::StrippedCappedFlooredCPICoupon>
_makeStrippedCappedFlooredCPICoupon(
    const ext::shared_ptr<QuantExt::CappedFlooredCPICoupon>& underlying) {
    return ext::make_shared<QuantExt::StrippedCappedFlooredCPICoupon>(underlying);
}
%}
%rename(makeStrippedCappedFlooredCPICoupon) _makeStrippedCappedFlooredCPICoupon;
ext::shared_ptr<QuantExt::StrippedCappedFlooredCPICoupon>
_makeStrippedCappedFlooredCPICoupon(
    const ext::shared_ptr<QuantExt::CappedFlooredCPICoupon>& underlying);

%shared_ptr(QuantExt::StrippedCappedFlooredCPICashFlow)
namespace QuantExt {
class StrippedCappedFlooredCPICashFlow : public QuantLib::CPICashFlow {
  public:
    explicit StrippedCappedFlooredCPICashFlow(
        const ext::shared_ptr<QuantExt::CappedFlooredCPICashFlow>& underlying);
    Real amount() const;
    ext::shared_ptr<QuantExt::CappedFlooredCPICashFlow> underlying() const;
};
} // namespace QuantExt

%shared_ptr(QuantExt::StrippedCappedFlooredCPICouponLeg)
namespace QuantExt {
class StrippedCappedFlooredCPICouponLeg {
  public:
    explicit StrippedCappedFlooredCPICouponLeg(const QuantLib::Leg& underlyingLeg);
    operator QuantLib::Leg() const;
};
} // namespace QuantExt

%{
QuantLib::Leg _makeStrippedCappedFlooredCPICouponLeg(const QuantLib::Leg& underlyingLeg) {
    return QuantExt::StrippedCappedFlooredCPICouponLeg(underlyingLeg);
}
%}
%rename(makeStrippedCappedFlooredCPICouponLeg) _makeStrippedCappedFlooredCPICouponLeg;
QuantLib::Leg _makeStrippedCappedFlooredCPICouponLeg(const QuantLib::Leg& underlyingLeg);

// ---------------------------------------------------------------------------
// QuantExt Inflation Pricers
// ---------------------------------------------------------------------------
%shared_ptr(QuantExt::InflationCashFlowPricer)
namespace QuantExt {
class InflationCashFlowPricer : public Observer, public Observable {
  public:
    InflationCashFlowPricer(
        const Handle<CPIVolatilitySurface>& vol = Handle<CPIVolatilitySurface>(),
        const Handle<YieldTermStructure>& yts = Handle<YieldTermStructure>());
    Handle<CPIVolatilitySurface> volatility();
    Handle<YieldTermStructure> yieldCurve();
};
} // namespace QuantExt

%shared_ptr(QuantExt::BlackCPICashFlowPricer)
namespace QuantExt {
class BlackCPICashFlowPricer : public InflationCashFlowPricer {
  public:
    BlackCPICashFlowPricer(
        const Handle<CPIVolatilitySurface>& vol = Handle<CPIVolatilitySurface>(),
        const Handle<YieldTermStructure>& yts = Handle<YieldTermStructure>());
};
} // namespace QuantExt

%shared_ptr(QuantExt::BachelierCPICashFlowPricer)
namespace QuantExt {
class BachelierCPICashFlowPricer : public InflationCashFlowPricer {
  public:
    BachelierCPICashFlowPricer(
        const Handle<CPIVolatilitySurface>& vol = Handle<CPIVolatilitySurface>(),
        const Handle<YieldTermStructure>& yts = Handle<YieldTermStructure>());
};
} // namespace QuantExt

%shared_ptr(QuantExt::CappedFlooredCPICouponPricer)
namespace QuantExt {
class CappedFlooredCPICouponPricer : public CPICouponPricer {
  public:
    CappedFlooredCPICouponPricer(
        const Handle<CPIVolatilitySurface>& vol = Handle<CPIVolatilitySurface>(),
        const Handle<YieldTermStructure>& yts = Handle<YieldTermStructure>());
};
} // namespace QuantExt

%shared_ptr(QuantExt::BlackCPICouponPricer)
namespace QuantExt {
class BlackCPICouponPricer : public CappedFlooredCPICouponPricer {
  public:
    BlackCPICouponPricer(
        const Handle<CPIVolatilitySurface>& vol = Handle<CPIVolatilitySurface>(),
        const Handle<YieldTermStructure>& yts = Handle<YieldTermStructure>());
};
} // namespace QuantExt

%shared_ptr(QuantExt::BachelierCPICouponPricer)
namespace QuantExt {
class BachelierCPICouponPricer : public CappedFlooredCPICouponPricer {
  public:
    BachelierCPICouponPricer(
        const Handle<CPIVolatilitySurface>& vol = Handle<CPIVolatilitySurface>(),
        const Handle<YieldTermStructure>& yts = Handle<YieldTermStructure>());
};
} // namespace QuantExt

// ---------------------------------------------------------------------------
// QuantExt non-standard YoY inflation coupons and pricers
// ---------------------------------------------------------------------------
%shared_ptr(QuantLib::InflationCouponPricer)
%nodefaultctor InflationCouponPricer;
class InflationCouponPricer {
};

%shared_ptr(QuantExt::NonStandardYoYInflationCoupon)
namespace QuantExt {
class NonStandardYoYInflationCoupon : public InflationCoupon {
  public:
    NonStandardYoYInflationCoupon(
        const Date& paymentDate,
        Real nominal,
        const Date& startDate,
        const Date& endDate,
        Natural fixingDays,
        const ext::shared_ptr<ZeroInflationIndex>& index,
        const Period& observationLag,
        const DayCounter& dayCounter,
        Real gearing = 1.0,
        Spread spread = 0.0,
        const Date& refPeriodStart = Date(),
        const Date& refPeriodEnd = Date(),
        bool addInflationNotional = false,
        CPI::InterpolationType interpolation = CPI::Flat);

    Real gearing() const;
    Spread spread() const;
    Rate adjustedFixing() const;
    Date fixingDateNumerator() const;
    Date fixingDateDenumerator() const;
    ext::shared_ptr<ZeroInflationIndex> cpiIndex() const;
    Rate indexFixing() const;
    Date fixingDate() const;
    Rate rate() const;
    bool addInflationNotional() const;
    bool isInterpolated() const;
    CPI::InterpolationType interpolationType() const;
    void setPricer(const ext::shared_ptr<QuantLib::InflationCouponPricer>& pricer);
};
} // namespace QuantExt

%shared_ptr(QuantExt::NonStandardYoYInflationCouponPricer)
namespace QuantExt {
class NonStandardYoYInflationCouponPricer : public InflationCouponPricer {
  public:
    NonStandardYoYInflationCouponPricer(
        const Handle<YieldTermStructure>& nominalTermStructure);
    NonStandardYoYInflationCouponPricer(
        const Handle<YoYOptionletVolatilitySurface>& capletVol,
        const Handle<YieldTermStructure>& nominalTermStructure);

    Handle<YoYOptionletVolatilitySurface> capletVolatility() const;
    Handle<YieldTermStructure> nominalTermStructure() const;
    void setCapletVolatility(const Handle<YoYOptionletVolatilitySurface>& capletVol);
    Real swapletPrice() const;
    Rate swapletRate() const;
    Real capletPrice(Rate effectiveCap) const;
    Rate capletRate(Rate effectiveCap) const;
    Real floorletPrice(Rate effectiveFloor) const;
    Rate floorletRate(Rate effectiveFloor) const;
};
} // namespace QuantExt

%shared_ptr(QuantExt::NonStandardCappedFlooredYoYInflationCoupon)
namespace QuantExt {
class NonStandardCappedFlooredYoYInflationCoupon : public NonStandardYoYInflationCoupon {
  public:
    NonStandardCappedFlooredYoYInflationCoupon(
        const ext::shared_ptr<NonStandardYoYInflationCoupon>& underlying,
        Rate cap = Null<Rate>(),
        Rate floor = Null<Rate>());
    NonStandardCappedFlooredYoYInflationCoupon(
        const Date& paymentDate,
        Real nominal,
        const Date& startDate,
        const Date& endDate,
        Natural fixingDays,
        const ext::shared_ptr<ZeroInflationIndex>& index,
        const Period& observationLag,
        const DayCounter& dayCounter,
        Real gearing = 1.0,
        Spread spread = 0.0,
        Rate cap = Null<Rate>(),
        Rate floor = Null<Rate>(),
        const Date& refPeriodStart = Date(),
        const Date& refPeriodEnd = Date(),
        bool addInflationNotional = false,
        CPI::InterpolationType interpolation = CPI::Flat);

    Rate rate() const;
    Rate cap() const;
    Rate floor() const;
    Rate effectiveCap() const;
    Rate effectiveFloor() const;
    bool isCapped() const;
    bool isFloored() const;
    void setPricer(const ext::shared_ptr<NonStandardYoYInflationCouponPricer>& pricer);
};
} // namespace QuantExt

%shared_ptr(QuantExt::CappedFlooredYoYInflationCoupon)
%rename(QLECappedFlooredYoYInflationCoupon) QuantExt::CappedFlooredYoYInflationCoupon;
namespace QuantExt {
class CappedFlooredYoYInflationCoupon : public QuantLib::CappedFlooredYoYInflationCoupon {
  public:
    CappedFlooredYoYInflationCoupon(
        const Date& paymentDate,
        Real nominal,
        const Date& startDate,
        const Date& endDate,
        Natural fixingDays,
        const ext::shared_ptr<YoYInflationIndex>& index,
        const Period& observationLag,
        CPI::InterpolationType interpolation,
        const DayCounter& dayCounter,
        Real gearing = 1.0,
        Spread spread = 0.0,
        Rate cap = Null<Rate>(),
        Rate floor = Null<Rate>(),
        const Date& refPeriodStart = Date(),
        const Date& refPeriodEnd = Date(),
        bool addInflationNotional = false);

    Rate rate() const;
    Rate cap() const;
    Rate floor() const;
    bool isCapped() const;
    bool isFloored() const;
};
} // namespace QuantExt

%shared_ptr(QuantExt::StrippedCappedFlooredYoYInflationCoupon)
%nodefaultctor QuantExt::StrippedCappedFlooredYoYInflationCoupon;
namespace QuantExt {
class StrippedCappedFlooredYoYInflationCoupon : public YoYInflationCoupon {
  public:
    Rate rate() const;
    Rate cap() const;
    Rate floor() const;
    Rate effectiveCap() const;
    Rate effectiveFloor() const;
    bool isCap() const;
    bool isFloor() const;
    bool isCollar() const;
    void setPricer(const ext::shared_ptr<YoYInflationCouponPricer>& pricer);
    ext::shared_ptr<CappedFlooredYoYInflationCoupon> underlying();
};
} // namespace QuantExt

%{
ext::shared_ptr<QuantExt::StrippedCappedFlooredYoYInflationCoupon>
_makeStrippedCappedFlooredYoYInflationCoupon(
    const ext::shared_ptr<QuantExt::CappedFlooredYoYInflationCoupon>& underlying) {
    return ext::make_shared<QuantExt::StrippedCappedFlooredYoYInflationCoupon>(underlying);
}
%}
%rename(makeStrippedCappedFlooredYoYInflationCoupon) _makeStrippedCappedFlooredYoYInflationCoupon;
ext::shared_ptr<QuantExt::StrippedCappedFlooredYoYInflationCoupon>
_makeStrippedCappedFlooredYoYInflationCoupon(
    const ext::shared_ptr<QuantExt::CappedFlooredYoYInflationCoupon>& underlying);

%shared_ptr(QuantExt::StrippedCappedFlooredYoYInflationCouponLeg)
namespace QuantExt {
class StrippedCappedFlooredYoYInflationCouponLeg {
  public:
    explicit StrippedCappedFlooredYoYInflationCouponLeg(const Leg& underlyingLeg);
    operator Leg() const;
};
} // namespace QuantExt

%{
Leg _QLENonStandardYoYInflationCouponLeg(
    const Schedule& schedule,
    const Calendar& calendar,
    const ext::shared_ptr<ZeroInflationIndex>& index,
    const Period& observationLag,
    const std::vector<Real>& notionals,
    const DayCounter& paymentDayCounter = DayCounter(),
    BusinessDayConvention paymentAdjustment = Following,
    Natural fixingDays = 0,
    const std::vector<Real>& gearings = {},
    const std::vector<Spread>& spreads = {},
    const std::vector<Rate>& caps = {},
    const std::vector<Rate>& floors = {},
    const Handle<YieldTermStructure>& rateCurve = Handle<YieldTermStructure>(),
    bool addInflationNotional = false,
    CPI::InterpolationType observationInterpolation = CPI::Flat,
    const std::vector<Date>& paymentDates = {},
    Integer paymentLag = 0) {
    return QuantExt::NonStandardYoYInflationLeg(schedule, calendar, index, observationLag)
        .withNotionals(notionals)
        .withPaymentDayCounter(paymentDayCounter)
        .withPaymentAdjustment(paymentAdjustment)
        .withFixingDays(fixingDays)
        .withGearings(gearings)
        .withSpreads(spreads)
        .withCaps(caps)
        .withFloors(floors)
        .withRateCurve(rateCurve)
        .withInflationNotional(addInflationNotional)
        .withObservationInterpolation(observationInterpolation)
        .withPaymentDates(paymentDates)
        .withPaymentLag(paymentLag);
}
%}
#if !defined(SWIGJAVA) && !defined(SWIGCSHARP)
%feature("kwargs") _QLENonStandardYoYInflationCouponLeg;
#endif
%rename(NonStandardYoYInflationCouponLeg) _QLENonStandardYoYInflationCouponLeg;
Leg _QLENonStandardYoYInflationCouponLeg(
    const Schedule& schedule,
    const Calendar& calendar,
    const ext::shared_ptr<ZeroInflationIndex>& index,
    const Period& observationLag,
    const std::vector<Real>& notionals,
    const DayCounter& paymentDayCounter = DayCounter(),
    BusinessDayConvention paymentAdjustment = Following,
    Natural fixingDays = 0,
    const std::vector<Real>& gearings = {},
    const std::vector<Spread>& spreads = {},
    const std::vector<Rate>& caps = {},
    const std::vector<Rate>& floors = {},
    const Handle<YieldTermStructure>& rateCurve = Handle<YieldTermStructure>(),
    bool addInflationNotional = false,
    CPI::InterpolationType observationInterpolation = CPI::Flat,
    const std::vector<Date>& paymentDates = {},
    Integer paymentLag = 0);

// ---------------------------------------------------------------------------
// QuantExt::CPILeg builder (kwargs pattern)
// ---------------------------------------------------------------------------
%{
Leg _QLECPILeg(
    const Schedule& schedule,
    const ext::shared_ptr<ZeroInflationIndex>& index,
    const Handle<YieldTermStructure>& rateCurve,
    Real baseCPI,
    const Period& observationLag,
    const std::vector<Real>& notionals,
    const std::vector<Real>& fixedRates,
    const DayCounter& paymentDayCounter = DayCounter(),
    const BusinessDayConvention paymentConvention = Following,
    const Calendar& paymentCalendar = Calendar(),
    Integer paymentLag = 0,
    CPI::InterpolationType observationInterpolation = CPI::AsIndex,
    bool subtractInflationNominal = false,
    const std::vector<Rate>& caps = {},
    const std::vector<Rate>& floors = {})
{
    return QuantExt::CPILeg(schedule, index, rateCurve, baseCPI, observationLag)
        .withNotionals(notionals)
        .withFixedRates(fixedRates)
        .withPaymentDayCounter(paymentDayCounter)
        .withPaymentAdjustment(paymentConvention)
        .withPaymentCalendar(paymentCalendar)
        .withPaymentLag(paymentLag)
        .withObservationInterpolation(observationInterpolation)
        .withSubtractInflationNominal(subtractInflationNominal)
        .withCaps(caps)
        .withFloors(floors);
}
%}
#if !defined(SWIGJAVA) && !defined(SWIGCSHARP)
%feature("kwargs") _QLECPILeg;
#endif
%rename(QLECPILeg) _QLECPILeg;
Leg _QLECPILeg(
    const Schedule& schedule,
    const ext::shared_ptr<ZeroInflationIndex>& index,
    const Handle<YieldTermStructure>& rateCurve,
    Real baseCPI,
    const Period& observationLag,
    const std::vector<Real>& notionals,
    const std::vector<Real>& fixedRates,
    const DayCounter& paymentDayCounter = DayCounter(),
    const BusinessDayConvention paymentConvention = Following,
    const Calendar& paymentCalendar = Calendar(),
    Integer paymentLag = 0,
    CPI::InterpolationType observationInterpolation = CPI::AsIndex,
    bool subtractInflationNominal = false,
    const std::vector<Rate>& caps = {},
    const std::vector<Rate>& floors = {});

%shared_ptr(QuantExt::CommodityCashFlow)
namespace QuantExt {
class CommodityCashFlow : public CashFlow {
  public:
    QuantLib::Real quantity() const;
    QuantLib::Real spread() const;
    QuantLib::Real gearing() const;
    bool useFuturePrice() const;
};
} // namespace QuantExt

%shared_ptr(QuantExt::CommodityIndexedCashFlow)
namespace QuantExt {
class CommodityIndexedCashFlow : public CommodityCashFlow {
  public:
    enum class PaymentTiming { InAdvance, InArrears, RelativeToExpiry };

    CommodityIndexedCashFlow(QuantLib::Real quantity,
                             const QuantLib::Date& pricingDate,
                             const QuantLib::Date& paymentDate,
                             const ext::shared_ptr<CommodityIndex>& index,
                             QuantLib::Real spread = 0.0,
                             QuantLib::Real gearing = 1.0,
                             bool useFuturePrice = false,
                             const Date& contractDate = Date(),
                             const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr,
                             QuantLib::Natural dailyExpiryOffset = QuantLib::Null<QuantLib::Natural>(),
                             const ext::shared_ptr<FxIndex>& fxIndex = nullptr);

    const QuantLib::Date& pricingDate() const;
    QuantLib::Date date() const override;
    QuantLib::Real amount() const override;
};
} // namespace QuantExt

// QuantExt::CommodityQuantityFrequency enum
namespace QuantExt {
enum class CommodityQuantityFrequency {
    PerCalculationPeriod,
    PerCalendarDay,
    PerPricingDay,
    PerHour,
    PerHourAndCalendarDay
};
} // namespace QuantExt

// QuantExt::CommodityIndexedAverageCashFlow
%shared_ptr(QuantExt::CommodityIndexedAverageCashFlow)
namespace QuantExt {
class CommodityIndexedAverageCashFlow : public CommodityCashFlow {
  public:
    enum class PaymentTiming { InAdvance, InArrears };

    // Constructor 1: explicit payment date
    CommodityIndexedAverageCashFlow(
        QuantLib::Real quantity, const QuantLib::Date& startDate, const QuantLib::Date& endDate,
        const QuantLib::Date& paymentDate, const ext::shared_ptr<CommodityIndex>& index,
        const QuantLib::Calendar& pricingCalendar = QuantLib::Calendar(),
        QuantLib::Real spread = 0.0, QuantLib::Real gearing = 1.0,
        bool useFuturePrice = false, QuantLib::Natural deliveryDateRoll = 0,
        QuantLib::Integer futureMonthOffset = 0,
        const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr,
        bool includeEndDate = true, bool excludeStartDate = true,
        bool useBusinessDays = true);

    // Constructor 2: deduced payment date
    CommodityIndexedAverageCashFlow(
        QuantLib::Real quantity, const QuantLib::Date& startDate, const QuantLib::Date& endDate,
        QuantLib::Natural paymentLag, QuantLib::Calendar paymentCalendar,
        QuantLib::BusinessDayConvention paymentConvention,
        const ext::shared_ptr<CommodityIndex>& index,
        const QuantLib::Calendar& pricingCalendar = QuantLib::Calendar(),
        QuantLib::Real spread = 0.0, QuantLib::Real gearing = 1.0,
        CommodityIndexedAverageCashFlow::PaymentTiming paymentTiming
            = CommodityIndexedAverageCashFlow::PaymentTiming::InArrears,
        bool useFuturePrice = false, QuantLib::Natural deliveryDateRoll = 0,
        QuantLib::Integer futureMonthOffset = 0,
        const ext::shared_ptr<FutureExpiryCalculator>& calc = nullptr,
        bool includeEndDate = true, bool excludeStartDate = true,
        const QuantLib::Date& paymentDateOverride = Date(),
        bool useBusinessDays = true);

    // Accessors
    const QuantLib::Date& startDate() const;
    const QuantLib::Date& endDate() const;
    ext::shared_ptr<CommodityIndex> index() const;
    QuantLib::Natural deliveryDateRoll() const;
    QuantLib::Integer futureMonthOffset() const;
    bool useBusinessDays() const;
    QuantLib::Real periodQuantity() const;
    QuantLib::Date date() const;
    QuantLib::Real amount() const;
};
} // namespace QuantExt

// QuantExt::CommodityIndexedAverageLeg builder using helper-function-with-kwargs pattern
%{
Leg _CommodityIndexedAverageLeg(
    const Schedule& schedule,
    const ext::shared_ptr<QuantExt::CommodityIndex>& index,
    const std::vector<Real>& quantities,
    Natural paymentLag = 0,
    const Calendar& paymentCalendar = Calendar(),
    BusinessDayConvention paymentConvention = Following,
    const Calendar& pricingCalendar = Calendar(),
    const std::vector<Real>& spreads = {},
    const std::vector<Real>& gearings = {},
    QuantExt::CommodityIndexedAverageCashFlow::PaymentTiming paymentTiming
        = QuantExt::CommodityIndexedAverageCashFlow::PaymentTiming::InArrears,
    bool useFuturePrice = false,
    Natural deliveryDateRoll = 0,
    Integer futureMonthOffset = 0,
    const ext::shared_ptr<QuantExt::FutureExpiryCalculator>& calc = nullptr,
    bool includeEndDate = true,
    bool excludeStartDate = true,
    bool useBusinessDays = true,
    QuantExt::CommodityQuantityFrequency quantityFrequency
        = QuantExt::CommodityQuantityFrequency::PerCalculationPeriod,
    const ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr)
{
    QuantExt::CommodityIndexedAverageLeg leg(schedule, index);
    if (quantities.size() == 1)
        leg.withQuantities(quantities[0]);
    else
        leg.withQuantities(quantities);
    leg.withPaymentLag(paymentLag)
       .withPaymentCalendar(paymentCalendar)
       .withPaymentConvention(paymentConvention)
       .withPricingCalendar(pricingCalendar)
       .paymentTiming(paymentTiming)
       .useFuturePrice(useFuturePrice)
       .withDeliveryDateRoll(deliveryDateRoll)
       .withFutureMonthOffset(futureMonthOffset)
       .withFutureExpiryCalculator(calc)
       .includeEndDate(includeEndDate)
       .excludeStartDate(excludeStartDate)
       .useBusinessDays(useBusinessDays)
       .withQuantityFrequency(quantityFrequency)
       .withFxIndex(fxIndex);
    if (spreads.size() == 1)
        leg.withSpreads(spreads[0]);
    else if (!spreads.empty())
        leg.withSpreads(spreads);
    if (gearings.size() == 1)
        leg.withGearings(gearings[0]);
    else if (!gearings.empty())
        leg.withGearings(gearings);
    return leg;
}
%}
#if !defined(SWIGJAVA) && !defined(SWIGCSHARP)
%feature("kwargs") _CommodityIndexedAverageLeg;
#endif
%rename(CommodityIndexedAverageLeg) _CommodityIndexedAverageLeg;
Leg _CommodityIndexedAverageLeg(
    const Schedule& schedule,
    const ext::shared_ptr<QuantExt::CommodityIndex>& index,
    const std::vector<Real>& quantities,
    Natural paymentLag = 0,
    const Calendar& paymentCalendar = Calendar(),
    BusinessDayConvention paymentConvention = Following,
    const Calendar& pricingCalendar = Calendar(),
    const std::vector<Real>& spreads = {},
    const std::vector<Real>& gearings = {},
    QuantExt::CommodityIndexedAverageCashFlow::PaymentTiming paymentTiming
        = QuantExt::CommodityIndexedAverageCashFlow::PaymentTiming::InArrears,
    bool useFuturePrice = false,
    Natural deliveryDateRoll = 0,
    Integer futureMonthOffset = 0,
    const ext::shared_ptr<QuantExt::FutureExpiryCalculator>& calc = nullptr,
    bool includeEndDate = true,
    bool excludeStartDate = true,
    bool useBusinessDays = true,
    QuantExt::CommodityQuantityFrequency quantityFrequency
        = QuantExt::CommodityQuantityFrequency::PerCalculationPeriod,
    const ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr);

%shared_ptr(QuantExt::TRSCashFlow)
namespace QuantExt {
class TRSCashFlow : public CashFlow {
  public:
    TRSCashFlow(const Date& paymentDate,
                const Date& fixingStartDate,
                const Date& fixingEndDate,
                const Real notional,
                const QuantLib::ext::shared_ptr<Index>& Index,
                const Real initialPrice = Null<Real>(),
                const QuantLib::ext::shared_ptr<FxIndex>& fxIndex = nullptr,
                const bool applyFXIndexFixingDays = false);

    Real amount() const override;
    Date date() const override;
    const Date& fixingStartDate() const;
    const Date& fixingEndDate() const;
    Real initialPrice() const;
};
} // namespace QuantExt

// QuantExt::BondTRSCashFlow – bond TRS cashflow (subclass of TRSCashFlow)
%shared_ptr(QuantExt::BondTRSCashFlow)
namespace QuantExt {
class BondTRSCashFlow : public TRSCashFlow {
  public:
    BondTRSCashFlow(const Date& paymentDate,
                    const Date& fixingStartDate,
                    const Date& fixingEndDate,
                    Real bondNotional,
                    const ext::shared_ptr<Index>& index,
                    Real initialPrice = Null<Real>(),
                    const ext::shared_ptr<FxIndex>& fxIndex = nullptr,
                    bool applyFXIndexFixingDays = false);

    Real notional(Date date) const;
    Real notional() const;
    void setFixingStartDate(Date fixingDate);
};
} // namespace QuantExt

// QuantExt::BondTRSLeg builder using helper-function-with-kwargs pattern
%{
Leg _BondTRSLeg(
    const std::vector<Date>& valuationDates,
    const std::vector<Date>& paymentDates,
    Real bondNotional,
    const ext::shared_ptr<Index>& index,
    const ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr,
    Real initialPrice = Null<Real>(),
    bool applyFXIndexFixingDays = false)
{
    QuantExt::BondTRSLeg leg(valuationDates, paymentDates, bondNotional, index, fxIndex);
    if (initialPrice != Null<Real>())
        leg.withInitialPrice(initialPrice);
    leg.withApplyFXIndexFixingDays(applyFXIndexFixingDays);
    return leg;
}
%}
#if !defined(SWIGJAVA) && !defined(SWIGCSHARP)
%feature("kwargs") _BondTRSLeg;
#endif
%rename(BondTRSLeg) _BondTRSLeg;
Leg _BondTRSLeg(
    const std::vector<Date>& valuationDates,
    const std::vector<Date>& paymentDates,
    Real bondNotional,
    const ext::shared_ptr<Index>& index,
    const ext::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr,
    Real initialPrice = Null<Real>(),
    bool applyFXIndexFixingDays = false);

#endif
