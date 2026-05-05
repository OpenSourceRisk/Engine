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
%include qle_indexes.i

namespace QuantExt {
    class FutureExpiryCalculator;
    class EquityCouponPricer;
    class CorrelationTermStructure;
} // namespace QuantExt

%{
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/equitycouponpricer.hpp>
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

// QuantExt::EquityLeg builder using helper-function-with-kwargs pattern
%{
Leg _EquityLeg(
    const Schedule& schedule,
    const ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve,
    const std::vector<Real>& notionals,
    const DayCounter& paymentDayCounter = DayCounter(),
    const BusinessDayConvention paymentConvention = Following,
    const Calendar& paymentCalendar = Calendar(),
    Natural paymentLag = 0,
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
    Natural paymentLag = 0,
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
};
} // namespace QuantExt

#endif
